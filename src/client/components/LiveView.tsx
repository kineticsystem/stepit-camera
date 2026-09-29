import { useEffect, useState } from 'react';
import { Camera } from '../camera/camera';
import { useCamera } from '../camera/store';
import { cameraVideoUrl, useSettings } from '../settings';
import { PlayIcon, StopIcon } from './icons';

/**
 * The live view, streamed by web_video_server as MJPEG into a plain <img>.
 * The camera driver only sends frames while streaming is on.
 *
 * During a test shot the mirror goes down, and no frame comes until the
 * picture is downloaded: the last frame stays, greyed out.
 */
export function LiveView() {
  const { streaming, streamError, setStreaming, shot } = useCamera();
  const shooting = shot?.state === 'taking' || shot?.state === 'waiting';
  const node = useSettings((s) => s.cameraNode);
  const server = useSettings((s) => cameraVideoUrl(s));
  const [failed, setFailed] = useState(false);
  // Changing the key opens the stream again, e.g. after web_video_server restarted.
  const [attempt, setAttempt] = useState(0);
  const src = Camera.streamUrl(server, node);

  useEffect(() => setFailed(false), [src, streaming]);

  let overlay: string | undefined;
  if (streamError) overlay = streamError;
  else if (!streaming) overlay = 'The live view is off.';
  else if (failed) overlay = `Cannot reach web_video_server at ${server}. Is the camera driver running with web_video:=true?`;

  return (
    <section className="live-view">
      <div className="panel-header">
        <h1>Live view</h1>
        <div className="panel-actions">
          {streaming ? (
            <button onClick={() => void setStreaming(false)}>
              <StopIcon />
              Stop
            </button>
          ) : (
            <button className="primary" onClick={() => void setStreaming(true)}>
              <PlayIcon />
              Start
            </button>
          )}
        </div>
      </div>
      <div className={`viewport${shooting && streaming ? ' paused' : ''}`}>
        {streaming && !failed && (
          <img key={`${src}#${attempt}`} src={src} alt="What the camera sees" onError={() => setFailed(true)} />
        )}
        {shooting && streaming && !overlay && <div className="viewport-badge">Taking a picture: the live view is paused</div>}
        {overlay && (
          <div className="viewport-message">
            <p>{overlay}</p>
            {streaming && failed && (
              <button onClick={() => { setFailed(false); setAttempt(attempt + 1); }}>Retry</button>
            )}
          </div>
        )}
      </div>
    </section>
  );
}
