// How the settings of the camera are shown: their labels, their order, and
// their values the way a photographer reads them.

import type { CameraSetting } from './camera';

const LABELS: Record<string, string> = {
  iso: 'ISO',
  shutter_speed: 'Shutter speed',
  aperture: 'Aperture',
  white_balance: 'White balance',
  exposure_compensation: 'Exposure compensation',
};

/** The settings in the order of LABELS, then any other the driver adds, by name. */
export function sortSettings(settings: CameraSetting[]): CameraSetting[] {
  const order = Object.keys(LABELS);
  const rank = (name: string) => (order.includes(name) ? order.indexOf(name) : order.length);
  return [...settings].sort((a, b) => rank(a.name) - rank(b.name) || a.name.localeCompare(b.name));
}

export function settingLabel(name: string): string {
  return LABELS[name] ?? name.replace(/_/g, ' ').replace(/^./, (c) => c.toUpperCase());
}

/** A value as the camera shows it: f/5.6, 1/125 s, 2″, +0.3 EV. */
export function formatValue(name: string, value: string): string {
  const number = /^-?\d+(\.\d+)?$/.test(value);
  switch (name) {
    case 'aperture':
      return number ? `f/${value}` : value;
    case 'shutter_speed':
      if (/^1\/\d+$/.test(value)) return `${value} s`;
      return number ? `${value}″` : value;
    case 'exposure_compensation':
      if (!number) return value;
      return `${Number(value) > 0 ? '+' : ''}${value} EV`;
    default:
      return value;
  }
}

/** The choices to offer: the camera's, and the current value even when missing from them. */
export function choicesOf(setting: CameraSetting): string[] {
  if (!setting.value || setting.choices.includes(setting.value)) return setting.choices;
  return [setting.value, ...setting.choices];
}
