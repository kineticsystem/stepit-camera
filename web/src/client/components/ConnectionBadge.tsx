import { useStatus } from '../ros/connection';

const LABELS = { connected: 'Connected', connecting: 'Connecting…', disconnected: 'Disconnected' };

/** Whether the page reaches a rosbridge, e.g. the camera's. */
export function ConnectionBadge({ url }: { url: string }) {
  const status = useStatus(url);
  return (
    <span className={`badge badge-${status}`} title={`rosbridge at ${url}`}>
      <span className="dot" />
      {LABELS[status]}
    </span>
  );
}
