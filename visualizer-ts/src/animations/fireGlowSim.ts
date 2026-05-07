import { LED } from "../core/ledSystem";
import { getLEDDescriptor } from "../ledMap";
import { LEDAnimation } from "./types";

type GradientStop = { pos: number; r: number; g: number; b: number };

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

// ── Flicker ───────────────────────────────────────────────────────────────────
// Each LED flickers independently. Intensity swings between (1 - AMOUNT) and 1.
// SPEED controls how fast the flicker oscillates.

const FLICKER_AMOUNT = 1.0;
const FLICKER_SPEED  = 20.0;

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

export const fireGlowSim: LEDAnimation = {
  name: "fireGlowSim",

  update(leds: LED[], time: number) {
    for (const led of leds) {
      const { segment, t } = getLEDDescriptor(led.id);

      let gradPos: number;
      let gradient: GradientStop[];

      if (segment === "bell") {
        gradPos = t * BELL_FRACTION;
        gradient = BELL_OUTER;
      } else if (segment === "outer") {
        gradPos = BELL_FRACTION + t * (1 - BELL_FRACTION);
        gradient = BELL_OUTER;
      } else {
        gradPos = 1 - t;
        gradient = INNER;
      }

      const [r, g, b] = sample(gradient, gradPos);
      led.color.setRGB(r, g, b);

      // Two sine waves at coprime frequencies give each LED an organic flicker
      led.intensity = 1 - FLICKER_AMOUNT * (
        0.5 + 0.5 * Math.sin(time * FLICKER_SPEED + led.id * 0.4)
      ) * (
        0.5 + 0.5 * Math.sin(time * FLICKER_SPEED * 1.37 + led.id * 0.91)
      );
    }
  },
};
