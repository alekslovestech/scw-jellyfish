import { Effect } from "../types";
import { cfg } from "../../config";
import { CYCLE_NAMES, cycleColor, setRGB, RGB } from "./cyclePalette";

// --- FIRE COLOUR CYCLE (fireSteady, 3 colours) ---
// fireSteady's gradient has three colours (base → mid → tips) over a fixed white
// hot core, so it cycles differently from the 2-colour bases.
//
// The default "flames" cycle cross-fades the WHOLE fire palette between coherent
// fire looks inspired by fireSpread — red fire → green fire → blue fire → (loop)
// back to red — so at any moment the flame reads as one hue but the whole thing
// slowly travels the full circle.
//
// The other cycles (rainbow/ocean/…) fall back to laying three consecutive cycle
// colours across the gradient. While ON this overrides the manual Fire Colors
// pickers; turn it off to hand-set fire colours.

type FireSet = { c1: RGB; c2: RGB; c3: RGB };

// Full fire palettes (base → mid → tips), matching fireSpread's red/green/blue
// flames. Cyclic: red → green → blue → red.
const FLAMES: FireSet[] = [
  { c1: { r: 1, g: 1, b: 0 },     c2: { r: 1, g: 0.2, b: 0 }, c3: { r: 1, g: 0, b: 0 } },     // red fire
  { c1: { r: 0.6, g: 1, b: 0 },   c2: { r: 0.1, g: 0.9, b: 0 }, c3: { r: 0, g: 0.4, b: 0 } }, // green fire
  { c1: { r: 0.4, g: 0.8, b: 1 }, c2: { r: 0.1, g: 0.3, b: 1 }, c3: { r: 0.1, g: 0, b: 0.6 } }, // blue fire
];

function lerpRGB(a: RGB, b: RGB, f: number): RGB {
  return { r: a.r + (b.r - a.r) * f, g: a.g + (b.g - a.g) * f, b: a.b + (b.b - a.b) * f };
}

function wrap(i: number, n: number): number {
  return ((i % n) + n) % n;
}

const params = {
  cycle: "flames", // "flames" (red→green→blue fire), a named cycle, or "random"
  transition: 6, // seconds to fade from one flame/colour to the next
};

export const fireColorCycle: Effect = {
  name: "fireColorCycle",
  enabled: false, // off by default — toggle it on in the panel

  params,
  controls: [
    { key: "cycle", label: "cycle", options: ["flames", ...CYCLE_NAMES] },
    { key: "transition", label: "transition (s)", min: 0.5, max: 20, step: 0.5 },
  ],

  apply(_leds, time) {
    if (params.cycle === "flames") {
      // Cross-fade the whole fire palette: red → green → blue → red (full circle).
      const T = Math.max(0.1, params.transition);
      const base = Math.floor(time / T);
      const frac = time / T - base;
      const a = FLAMES[wrap(base, FLAMES.length)];
      const b = FLAMES[wrap(base + 1, FLAMES.length)];
      setRGB(cfg.firePalette.c1, lerpRGB(a.c1, b.c1, frac));
      setRGB(cfg.firePalette.c2, lerpRGB(a.c2, b.c2, frac));
      setRGB(cfg.firePalette.c3, lerpRGB(a.c3, b.c3, frac));
      return;
    }

    // Other cycles: three consecutive cycle colours span the gradient.
    setRGB(cfg.firePalette.c1, cycleColor(params.cycle, params.transition, time, 0));
    setRGB(cfg.firePalette.c2, cycleColor(params.cycle, params.transition, time, 1));
    setRGB(cfg.firePalette.c3, cycleColor(params.cycle, params.transition, time, 2));
  },
};
