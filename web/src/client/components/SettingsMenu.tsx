import { useEffect, useRef, useState } from 'react';
import { cameraRosbridgeUrl, cameraVideoUrl, useSettings, type Theme } from '../settings';
import { GearIcon } from './icons';

/** The preferences of this browser: the theme, and where the camera's servers are. */
export function SettingsMenu() {
  const [open, setOpen] = useState(false);
  const ref = useRef<HTMLDivElement>(null);
  const settings = useSettings();

  useEffect(() => {
    if (!open) return;
    const close = (e: MouseEvent) => {
      if (!ref.current?.contains(e.target as Node)) setOpen(false);
    };
    const escape = (e: KeyboardEvent) => e.key === 'Escape' && setOpen(false);
    document.addEventListener('mousedown', close);
    document.addEventListener('keydown', escape);
    return () => {
      document.removeEventListener('mousedown', close);
      document.removeEventListener('keydown', escape);
    };
  }, [open]);

  return (
    <div className="settings" ref={ref}>
      <button className={`icon-button${open ? ' active' : ''}`} title="Settings" onClick={() => setOpen(!open)}>
        <GearIcon />
      </button>
      {open && (
        <div className="settings-popover">
          <h2>Settings</h2>
          <label className="setting">
            <span className="setting-label">Theme</span>
            <select value={settings.theme} onChange={(e) => settings.update({ theme: e.target.value as Theme })}>
              <option value="auto">Auto</option>
              <option value="light">Light</option>
              <option value="dark">Dark</option>
            </select>
          </label>
          <h2 className="settings-group">Camera</h2>
          <UrlField
            label="rosbridge"
            value={settings.cameraRosbridgeUrl}
            placeholder={cameraRosbridgeUrl({ cameraRosbridgeUrl: '' })}
            onChange={(cameraRosbridgeUrl) => settings.update({ cameraRosbridgeUrl })}
          />
          <UrlField
            label="Video server"
            value={settings.cameraVideoUrl}
            placeholder={cameraVideoUrl({ cameraVideoUrl: '' })}
            onChange={(cameraVideoUrl) => settings.update({ cameraVideoUrl })}
          />
          <UrlField
            label="Camera node"
            value={settings.cameraNode}
            placeholder="/camera"
            onChange={(cameraNode) => settings.update({ cameraNode: cameraNode.trim() || '/camera' })}
          />
        </div>
      )}
    </div>
  );
}

/** A text field that applies its value when it loses the focus or on Enter, not on every key. */
function UrlField(props: { label: string; value: string; placeholder: string; onChange(value: string): void }) {
  const [text, setText] = useState(props.value);
  useEffect(() => setText(props.value), [props.value]);
  const apply = () => text !== props.value && props.onChange(text);
  return (
    <label className="setting-field">
      <span className="setting-label">{props.label}</span>
      <input
        className="mono"
        value={text}
        placeholder={props.placeholder}
        spellCheck={false}
        onChange={(e) => setText(e.target.value)}
        onBlur={apply}
        onKeyDown={(e) => e.key === 'Enter' && apply()}
      />
    </label>
  );
}
