import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { decodeBytes, Rosbridge, type Status } from '../src/client/ros/rosbridge';
import { FakeSocket } from './fakeSocket';

describe('the rosbridge client', () => {
  beforeEach(() => {
    FakeSocket.all = [];
    vi.useFakeTimers();
    vi.stubGlobal('WebSocket', FakeSocket);
  });
  afterEach(() => {
    vi.unstubAllGlobals();
    vi.useRealTimers();
  });

  it('calls a service and returns its response', async () => {
    const ros = new Rosbridge('ws://robot:9090');
    FakeSocket.last.open();
    const response = ros.callService('/camera/take_picture', 'std_srvs/srv/Trigger');
    expect(FakeSocket.last.lastSent('call_service')).toMatchObject({
      service: '/camera/take_picture', type: 'std_srvs/srv/Trigger', args: {},
    });
    FakeSocket.last.respond({ success: true, message: 'Shutter released' });
    await expect(response).resolves.toEqual({ success: true, message: 'Shutter released' });
  });

  it('fails a call that rosbridge could not make, or that is not answered', async () => {
    const ros = new Rosbridge('ws://robot:9090', { callTimeout: 1000 });
    FakeSocket.last.open();
    const unknown = ros.callService('/nobody', 'std_srvs/srv/Trigger');
    FakeSocket.last.respond('Service /nobody does not exist', false);
    await expect(unknown).rejects.toThrow('Service /nobody does not exist');

    const slow = ros.callService('/camera/get_settings', 'stepit_camera_msgs/srv/GetSettings');
    vi.advanceTimersByTime(1000);
    await expect(slow).rejects.toThrow('did not answer in time');
  });

  it('fails a call at once when not connected, and the calls in flight when the connection drops', async () => {
    const ros = new Rosbridge('ws://robot:9090');
    await expect(ros.callService('/a', 'std_srvs/srv/Trigger')).rejects.toThrow('Not connected');

    FakeSocket.last.open();
    const call = ros.callService('/a', 'std_srvs/srv/Trigger');
    FakeSocket.last.close();
    await expect(call).rejects.toThrow('was lost');
  });

  it('subscribes once per topic, and unsubscribes with the last listener', () => {
    const ros = new Rosbridge('ws://robot:9090');
    FakeSocket.last.open();
    const first = vi.fn();
    const second = vi.fn();
    const stopFirst = ros.subscribe('/camera/picture', 'stepit_camera_msgs/msg/Picture', first);
    const stopSecond = ros.subscribe('/camera/picture', 'stepit_camera_msgs/msg/Picture', second);
    const socket = FakeSocket.last;
    expect(socket.sent.filter((m) => m.op === 'subscribe')).toHaveLength(1);

    socket.receive({ op: 'publish', topic: '/camera/picture', msg: { name: 'IMG_0001.JPG' } });
    socket.receive({ op: 'publish', topic: '/other', msg: {} });
    expect(first).toHaveBeenCalledExactlyOnceWith({ name: 'IMG_0001.JPG' });
    expect(second).toHaveBeenCalledOnce();

    expect(socket.lastSent('subscribe').qos).toBeUndefined();

    stopFirst();
    expect(socket.sent.filter((m) => m.op === 'unsubscribe')).toHaveLength(0);
    stopSecond();
    expect(socket.lastSent('unsubscribe')).toMatchObject({ topic: '/camera/picture', id: socket.lastSent('subscribe').id });
  });

  it('puts a large message back together from its fragments', () => {
    const ros = new Rosbridge('ws://robot:9090');
    FakeSocket.last.open();
    const listener = vi.fn();
    ros.subscribe('/camera/picture', 'stepit_camera_msgs/msg/Picture', listener);
    const text = JSON.stringify({ op: 'publish', topic: '/camera/picture', msg: { name: 'IMG_0001.CR2', data: 'AAEC' } });
    const parts = [text.slice(0, 20), text.slice(20, 40), text.slice(40)];
    FakeSocket.last.receive({ op: 'fragment', id: 'big', data: parts[0], num: 0, total: 3 });
    FakeSocket.last.receive({ op: 'fragment', id: 'big', data: parts[1], num: 1, total: 3 });
    expect(listener).not.toHaveBeenCalled();
    FakeSocket.last.receive({ op: 'fragment', id: 'big', data: parts[2], num: 2, total: 3 });
    expect(listener).toHaveBeenCalledExactlyOnceWith({ name: 'IMG_0001.CR2', data: 'AAEC' });
  });

  it('reconnects on its own, and subscribes again', () => {
    const statuses: Status[] = [];
    const ros = new Rosbridge('ws://robot:9090', { reconnectDelay: 500 });
    ros.onStatus((s) => statuses.push(s));
    FakeSocket.last.open();
    ros.subscribe('/camera/picture', 'stepit_camera_msgs/msg/Picture', () => {});

    FakeSocket.last.close();
    expect(ros.getStatus()).toBe('disconnected');
    vi.advanceTimersByTime(500);
    expect(FakeSocket.all).toHaveLength(2);
    FakeSocket.last.open();
    expect(FakeSocket.last.lastSent('subscribe')).toMatchObject({ topic: '/camera/picture' });
    expect(statuses).toEqual(['connected', 'disconnected', 'connecting', 'connected']);
  });

  it('stays closed once closed', () => {
    const ros = new Rosbridge('ws://robot:9090', { reconnectDelay: 500 });
    FakeSocket.last.open();
    ros.close();
    vi.advanceTimersByTime(5000);
    expect(FakeSocket.all).toHaveLength(1);
    expect(ros.getStatus()).toBe('disconnected');
  });

  it('decodes the bytes of a uint8[] field', () => {
    expect(decodeBytes(btoa('\xff\xd8\x00'))).toEqual(new Uint8Array([0xff, 0xd8, 0]));
    expect(decodeBytes([1, 2])).toEqual(new Uint8Array([1, 2]));
  });
});
