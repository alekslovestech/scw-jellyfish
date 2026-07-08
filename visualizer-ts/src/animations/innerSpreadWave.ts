import { LED } from "../core/ledSystem";
import { getLEDDescriptor } from "../ledMap";

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

export const innerSpreadWave = {
  name: "innerSpreadWave",

  update(leds: LED[], time: number) {
    for (const led of leds) {
      const { segment, jellyId } = getLEDDescriptor(led.id);
      const t = led.t ?? 0;

      const pos = pathPos(segment, t);
      const jellyOffset = jellyId * JELLY_PHASE;

      // Traveling wave: crest moves from inner tip (pos=0) toward outer tip (pos=1).
      const wave = Math.max(0, Math.sin(pos * WORM_FREQ - time * SPEED + jellyOffset));

      if (segment === "inner") {
        // Fuchsia worm on inner tentacles; dark between pulses.
        led.color.setRGB(wave, 0, wave);
        led.intensity = wave;
      } else {
        // Bell and outer are yellow at rest; wave turns them fuchsia as it passes through.
        led.color.setRGB(1, 1 - wave, wave);
        led.intensity = 1;
      }
    }
  },
};
