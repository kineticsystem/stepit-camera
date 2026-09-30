// The connections to rosbridge, one per URL, shared by the whole page. A
// rosbridge only knows the interfaces installed next to it, so each ROS2
// system the page talks to has its own, e.g. the camera's on port 9091.

import { create } from 'zustand';
import { Rosbridge, type Status } from './rosbridge';

const connections = new Map<string, Rosbridge>();

/** The status of each connection, by URL. */
const useStatuses = create<Record<string, Status>>(() => ({}));

/** The connection to the rosbridge at this URL, opened on first use. */
export function rosbridge(url: string): Rosbridge {
  let ros = connections.get(url);
  if (!ros) {
    const opened = new Rosbridge(url);
    connections.set(url, opened);
    useStatuses.setState({ [url]: opened.getStatus() });
    opened.onStatus((status) => useStatuses.setState({ [url]: status }));
    ros = opened;
  }
  return ros;
}

/** Closes the connection to this URL, e.g. once the settings point elsewhere. */
export function disconnect(url: string): void {
  connections.get(url)?.close();
  connections.delete(url);
}

export function statusOf(url: string): Status {
  return useStatuses.getState()[url] ?? 'disconnected';
}

/** The status of the connection to this URL, as a React hook. */
export function useStatus(url: string): Status {
  return useStatuses((s) => s[url] ?? 'disconnected');
}

/** Calls the listener whenever the connection to this URL is established. Returns a function that stops it. */
export function onConnected(url: string, listener: () => void): () => void {
  return useStatuses.subscribe((s, previous) => {
    if (s[url] === 'connected' && previous[url] !== 'connected') listener();
  });
}
