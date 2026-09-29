// The state of the camera section: the settings read from the driver, the
// live view, and the test shot.

import { create } from 'zustand';
import { disconnect, onConnected, rosbridge, statusOf } from '../ros/connection';
import { errorMessage } from '../ros/rosbridge';
import { cameraRosbridgeUrl, useSettings } from '../settings';
import { Camera, type CameraSetting, type Picture } from './camera';
import { rawPreview } from './raw';

/** How often the settings are read again, to follow changes made on the camera itself. */
const REFRESH_PERIOD = 3000;
/** How long a shot may take to reach the driver, after the shutter was released. */
const PICTURE_TIMEOUT = 60000;
/** How long to wait for the other file of the same shot, e.g. the JPEG of RAW+JPEG. */
const SECOND_FILE_WAIT = 2000;

export interface Shot {
  state: 'taking' | 'waiting' | 'done' | 'failed';
  message: string;
  pictures: ShotPicture[];
}

export interface ShotPicture {
  name: string;
  path: string;
  size: number;
  /** An object URL of the picture, if the browser can show it: a JPEG, or the preview inside a RAW. */
  url?: string;
  /** The URL shows the preview inside a RAW, not the picture itself. */
  preview?: boolean;
}

interface CameraState {
  settings: CameraSetting[];
  /** Why the settings cannot be read, e.g. the camera is off. */
  error?: string;
  /** The settings being changed, with the value asked for. */
  changing: Record<string, string>;
  /** Why the last change failed, by setting. */
  refused: Record<string, string>;
  streaming: boolean;
  streamError?: string;
  shot?: Shot;

  refresh(): Promise<void>;
  change(name: string, value: string): Promise<void>;
  setStreaming(on: boolean): Promise<void>;
  takeShot(): Promise<void>;
}

const camera = () => new Camera(rosbridge(cameraRosbridgeUrl(useSettings.getState())), useSettings.getState().cameraNode);
const delay = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

export const useCamera = create<CameraState>((set, get) => ({
  settings: [],
  changing: {},
  refused: {},
  streaming: false,

  async refresh() {
    try {
      set({ settings: await camera().getSettings(), error: undefined });
    } catch (e) {
      set({ error: errorMessage(e) });
    }
  },

  async change(name, value) {
    set((s) => ({ changing: { ...s.changing, [name]: value }, refused: without(s.refused, name) }));
    try {
      await camera().setSetting(name, value);
    } catch (e) {
      set((s) => ({ refused: { ...s.refused, [name]: errorMessage(e) } }));
    } finally {
      set((s) => ({ changing: without(s.changing, name) }));
    }
    await get().refresh();
  },

  async setStreaming(on) {
    useSettings.getState().update({ liveView: on });
    set({ streaming: on, streamError: undefined });
    try {
      await (on ? camera().startStreaming() : camera().stopStreaming());
    } catch (e) {
      set({ streamError: errorMessage(e) });
    }
  },

  async takeShot() {
    const previous = get().shot;
    previous?.pictures.forEach((p) => p.url && URL.revokeObjectURL(p.url));
    const pictures: ShotPicture[] = [];
    const update = (change: Partial<Shot>) => set((s) => ({ shot: { ...s.shot!, ...change, pictures: [...pictures] } }));
    set({ shot: { state: 'taking', message: 'Releasing the shutter…', pictures } });

    const cam = camera();
    let arrived: () => void = () => {};
    const first = new Promise<void>((resolve) => (arrived = resolve));
    // Subscribe before releasing the shutter, so that the picture cannot come first.
    const stop = cam.onPicture((picture) => {
      pictures.push(shotPicture(picture));
      update({});
      arrived();
    });
    try {
      await cam.takePicture();
      update({ state: 'waiting', message: 'Downloading the picture…' });
      const got = await Promise.race([first.then(() => true), delay(PICTURE_TIMEOUT).then(() => false)]);
      if (!got) {
        update({
          state: 'failed',
          message: 'The shutter was released, but the picture did not reach the page. The driver may still have saved it, in its pictures folder.',
        });
        return;
      }
      await delay(SECOND_FILE_WAIT);
      update({ state: 'done', message: '' });
    } catch (e) {
      update({ state: 'failed', message: errorMessage(e) });
    } finally {
      stop();
    }
  },
}));

function shotPicture(picture: Picture): ShotPicture {
  const shot: ShotPicture = { name: picture.name, path: picture.path, size: picture.bytes.length };
  const jpeg = /\.jpe?g$/i.test(picture.name) ? picture.bytes : rawPreview(picture.bytes);
  if (jpeg) {
    shot.url = URL.createObjectURL(new Blob([jpeg as BlobPart], { type: 'image/jpeg' }));
    shot.preview = jpeg !== picture.bytes;
  }
  return shot;
}

function without<T>(record: Record<string, T>, key: string): Record<string, T> {
  const { [key]: _, ...rest } = record;
  return rest;
}

/**
 * Keeps the camera section up to date while it is shown: reads the settings
 * now and then, and starts the live view again whenever rosbridge
 * reconnects, if it was on. Returns a function that stops it.
 */
export function followCamera(): () => void {
  const url = () => cameraRosbridgeUrl(useSettings.getState());
  const connected = () => {
    void useCamera.getState().refresh();
    if (useSettings.getState().liveView) void useCamera.getState().setStreaming(true);
  };
  let stopFollowing = follow(url());
  function follow(to: string) {
    rosbridge(to);
    if (statusOf(to) === 'connected') connected();
    return onConnected(to, connected);
  }
  // Another rosbridge in the settings: leave the old one, and follow the new one.
  const unsubscribeSettings = useSettings.subscribe((settings, previous) => {
    const [to, from] = [cameraRosbridgeUrl(settings), cameraRosbridgeUrl(previous)];
    if (to === from) return;
    stopFollowing();
    disconnect(from);
    useCamera.setState({ settings: [], error: undefined });
    stopFollowing = follow(to);
  });
  const timer = setInterval(() => {
    const { changing, shot } = useCamera.getState();
    // During a shot, the driver is busy downloading the picture, e.g. 29 MB
    // for a RAW: a request would only time out.
    const shooting = shot?.state === 'taking' || shot?.state === 'waiting';
    if (statusOf(url()) === 'connected' && Object.keys(changing).length === 0 && !shooting) {
      void useCamera.getState().refresh();
    }
  }, REFRESH_PERIOD);
  return () => {
    stopFollowing();
    unsubscribeSettings();
    clearInterval(timer);
  };
}
