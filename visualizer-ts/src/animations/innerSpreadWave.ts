import { LED } from "../core/ledSystem";
import { getLEDDescriptor } from "../ledMap";
import { cfg } from "../config";
import { isLit } from "./movementReach";

// ─────────────────────────────────────────────────────────────
// INNER SPREAD WAVE SETTINGS
// ─────────────────────────────────────────────────────────────

const WORM_FREQ = 17;
// Wave spatial frequency along the full inner→bell→outer path.
// Calibrated so the inner segment sees ~1.6 full cycles (matching
// decoupleInnerTentacles). Bell and outer each see ~0.5 cycles.

const SPEED = 5;
// Wave travel speed — same as decoupleInnerTentacles.

const JELLY_PHASE = 0.8;
// Phase offset between jellyfish so they ripple rather than all pulse together.

// ─────────────────────────────────────────────────────────────
// PATH POSITION
//
// Maps each LED to a continuous 0→1 position along the path:
//   inner tip (0.0) ──► inner root (0.6) ──► bell outer rim (0.8) ──► outer tip (1.0)
//
// Weights match LED counts on a standard jelly: inner=30, bell=10, outer=10.
// ─────────────────────────────────────────────────────────────

function pathPos(segment: string, t: number): number {
  if (segment === "inner") return t * 0.6;          // t=0 tip → t=1 root
  if (segment === "bell")  return 0.6 + t * 0.2;   // t=0 inner edge → t=1 outer rim
  return 0.8 + t * 0.2;                             // t=0 root → t=1 outer tip
}

// Speed multiplier on the travelling wave (1 = the original SPEED cadence).
const params = { speed: 1 };

export const innerSpreadWave = {
  name: "innerSpreadWave",
  colorMode: "two" as const,
  // innerGlow only reads well over movementSimulation — hide it here.
  excludeEffects: ["innerGlow"],
  params,
  controls: [{ key: "speed", label: "speed", min: 0.1, max: 6, step: 0.05 }],

  update(leds: LED[], time: number) {
    // Two-colour palette (editable live from the panel):
    //   inner = the travelling "worm"/wave colour, outer = the rest colour.
    // Defaults of fuchsia (inner) + yellow (outer) reproduce the original look.
    const inner = cfg.palette.inner;
    const outer = cfg.palette.outer;

    for (const led of leds) {
      const desc = getLEDDescriptor(led.id);
      const { segment, jellyId } = desc;
      const t = led.t ?? 0;

      const pos = pathPos(segment, t);
      const jellyOffset = jellyId * JELLY_PHASE;

      // Traveling wave: crest moves from inner tip (pos=0) toward outer tip (pos=1).
      const wave = Math.max(0, Math.sin(pos * WORM_FREQ - time * SPEED * params.speed + jellyOffset));

      // Tentacle motion + intense-reaction reach come from the shared engine:
      // the worm colour is only visible where the tentacle currently reaches, so
      // a reaction extends it outward exactly like movementSimulation.
      const on = isLit(desc, time);

      if (segment === "inner") {
        // Worm colour on inner tentacles; dark between pulses.
        led.color.setRGB(inner.r * wave, inner.g * wave, inner.b * wave);
        led.intensity = on ? wave : 0;
      } else {
        // Bell and outer rest at the outer colour; the wave blends them toward
        // the worm colour as it passes through.
        led.color.setRGB(
          outer.r + (inner.r - outer.r) * wave,
          outer.g + (inner.g - outer.g) * wave,
          outer.b + (inner.b - outer.b) * wave,
        );
        led.intensity = on ? 1 : 0;
      }
    }
  },
};
