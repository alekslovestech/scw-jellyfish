import { LED } from "../core/ledSystem";
import { getLEDDescriptor } from "../ledMap";
import { LEDAnimation } from "./types";

type GradientStop = { pos: number; r: number; g: number; b: number };

// ── Section 1: Bell + Outer tentacles ────────────────────────────────────────
// Treated as one continuous run.
// pos 0.0 = bell inner edge   pos 1.0 = outer tentacle tip
// Edit pos (0–1) to control how much of the section each colour occupies.

const BELL_OUTER: GradientStop[] = [
  { pos: 0.00, r: 1, g: 1, b: 1 }, // white
  { pos: 0.20, r: 1, g: 1,   b: 0 }, // yellow
  { pos: 0.4, r: 1, g: 0.2, b: 0 }, // orange
  { pos: 0.80, r: 1, g: 0,   b: 0 }, // red
];

// Fraction of the combined gradient taken up by the bell (0–1).
// 0.5 = bell and outer tentacles each occupy half the range.
const BELL_FRACTION = 0.5;

// ── Section 2: Inner tentacles ───────────────────────────────────────────────
// pos 0.0 = root (near bell)   pos 1.0 = tip (bottom)

const INNER: GradientStop[] = [
  { pos: 0.00, r: 1, g: 1, b: 1 }, // white
  { pos: 0.20, r: 1, g: 1,   b: 0 }, // yellow
  { pos: 0.5, r: 1, g: 0.2, b: 0 }, // orange
  { pos: 0.9, r: 1, g: 0,   b: 0 }, // red
];

// ── Gradient helper ───────────────────────────────────────────────────────────

function sample(stops: GradientStop[], pos: number): [number, number, number] {
  const p = Math.max(0, Math.min(1, pos));
  if (p <= stops[0].pos) return [stops[0].r, stops[0].g, stops[0].b];
  const last = stops[stops.length - 1];
  if (p >= last.pos) return [last.r, last.g, last.b];
  for (let i = 1; i < stops.length; i++) {
    if (p <= stops[i].pos) {
      const a = stops[i - 1];
      const b = stops[i];
      const blend = (p - a.pos) / (b.pos - a.pos);
      return [
        a.r + (b.r - a.r) * blend,
        a.g + (b.g - a.g) * blend,
        a.b + (b.b - a.b) * blend,
      ];
    }
  }
  return [last.r, last.g, last.b];
}

// ── Animation ─────────────────────────────────────────────────────────────────

export const fireGlow: LEDAnimation = {
  name: "fireGlow",

  update(leds: LED[], _time: number) {
    for (const led of leds) {
      const { segment, t } = getLEDDescriptor(led.id);
      led.intensity = 1.0;

      let gradPos: number;
      let gradient: GradientStop[];

      if (segment === "bell") {
        // bell t=0 (inner edge) → t=1 (rim) maps to 0 → BELL_FRACTION
        gradPos = t * BELL_FRACTION;
        gradient = BELL_OUTER;
      } else if (segment === "outer") {
        // outer t=0 (rim) → t=1 (tip) maps to BELL_FRACTION → 1
        gradPos = BELL_FRACTION + t * (1 - BELL_FRACTION);
        gradient = BELL_OUTER;
      } else {
        // inner t=1 (root, near bell) → t=0 (tip) maps to pos 0 → 1
        gradPos = 1 - t;
        gradient = INNER;
      }

      const [r, g, b] = sample(gradient, gradPos);
      led.color.setRGB(r, g, b);
    }
  },
};
