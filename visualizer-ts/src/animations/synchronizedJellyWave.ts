import { LED } from "../core/ledSystem";
import { getLEDDescriptor } from "../ledMap";

// ─────────────────────────────────────────────────────────────────────────────
// SYNCHRONIZED JELLY WAVE
//
// Each jellyfish runs its own top-to-bottom cascade independently.
// The wave starts at the bell center and fans outward + downward through
// both tentacle sets simultaneously.
//
// Cascade order (0 = first lit, 1 = last lit):
//   Bell inner edge (0.0) → bell outer rim (BELL_BAND)
//   Inner root (BELL_BAND) → inner tip (1.0)   ← same moment as outer
//   Outer root (BELL_BAND) → outer tip (1.0)
// ─────────────────────────────────────────────────────────────────────────────

const SPEED       = 0.5;
const SOFTNESS    = 0.08;
const PAUSE_FULL  = 0.3;
const PAUSE_EMPTY = 0.3;

// Fraction of the cascade range occupied by the bell (top portion).
const BELL_BAND = 0.15;

const COLOR_R = 0.0;
const COLOR_G = 1.0;
const COLOR_B = 0.0;

// ─────────────────────────────────────────────────────────────────────────────

// Returns 0→1: physical position in the top-to-bottom cascade within one jelly.
// Works for both jelly 0 and standard jellies — t convention is the same.
function cascadePos(segment: string, t: number): number {
  if (segment === "bell") {
    // t=0 = inner edge (top), t=1 = outer rim (junction with tentacles)
    return t * BELL_BAND;
  }
  if (segment === "inner") {
    // t=0 = tip (bottom), t=1 = root (top, near bell)
    return BELL_BAND + (1 - t) * (1 - BELL_BAND);
  }
  // outer: t=0 = root (top, at bell rim), t=1 = tip (bottom)
  return BELL_BAND + t * (1 - BELL_BAND);
}

// ─────────────────────────────────────────────────────────────────────────────

export const synchronizedJellyWave = {
  name: "synchronizedJellyWave",

  update(leds: LED[], time: number) {
    const sweepDuration = 1.0 / SPEED;
    const cycleDuration = sweepDuration * 2 + PAUSE_FULL + PAUSE_EMPTY;
    const cycleTime = time % cycleDuration;

    let intensity_fn: (pos: number) => number;

    if (cycleTime < sweepDuration) {
      // Phase 1: wave front sweeps top → bottom (LEDs turn on)
      const waveFront = cycleTime * SPEED;
      intensity_fn = (pos) => Math.min(1, Math.max(0, (waveFront - pos) / SOFTNESS));

    } else if (cycleTime < sweepDuration + PAUSE_FULL) {
      // Phase 2: all on
      intensity_fn = () => 1.0;

    } else if (cycleTime < sweepDuration * 2 + PAUSE_FULL) {
      // Phase 3: wave retreats bottom → top (LEDs turn off)
      const elapsed   = cycleTime - sweepDuration - PAUSE_FULL;
      const waveFront = elapsed * SPEED;
      intensity_fn = (pos) => Math.min(1, Math.max(0, ((1 - pos) - waveFront) / SOFTNESS));

    } else {
      // Phase 4: all off
      intensity_fn = () => 0.0;
    }

    for (const led of leds) {
      const { segment } = getLEDDescriptor(led.id);
      const pos = cascadePos(segment, led.t ?? 0);
      const intensity = intensity_fn(pos);

      led.intensity = intensity;
      led.color.setRGB(COLOR_R, COLOR_G * intensity, COLOR_B);
    }
  },
};
