import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { Camera } from '../src/client/camera/camera';
import { choicesOf, formatValue, settingLabel, sortSettings } from '../src/client/camera/format';
import { Rosbridge } from '../src/client/ros/rosbridge';
import { FakeSocket } from './fakeSocket';

describe('the camera over rosbridge', () => {
  let camera: Camera;
  let socket: FakeSocket;

  beforeEach(() => {
    FakeSocket.all = [];
    vi.stubGlobal('WebSocket', FakeSocket);
    camera = new Camera(new Rosbridge('ws://camera-pc:9090'), '/camera');
    socket = FakeSocket.last;
    socket.open();
  });
  afterEach(() => vi.unstubAllGlobals());

  it('reads the settings, or why it cannot', async () => {
    const settings = [{ name: 'iso', value: '400', choices: ['Auto', '400'] }];
    const read = camera.getSettings();
    expect(socket.lastSent('call_service')).toMatchObject({ service: '/camera/get_settings' });
    socket.respond({ success: true, message: '', settings });
    await expect(read).resolves.toEqual(settings);

    const off = camera.getSettings();
    socket.respond({ success: false, message: 'The camera is not connected', settings: [] });
    await expect(off).rejects.toThrow('The camera is not connected');
  });

  it('sets a setting as a string parameter, and reports why the camera refused it', async () => {
    const set = camera.setSetting('iso', '800');
    expect(socket.lastSent('call_service')).toMatchObject({
      service: '/camera/set_parameters',
      type: 'rcl_interfaces/srv/SetParameters',
      args: { parameters: [{ name: 'iso', value: { type: 4, string_value: '800' } }] },
    });
    socket.respond({ results: [{ successful: true, reason: '' }] });
    await expect(set).resolves.toBeUndefined();

    const refused = camera.setSetting('iso', '123');
    socket.respond({ results: [{ successful: false, reason: 'The camera does not accept iso 123' }] });
    await expect(refused).rejects.toThrow('does not accept iso 123');
  });

  it('takes a picture, and fails with the reason of the driver', async () => {
    const shot = camera.takePicture();
    expect(socket.lastSent('call_service')).toMatchObject({ service: '/camera/take_picture', type: 'std_srvs/srv/Trigger' });
    socket.respond({ success: true, message: 'Shutter released' });
    await expect(shot).resolves.toBe('Shutter released');

    const failed = camera.takePicture();
    socket.respond({ success: false, message: 'The camera is not connected' });
    await expect(failed).rejects.toThrow('The camera is not connected');
  });

  it('hears of the saved pictures, reliably, without their content', () => {
    const listener = vi.fn();
    camera.onPicture(listener);
    expect(socket.lastSent('subscribe')).toMatchObject({
      topic: '/camera/saved_picture', type: 'stepit_camera_msgs/msg/Picture', qos: { reliability: 'reliable' },
    });
    socket.receive({ op: 'publish', topic: '/camera/saved_picture', msg: { name: 'IMG_0001.JPG', path: '/p/IMG_0001_1.JPG', data: [] } });
    expect(listener).toHaveBeenCalledWith({ name: 'IMG_0001.JPG', path: '/p/IMG_0001_1.JPG' });
  });

  it('loads a saved picture from the web server, under the name it was saved with', () => {
    expect(Camera.pictureUrl('/home/developer/ws/pictures/IMG_0001_1.CR2')).toBe('/pictures/IMG_0001_1.CR2');
    expect(Camera.pictureUrl('/p/IMG 1#.JPG', 'http://camera-pc:8090')).toBe('http://camera-pc:8090/pictures/IMG%201%23.JPG');
  });

  it('streams the live view through web_video_server, without decoding it', () => {
    expect(Camera.streamUrl('http://camera-pc:8081', '/camera')).toBe(
      'http://camera-pc:8081/stream?topic=/camera/preview&type=ros_compressed');
  });
});

describe('the settings as shown', () => {
  it('are in a fixed order, the unknown ones last', () => {
    const names = sortSettings(['zoom', 'white_balance', 'aperture', 'iso', 'shutter_speed'].map((name) => ({ name, value: '', choices: [] })))
      .map((s) => s.name);
    expect(names).toEqual(['iso', 'shutter_speed', 'aperture', 'white_balance', 'zoom']);
    expect(settingLabel('white_balance')).toBe('White balance');
    expect(settingLabel('focus_mode')).toBe('Focus mode');
  });

  it('read as on the camera', () => {
    expect(formatValue('aperture', '5.6')).toBe('f/5.6');
    expect(formatValue('shutter_speed', '1/125')).toBe('1/125 s');
    expect(formatValue('shutter_speed', '2.5')).toBe('2.5″');
    expect(formatValue('shutter_speed', 'bulb')).toBe('bulb');
    expect(formatValue('exposure_compensation', '0.3')).toBe('+0.3 EV');
    expect(formatValue('exposure_compensation', '-1')).toBe('-1 EV');
    expect(formatValue('iso', 'Auto')).toBe('Auto');
  });

  it('offer the current value even when the camera does not list it', () => {
    expect(choicesOf({ name: 'white_balance', value: 'Cloudy', choices: [] })).toEqual(['Cloudy']);
    expect(choicesOf({ name: 'iso', value: '400', choices: ['100', '400'] })).toEqual(['100', '400']);
  });
});
