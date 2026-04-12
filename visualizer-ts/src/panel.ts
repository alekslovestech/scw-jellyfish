import { Pane } from 'tweakpane';
import { cfg } from './config';

export function buildPanel(redraw: () => void): void {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const pane = new Pane({ title: 'Jellyfish' }) as any;

  const bell = pane.addFolder({ title: 'Bell' });
  bell.addBinding(cfg.bell, 'radius', { min: 0.5, max: 5.0, step: 0.1 }).on('change', redraw);

  const tent = pane.addFolder({ title: 'Outer tentacles' });
  tent.addBinding(cfg.tentacles, 'count',          { min: 1,   max: 32,  step: 1    }).on('change', redraw);
  tent.addBinding(cfg.tentacles, 'radius',         { min: 0.01,max: 0.3, step: 0.01 }).on('change', redraw);
  tent.addBinding(cfg.tentacles, 'length',         { min: 0.5, max: 8.0, step: 0.1  }).on('change', redraw);
  tent.addBinding(cfg.tentacles, 'wave_amplitude', { min: 0.0, max: 0.5, step: 0.01, label: 'wave amp'  }).on('change', redraw);
  tent.addBinding(cfg.tentacles, 'wave_frequency', { min: 0.0, max: 5.0, step: 0.25, label: 'wave freq' }).on('change', redraw);

  const cent = pane.addFolder({ title: 'Central tentacles' });
  cent.addBinding(cfg.central, 'count',          { min: 1,   max: 16,   step: 1    }).on('change', redraw);
  cent.addBinding(cfg.central, 'radius',         { min: 0.01,max: 0.5,  step: 0.01 }).on('change', redraw);
  cent.addBinding(cfg.central, 'length',         { min: 0.5, max: 10.0, step: 0.1  }).on('change', redraw);
  cent.addBinding(cfg.central, 'wave_amplitude', { min: 0.0, max: 0.5,  step: 0.01, label: 'wave amp'   }).on('change', redraw);
  cent.addBinding(cfg.central, 'wave_frequency', { min: 0.0, max: 5.0,  step: 0.25, label: 'wave freq'  }).on('change', redraw);
  cent.addBinding(cfg.central, 'ring_radius',    { min: 0.0, max: 2.0,  step: 0.05, label: 'ring radius'}).on('change', redraw);
}
