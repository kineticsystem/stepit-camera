// A client of rosbridge: a WebSocket that speaks JSON, so the browser needs no
// ROS. It calls services and subscribes to topics, and reconnects on its own
// when the connection drops, e.g. while the robot restarts.
//
//   → call_service       a request                ← service_response
//   → subscribe          a topic, once per topic  ← publish, for each message
//   → unsubscribe
//                                                ← fragment, parts of a large message
//
// rosbridge splits a message larger than its fragment size, 10 MB by default,
// e.g. a picture, into fragments of its JSON text, which come in order. It
// encodes a uint8[] field as a base64 string: see decodeBytes().

export type Status = 'connecting' | 'connected' | 'disconnected';

export class RosbridgeError extends Error {}

/**
 * The QoS of a subscription, as rosbridge takes it. Without one, rosbridge
 * subscribes best effort, unless the publisher is transient local.
 */
export interface Qos {
  reliability?: 'reliable' | 'best_effort';
  durability?: 'volatile' | 'transient_local';
  history?: 'keep_last' | 'keep_all';
  depth?: number;
}

interface PendingCall {
  resolve(values: unknown): void;
  reject(error: Error): void;
  timer: ReturnType<typeof setTimeout>;
}

interface Subscription {
  type: string;
  qos?: Qos;
  id: string;
  listeners: Set<(message: unknown) => void>;
}

export interface RosbridgeOptions {
  /** How long to wait before connecting again, in milliseconds. */
  reconnectDelay?: number;
  /** How long a service may take to answer, in milliseconds. */
  callTimeout?: number;
}

let nextId = 0;
const uniqueId = (what: string) => `stepit-ui-${what}-${Date.now()}-${nextId++}`;

export class Rosbridge {
  readonly url: string;
  private socket?: WebSocket;
  private status: Status = 'connecting';
  private closed = false;
  private reconnectTimer?: ReturnType<typeof setTimeout>;
  private readonly pending = new Map<string, PendingCall>();
  private readonly subscriptions = new Map<string, Subscription>();
  /** The parts received so far of each fragmented message, by id. */
  private readonly fragments = new Map<string, string[]>();
  private readonly statusListeners = new Set<(status: Status) => void>();
  private readonly reconnectDelay: number;
  private readonly callTimeout: number;

  constructor(url: string, options: RosbridgeOptions = {}) {
    this.url = url;
    this.reconnectDelay = options.reconnectDelay ?? 2000;
    this.callTimeout = options.callTimeout ?? 10000;
    this.open();
  }

  getStatus(): Status {
    return this.status;
  }

  /** Calls the listener on every change of the status. Returns a function that stops it. */
  onStatus(listener: (status: Status) => void): () => void {
    this.statusListeners.add(listener);
    return () => this.statusListeners.delete(listener);
  }

