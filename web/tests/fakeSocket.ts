// A WebSocket that records what is sent, and lets a test answer as rosbridge.

export class FakeSocket {
  static OPEN = 1;
  static all: FakeSocket[] = [];
  static get last(): FakeSocket {
    return FakeSocket.all[FakeSocket.all.length - 1];
  }

  readyState = 0;
  sent: Record<string, any>[] = [];
  onopen?: () => void;
  onclose?: () => void;
  onmessage?: (e: { data: string }) => void;

  constructor(public url: string) {
    FakeSocket.all.push(this);
  }

  send(data: string) {
    this.sent.push(JSON.parse(data));
  }

  close() {
    if (this.readyState === 3) return;
    this.readyState = 3;
    this.onclose?.();
  }

  open() {
    this.readyState = FakeSocket.OPEN;
    this.onopen?.();
  }

  receive(msg: Record<string, unknown>) {
    this.onmessage?.({ data: JSON.stringify(msg) });
  }

  /** The last request sent with the given op. */
  lastSent(op: string): Record<string, any> {
    return [...this.sent].reverse().find((m) => m.op === op)!;
  }

  /** Answers the last service call. */
  respond(values: unknown, result = true) {
    const call = this.lastSent('call_service');
    this.receive({ op: 'service_response', id: call.id, service: call.service, values, result });
  }
}
