// Small line icons, drawn with the text color.

const common = {
  width: 16, height: 16, viewBox: '0 0 16 16', fill: 'none', stroke: 'currentColor',
  strokeWidth: 1.5, strokeLinecap: 'round', strokeLinejoin: 'round', className: 'icon', 'aria-hidden': true,
} as const;

export function CameraIcon() {
  return (
    <svg {...common}>
      <path d="M2 5.5a1 1 0 0 1 1-1h2l1-1.5h4l1 1.5h2a1 1 0 0 1 1 1V12a1 1 0 0 1-1 1H3a1 1 0 0 1-1-1z" />
      <circle cx="8" cy="8.5" r="2.5" />
    </svg>
  );
}

export function GearIcon() {
  return (
    <svg {...common}>
      <circle cx="8" cy="8" r="2" />
      <path d="M8 1.5v2M8 12.5v2M1.5 8h2M12.5 8h2M3.4 3.4l1.4 1.4M11.2 11.2l1.4 1.4M3.4 12.6l1.4-1.4M11.2 4.8l1.4-1.4" />
    </svg>
  );
}

export function PlayIcon() {
  return (
    <svg {...common}>
      <path d="M5 3.5v9l7-4.5z" />
    </svg>
  );
}

export function StopIcon() {
  return (
    <svg {...common}>
      <rect x="4" y="4" width="8" height="8" rx="1" />
    </svg>
  );
}

export function ShutterIcon() {
  return (
    <svg {...common}>
      <circle cx="8" cy="8" r="6" />
      <circle cx="8" cy="8" r="2.5" />
    </svg>
  );
}
