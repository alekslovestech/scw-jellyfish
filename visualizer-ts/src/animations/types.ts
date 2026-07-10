import { LED } from "../core/ledSystem";

export type LEDAnimation = {
  name: string;
  update: (leds: LED[], time: number) => void;
};

// Describes one tunable parameter of an effect so the panel can render a control
// for it. `key` is the property name inside the effect's `params` object.
//   - numeric slider: provide min/max/step
//   - dropdown: provide `options` (a list of string values); the bound param is
//     then a string.
export type EffectControl = {
  key: string;
  label: string;
  min?: number;
  max?: number;
  step?: number;
  options?: string[];
};

// An effect layers on top of the base animation. The AnimationManager runs the
// base first, then each enabled effect in order. An effect may modulate what the
// base produced (read/scale intensity), drive shared fxState the base reads, or
// draw its own pixels over the top (overlay).
//
// `params` holds live-tunable numeric values; `controls` tells the panel how to
// bind sliders to them.
export type Effect = {
  name: string;
  enabled: boolean;
  params?: Record<string, number | string>;
  controls?: EffectControl[];
  apply: (leds: LED[], time: number) => void;
  // Optional cleanup when the effect is switched off, e.g. to reset any shared
  // state it was driving so it doesn't stick.
  onDisable?: () => void;
};
