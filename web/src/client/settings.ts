// Preferences of this browser, kept in localStorage. Add new settings to
// Settings and DEFAULTS, and a control to components/SettingsMenu.tsx.

import { create } from 'zustand';

export type Theme = 'auto' | 'light' | 'dark';

export interface Settings {
  theme: Theme;
  /** The camera's rosbridge, e.g. ws://robot:9091; empty for port 9091 of the UI's host. */
  cameraRosbridgeUrl: string;
  /** The camera's web_video_server, e.g. http://robot:8081; empty for port 8081 of the UI's host. */
  cameraVideoUrl: string;
  /** The name of the camera node, e.g. /camera. */
  cameraNode: string;
  /** Whether the live view was on when the page was last used: it starts again with the page. */
  liveView: boolean;
}

const DEFAULTS: Settings = {
  theme: 'auto', cameraRosbridgeUrl: '', cameraVideoUrl: '', cameraNode: '/camera', liveView: true,
};
const KEY = 'stepit.settings';

function load(): Settings {
  try {
    return { ...DEFAULTS, ...JSON.parse(localStorage.getItem(KEY) ?? '{}') };
  } catch {
    return DEFAULTS;
  }
}

interface SettingsState extends Settings {
  update(change: Partial<Settings>): void;
}

export const useSettings = create<SettingsState>((set, get) => ({
  ...load(),
  update(change) {
    set(change);
    const { update: _, ...settings } = { ...get() };
    try {
      localStorage.setItem(KEY, JSON.stringify(settings));
    } catch { /* The settings then last until the page is reloaded. */ }
  },
}));

/** The host that served the page: the robot, usually, where rosbridge and web_video_server run too. */
const pageHost = () => (typeof location === 'undefined' ? '' : location.hostname) || 'localhost';

export function cameraRosbridgeUrl(settings: Pick<Settings, 'cameraRosbridgeUrl'>): string {
  return settings.cameraRosbridgeUrl.trim() || `ws://${pageHost()}:9091`;
}

export function cameraVideoUrl(settings: Pick<Settings, 'cameraVideoUrl'>): string {
  return (settings.cameraVideoUrl.trim() || `http://${pageHost()}:8081`).replace(/\/+$/, '');
}

/** Sets data-theme on <html>, which styles.css keys the dark palette on. */
function applyTheme() {
  const { theme } = useSettings.getState();
  const resolved = theme === 'auto' ? (systemDark.matches ? 'dark' : 'light') : theme;
  document.documentElement.dataset.theme = resolved;
}

const systemDark = window.matchMedia('(prefers-color-scheme: dark)');
applyTheme();
useSettings.subscribe(applyTheme);
// In Auto mode, follow the system when it switches, e.g. at sunset.
systemDark.addEventListener('change', applyTheme);
