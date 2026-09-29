import { useStatus } from '../ros/connection';

const LABELS = { connected: 'Connected', connecting: 'Connecting…', disconnected: 'Disconnected' };

/** Whether the UI reaches a module of the robot, through its rosbridge. */
export function ConnectionBadge({ url }: { url: string }) {
  const status = useStatus(url);
  return (
    <span className={`badge badge-${status}`} title={`rosbridge at ${url}`}>
      <span className="dot" />
      {LABELS[status]}
    </span>
  );
}
