import { useCamera } from '../camera/store';
import { choicesOf, formatValue, settingLabel, sortSettings } from '../camera/format';

/**
 * The settings of the camera, each a list of the values it accepts right now.
 * The choices depend on the mode dial and on the lens: a setting the camera
 * does not let us change has a single choice, and is disabled.
 */
export function CameraSettings() {
  const { settings, error, changing, refused, change } = useCamera();

  return (
    <section className="card">
      <h2>Settings</h2>
      {error && <p className="message error">{error}</p>}
      {!error && settings.length === 0 && <p className="muted">Reading the settings…</p>}
      <div className="fields">
        {sortSettings(settings).map((setting) => {
          const pending = changing[setting.name];
          const choices = choicesOf(setting);
          return (
            <label key={setting.name} className="field">
              <span className="field-label">{settingLabel(setting.name)}</span>
              <select
                value={pending ?? setting.value}
                disabled={pending !== undefined || choices.length < 2}
                onChange={(e) => void change(setting.name, e.target.value)}
              >
                {choices.map((choice) => (
                  <option key={choice} value={choice}>{formatValue(setting.name, choice)}</option>
                ))}
              </select>
              {refused[setting.name] && <span className="message error small">{refused[setting.name]}</span>}
            </label>
          );
        })}
      </div>
    </section>
  );
}
