import { LED } from "../core/ledSystem";
import { LEDAnimation } from "./types";
import { cfg } from "../config";
import { getLEDDescriptor } from "../ledMap";
import { isLit } from "./movementReach";

// ── fireSteady ──────────────────────────────────────────────────────────────
// Fire colouring + flicker, laid over the shared tentacle-reach engine. The
// gradient comes from a tunable 3-colour palette (cfg.firePalette) over a fixed
// white hot core; where each tentacle currently reaches — and how the intense
// reaction extends that reach — is identical to movementSimulation (via isLit).

type GradientStop = { pos: number; r: number; g: number; b: number };

// ── Living-fire tuning ──────────────────────────────────────────────────────

const WOBBLE_AMP = 0.1;
const WOBBLE_FREQ = 5.0;
const WAVE_SPEED = 10.5;

const FLICKER_AMP = 0.9;
const FLICKER_FREQ = 3.0;
const FLICKER_PHASE = 0.53;

const BELL_FRACTION = 0.5;

// ── Helpers ─────────────────────────────────────────────────────────────────

function sample(stops: GradientStop[], pos: number): [number, number, number] {
  const p = Math.max(0, Math.min(1, pos));
  if (p <= stops[0].pos) return [stops[0].r, stops[0].g, stops[0].b];
  const last = stops[stops.length - 1];
  if (p >= last.pos) return [last.r, last.g, last.b];
  for (let i = 1; i < stops.length; i++) {
    if (p <= stops[i].pos) {
      const a = stops[i - 1], b = stops[i];
      const f = (p - a.pos) / (b.pos - a.pos);
      return [a.r + (b.r - a.r) * f, a.g + (b.g - a.g) * f, a.b + (b.b - a.b) * f];
    }
  }
  return [last.r, last.g, last.b];
}

// Build the gradient stops for the current palette. Bell+outer share a slightly
// tighter gradient than the (longer) inner tentacle.
function bellOuterStops(): GradientStop[] {
  const { c1, c2, c3 } = cfg.firePalette;
  return [
    { pos: 0.0, r: 1, g: 1, b: 1 }, // white hot core
    { pos: 0.2, r: c1.r, g: c1.g, b: c1.b },
    { pos: 0.4, r: c2.r, g: c2.g, b: c2.b },
    { pos: 0.8, r: c3.r, g: c3.g, b: c3.b },
  ];
}

function innerStops(): GradientStop[] {
  const { c1, c2, c3 } = cfg.firePalette;
  return [
    { pos: 0.0, r: 1, g: 1, b: 1 }, // white hot core
    { pos: 0.2, r: c1.r, g: c1.g, b: c1.b },
    { pos: 0.5, r: c2.r, g: c2.g, b: c2.b },
    { pos: 0.9, r: c3.r, g: c3.g, b: c3.b },
  ];
}

// ── Animation ─────────────────────────────────────────────────────────────────

export const fireSteady: LEDAnimation = {
  name: "fireSteady",

  update(leds: LED[], time: number) {
    // Rebuild gradients once per frame so palette edits apply live.
    const bellOuter = bellOuterStops();
    const inner = innerStops();

    for (const led of leds) {
      const desc = getLEDDescriptor(led.id);
      const segment = desc.segment;
      const t = led.t ?? 0;

      // Tentacle motion + intense-reaction reach come from the shared engine.
      if (!isLit(desc, time)) {
        led.color.setRGB(0, 0, 0);
        led.intensity = 0;
        continue;
      }

      // Gradient position along the arm (bell+outer share a path; inner is its own).
      let gradPos: number;
      let gradient: GradientStop[];
      if (segment === "bell") {
        gradPos = t * BELL_FRACTION;
        gradient = bellOuter;
      } else if (segment === "outer") {
        gradPos = BELL_FRACTION + t * (1 - BELL_FRACTION);
        gradient = bellOuter;
      } else {
        gradPos = 1 - t;
        gradient = inner;
      }

      // Colour: sample the gradient at a wobbling position for a flame-lick feel.
      const wave = WOBBLE_AMP * Math.sin(gradPos * WOBBLE_FREQ - time * WAVE_SPEED);
      const [r, g, b] = sample(gradient, gradPos + wave);

      // Full-strength flicker (per-LED phase) — the shimmer of a live flame.
      const flicker = 1 - FLICKER_AMP * (0.5 + 0.5 * Math.sin(time * FLICKER_FREQ + led.id * FLICKER_PHASE));

      led.color.setRGB(r, g, b);
      led.intensity = flicker;
    }
  },
};
