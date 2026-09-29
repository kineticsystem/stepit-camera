import { useEffect } from 'react';
import { followCamera } from '../camera/store';
import { CameraSettings } from './CameraSettings';
import { LiveView } from './LiveView';
import { TestShot } from './TestShot';

/** The camera: what it sees on the left, its controls on the right. */
export function CameraPage() {
  useEffect(() => followCamera(), []);
  return (
    <div className="camera-page">
      <LiveView />
      <aside className="side">
        <CameraSettings />
        <TestShot />
      </aside>
    </div>
  );
}
