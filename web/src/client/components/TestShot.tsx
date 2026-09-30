import { useCamera } from '../camera/store';
import { useStatus } from '../ros/connection';
import { cameraRosbridgeUrl, useSettings } from '../settings';
import { ShutterIcon } from './icons';

/** A button that releases the shutter, and the picture it took. */
export function TestShot() {
  const { shot, takeShot } = useCamera();
  const connected = useStatus(useSettings((s) => cameraRosbridgeUrl(s))) === 'connected';
  const busy = shot?.state === 'taking' || shot?.state === 'waiting';

  return (
    <section className="card">
      <h2>Test shot</h2>
      <button className="primary shutter" disabled={!connected || busy} onClick={() => void takeShot()}>
        <ShutterIcon />
        {busy ? 'Taking…' : 'Take a test shot'}
      </button>
      {shot?.message && <p className={`message${shot.state === 'failed' ? ' error' : ' muted'}`}>{shot.message}</p>}
      {shot?.pictures.map((picture) => (
        <figure key={picture.name} className="shot">
          {picture.url ? (
            <a href={picture.url} target="_blank" rel="noreferrer">
              <img src={picture.url} alt={picture.name} />
            </a>
          ) : (
            <div className="shot-placeholder">The browser cannot show this file</div>
          )}
          <figcaption>
            <strong>{picture.name}</strong> <span className="muted">{(picture.size / 1e6).toFixed(1)} MB</span>
            {picture.preview && <span className="muted"> · its JPEG preview</span>}
            {picture.path && <div className="mono small muted" title={picture.path}>{picture.path}</div>}
          </figcaption>
        </figure>
      ))}
    </section>
  );
}