  /**
   * Calls a service and returns its response. Fails at once when not
   * connected, rather than waiting: a button should say it did not work.
   */
  callService<Response>(service: string, type: string, args: object = {}, timeout = this.callTimeout): Promise<Response> {
    return new Promise<Response>((resolve, reject) => {
      if (this.status !== 'connected' || !this.socket) {
        reject(new RosbridgeError(`Not connected to rosbridge at ${this.url}`));
        return;
      }
      const id = uniqueId('call');
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new RosbridgeError(`${service} did not answer in time`));
      }, timeout);
      this.pending.set(id, { resolve: (values) => resolve(values as Response), reject, timer });
      this.send({ op: 'call_service', id, service, type, args });
    });
  }

  /**
   * Calls the listener with every message of a topic. Returns a function that
   * stops it. The subscription survives a reconnection.
   *
   * rosbridge sets the QoS of a topic when its first client subscribes, and
   * shares the subscription with the clients that come after.
   */
  subscribe<Message>(topic: string, type: string, listener: (message: Message) => void, qos?: Qos): () => void {
    let subscription = this.subscriptions.get(topic);
    if (!subscription) {
      subscription = { type, qos, id: uniqueId('subscribe'), listeners: new Set() };
      this.subscriptions.set(topic, subscription);
      if (this.status === 'connected') this.sendSubscribe(topic, subscription);
    }
    const untyped = listener as (message: unknown) => void;
    subscription.listeners.add(untyped);
    return () => {
      const current = this.subscriptions.get(topic);
      if (!current || !current.listeners.delete(untyped) || current.listeners.size > 0) return;
      this.subscriptions.delete(topic);
      if (this.status === 'connected') this.send({ op: 'unsubscribe', id: current.id, topic });
    };
  }

  /** Closes the connection for good. */
  close(): void {
    this.closed = true;
    clearTimeout(this.reconnectTimer);
    this.socket?.close();
    this.failPending('The connection to rosbridge was closed');
    this.setStatus('disconnected');
  }

  private open() {
    this.setStatus('connecting');
    let socket: WebSocket;
    try {
      socket = new WebSocket(this.url);
    } catch {
      // An invalid URL: trying again will not help.
      this.setStatus('disconnected');
      return;
    }
    this.socket = socket;
    socket.onopen = () => {
      this.setStatus('connected');
      for (const [topic, subscription] of this.subscriptions) this.sendSubscribe(topic, subscription);
    };
    socket.onclose = () => {
      if (this.socket !== socket) return;
      this.socket = undefined;
      this.failPending(`The connection to rosbridge at ${this.url} was lost`);
      this.setStatus('disconnected');
      if (!this.closed) this.reconnectTimer = setTimeout(() => this.open(), this.reconnectDelay);
    };
    socket.onmessage = (event) => this.receive(String(event.data));
  }

  private receive(data: string) {
    let message: {
      op?: string; id?: string; topic?: string; msg?: unknown; values?: unknown; result?: boolean;
      data?: string; num?: number; total?: number;
    };
    try {
      message = JSON.parse(data);
    } catch {
      return;
    }
    if (message.op === 'fragment' && message.id !== undefined) {
      const parts = this.fragments.get(message.id) ?? [];
      this.fragments.set(message.id, parts);
      parts[message.num ?? 0] = message.data ?? '';
      if (parts.filter((part) => part !== undefined).length === message.total) {
        this.fragments.delete(message.id);
        this.receive(parts.join(''));
      }
    } else if (message.op === 'publish' && message.topic) {
      for (const listener of this.subscriptions.get(message.topic)?.listeners ?? []) listener(message.msg);
    } else if (message.op === 'service_response' && message.id) {
      const call = this.pending.get(message.id);
      if (!call) return;
      this.pending.delete(message.id);
      clearTimeout(call.timer);
      // On failure, e.g. an unknown service, rosbridge puts its error in values.
      if (message.result === false) call.reject(new RosbridgeError(String(message.values)));
      else call.resolve(message.values);
    } else if (message.op === 'status' && message.id && (message as { level?: string }).level === 'error') {
      // rosbridge reports a malformed request this way, e.g. an unknown service type.
      const call = this.pending.get(message.id);
      if (!call) return;
      this.pending.delete(message.id);
      clearTimeout(call.timer);
      call.reject(new RosbridgeError(String((message as { msg?: string }).msg ?? 'rosbridge refused the request')));
    }
  }

  private sendSubscribe(topic: string, subscription: Subscription) {
    // queue_length 1: a page that falls behind skips to the latest message.
    this.send({
      op: 'subscribe', id: subscription.id, topic, type: subscription.type, queue_length: 1,
      ...(subscription.qos && { qos: subscription.qos }),
    });
  }

  private send(message: object) {
    this.socket?.send(JSON.stringify(message));
  }

  private failPending(reason: string) {
    for (const call of this.pending.values()) {
      clearTimeout(call.timer);
      call.reject(new RosbridgeError(reason));
    }
    this.pending.clear();
    this.fragments.clear();
  }

  private setStatus(status: Status) {
    if (status === this.status) return;
    this.status = status;
    for (const listener of this.statusListeners) listener(status);
  }
}

/** The bytes of a uint8[] field, which rosbridge sends as base64. */
export function decodeBytes(data: string | number[]): Uint8Array {
  if (Array.isArray(data)) return Uint8Array.from(data);
  const binary = atob(data);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
  return bytes;
}

/** The message of an error of any kind. */
export function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}
