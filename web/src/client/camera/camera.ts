// The ROS2 interface of the StepIt camera driver (stepit_camera), over
// rosbridge. See the README of stepit-camera for what each service does.
//
//   <node>/get_settings      the settings, their values and their choices
//   <node>/set_parameters    change a setting, e.g. iso, as `ros2 param set`
//   <node>/start_streaming   the live view on, and off with stop_streaming
//   <node>/take_picture      release the shutter for a test shot
//   <node>/picture           each picture the camera takes, once saved: where
//                            the file is, which the web server serves

import type { Rosbridge } from '../ros/rosbridge';

export interface CameraSetting {
  /** The name of the parameter, e.g. shutter_speed. */
  name: string;
  /** The value on the camera, e.g. 1/125. */
  value: string;
  /** The values the camera accepts right now. */
  choices: string[];
}

export interface Picture {
  /** The name of the file on the camera, e.g. IMG_0042.JPG. */
  name: string;
  /** Where the driver saved it. */
  path: string;
}

interface TriggerResponse {
  success: boolean;
  message: string;
}

interface GetSettingsResponse extends TriggerResponse {
  settings: CameraSetting[];
}

interface SetParametersResponse {
  results: { successful: boolean; reason: string }[];
}

interface PictureMessage {
  name: string;
  path: string;
}

/** ParameterType of rcl_interfaces: the settings are string parameters. */
const PARAMETER_STRING = 4;

export class CameraError extends Error {}

export class Camera {
  constructor(
    private readonly ros: Rosbridge,
    /** The name of the node, e.g. /camera. */
    readonly node: string,
  ) {}

  async getSettings(): Promise<CameraSetting[]> {
    const response = await this.ros.callService<GetSettingsResponse>(
      `${this.node}/get_settings`, 'stepit_camera_msgs/srv/GetSettings');
    if (!response.success) throw new CameraError(response.message);
    return response.settings;
  }

  /** Changes a setting. Fails with the camera's reason, e.g. the values it accepts. */
  async setSetting(name: string, value: string): Promise<void> {
    const response = await this.ros.callService<SetParametersResponse>(
      `${this.node}/set_parameters`, 'rcl_interfaces/srv/SetParameters',
      { parameters: [{ name, value: { type: PARAMETER_STRING, string_value: value } }] });
    const result = response.results[0];
    if (!result?.successful) throw new CameraError(result?.reason || `The camera did not accept ${name} ${value}`);
  }

  startStreaming(): Promise<string> {
    return this.trigger('start_streaming');
  }

  stopStreaming(): Promise<string> {
    return this.trigger('stop_streaming');
  }

  /** Releases the shutter. The picture comes later, through onPicture(). */
  takePicture(): Promise<string> {
    // The camera may be busy for a moment, e.g. writing the previous shot.
    return this.trigger('take_picture', 15000);
  }

  /**
   * Calls the listener with every picture the camera takes, once the driver
   * saved it. The message has no content: load it from pictureUrl().
   */
  onPicture(listener: (picture: Picture) => void): () => void {
    // Reliable, not rosbridge's default, best effort: a lost message is a
    // lost test shot.
    return this.ros.subscribe<PictureMessage>(`${this.node}/picture`, 'stepit_camera_msgs/msg/Picture', (message) =>
      listener({ name: message.name, path: message.path }),
    { reliability: 'reliable', durability: 'volatile', history: 'keep_last', depth: 2 });
  }

  /**
   * The address of a saved picture on the camera's web server, which serves
   * the driver's download_directory under /pictures. Relative to the page by
   * default: the same server serves both.
   */
  static pictureUrl(path: string, webUrl = ''): string {
    const name = path.slice(path.lastIndexOf('/') + 1);
    return `${webUrl}/pictures/${encodeURIComponent(name)}`;
  }

  /** The address of the live view on web_video_server, for an <img>. */
  static streamUrl(videoUrl: string, node: string): string {
    // ros_compressed passes the camera's JPEG frames through, without decoding
    // and encoding them again. The topic stays as it is: web_video_server does
    // not decode %2F, and a topic name needs nothing else escaped.
    return `${videoUrl}/stream?topic=${node}/preview&type=ros_compressed`;
  }

  private async trigger(service: string, timeout?: number): Promise<string> {
    const response = await this.ros.callService<TriggerResponse>(
      `${this.node}/${service}`, 'std_srvs/srv/Trigger', {}, timeout);
    if (!response.success) throw new CameraError(response.message);
    return response.message;
  }
}
