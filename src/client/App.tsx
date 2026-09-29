import { useState, type ReactNode } from 'react';
import { CameraPage } from './components/CameraPage';
import { ConnectionBadge } from './components/ConnectionBadge';
import { CameraIcon } from './components/icons';
import { SettingsMenu } from './components/SettingsMenu';
import { cameraRosbridgeUrl, useSettings, type Settings } from './settings';

interface Section {
  id: string;
  label: string;
  icon: ReactNode;
  /** The rosbridge of the module the section controls, whose connection the header shows. */
  rosbridge(settings: Settings): string;
  page(): ReactNode;
}

/**
 * The sections of the UI, one per part of the robot, in the order of the
 * navigation. Add one here to add a page, e.g. the macro rails.
 */
const SECTIONS: Section[] = [
  { id: 'camera', label: 'Camera', icon: <CameraIcon />, rosbridge: cameraRosbridgeUrl, page: () => <CameraPage /> },
];

const SECTION_KEY = 'stepit.section';

function storedSection(): string {
  try {
    const id = localStorage.getItem(SECTION_KEY);
    if (SECTIONS.some((s) => s.id === id)) return id!;
  } catch { /* Fall back to the first section. */ }
  return SECTIONS[0].id;
}

export function App() {
  const [section, setSection] = useState(storedSection);
  const current = SECTIONS.find((s) => s.id === section) ?? SECTIONS[0];
  const url = useSettings((s) => current.rosbridge(s));

  const open = (id: string) => {
    setSection(id);
    try {
      localStorage.setItem(SECTION_KEY, id);
    } catch { /* Only remembered until the page is reloaded. */ }
  };

  return (
    <div className="app">
      <header className="topbar">
        <span className="brand">StepIt Macro</span>
        <nav className="sections">
          {SECTIONS.map((s) => (
            <button key={s.id} className={s.id === current.id ? 'section active' : 'section'} onClick={() => open(s.id)}>
              {s.icon}
              {s.label}
            </button>
          ))}
        </nav>
        <span className="row-spacer" />
        <ConnectionBadge url={url} />
        <SettingsMenu />
      </header>
      <main className="page">{current.page()}</main>
    </div>
  );
}
