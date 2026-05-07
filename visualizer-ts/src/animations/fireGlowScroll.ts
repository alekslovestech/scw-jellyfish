import { LED } from "../core/ledSystem";
import { LEDAnimation } from "./types";

type GradientStop = { pos: number; r: number; g: number; b: number };

// ── Gradients (same starting point as fireGlow.ts — edit independently) ───────

const BELL_OUTER: GradientStop[] = [
  { pos: 0.00, r: 1, g: 1,   b: 1 }, // white
  { pos: 0.20, r: 1, g: 1,   b: 0 }, // yellow
  { pos: 0.40, r: 1, g: 0.2, b: 0 }, // orange
  { pos: 0.80, r: 1, g: 0,   b: 0 }, // red
];

const BELL_FRACTION = 0.5;

const INNER: GradientStop[] = [
  { pos: 0.00, r: 1, g: 1,   b: 1 }, // white
  { pos: 0.20, r: 1, g: 1,   b: 0 }, // yellow
  { pos: 0.50, r: 1, g: 0.2, b: 0 }, // orange
  { pos: 0.90, r: 1, g: 0,   b: 0 }, // red
];

// ── Tuning ────────────────────────────────────────────────────────────────────

const WOBBLE_AMP   = 0.1 // how far color bands shift (gradient units 0–1)
const WOBBLE_FREQ  = 5.0;  // spatial ripple frequency along the tentacle
const WAVE_SPEED   = 10.5;  // how fast the wave travels toward the bell

const FLICKER_AMP   = 0.9; // intensity swing (0 = no flicker)
const FLICKER_FREQ  = 3.0;  // base flicker frequency in Hz
const FLICKER_PHASE = 0.53; // phase spread per LED so each one flickers differently

// ── Gradient helper ───────────────────────────────────────────────────────────

function sample(stops: GradientStop[], pos: number): [number, number, number] {
  const p = Math.max(0, Math.min(1, pos));
  if (p <= stops[0].pos) return [stops[0].r, stops[0].g, stops[0].b];
  const last = stops[stops.length - 1];
  if (p >= last.pos) return [last.r, last.g, last.b];
  for (let i = 1; i < stops.length; i++) {
    if (p <= stops[i].pos) {
      const a = stops[i - 1], b = stops[i];
      const blend = (p - a.pos) / (b.pos - a.pos);
      return [a.r + (b.r - a.r) * blend, a.g + (b.g - a.g) * blend, a.b + (b.b - a.b) * blend];
    }
  }
  return [last.r, last.g, last.b];
}

// ── Animation ─────────────────────────────────────────────────────────────────

export const fireGlowScroll: LEDAnimation = {
  name: "fireGlowScroll",

  update(leds: LED[], time: number) {
    for (const led of leds) {
      // led.group and led.t are set by Jellyfish and avoid a descriptor lookup
      const group = led.group;
      const t = led.t ?? 0;

      let gradPos: number;
      let gradient: GradientStop[];

      if (group === "bell") {
        gradPos = t * BELL_FRACTION;
        gradient = BELL_OUTER;
      } else if (group === "outer") {
        gradPos = BELL_FRACTION + t * (1 - BELL_FRACTION);
        gradient = BELL_OUTER;
      } else {
        // inner: t=1 root (near bell) → pos 0 (hot); t=0 tip → pos 1 (cool)
        gradPos = 1 - t;
        gradient = INNER;
      }

      // Traveling wave in gradient space — color bands ripple toward the bell
      const wave = WOBBLE_AMP * Math.sin(gradPos * WOBBLE_FREQ - time * WAVE_SPEED);

      const [r, g, b] = sample(gradient, gradPos + wave);

      // Per-LED brightness flicker at slightly different phase per LED
      const flicker = 1 - FLICKER_AMP * (0.5 + 0.5 * Math.sin(time * FLICKER_FREQ + led.id * FLICKER_PHASE));

      led.color.setRGB(r, g, b);
      led.intensity = flicker;
    }
  },
};
