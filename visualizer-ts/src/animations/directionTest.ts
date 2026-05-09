import { LED } from "../core/ledSystem";
import { LEDDescriptor, getLEDDescriptor } from "../ledMap";
import { cfg } from "../config";

// ─────────────────────────────────────────────────────────────────────────────
// DIRECTION TEST
//
// All LEDs start ON (white). They turn off one by one to verify wiring order.
//
// Inner tentacle sweep:
//   INNER_DIR =  1 → tip (pos 0, bottom) turns off first, root (top) last
//   INNER_DIR = -1 → root turns off first, tip last
//
// Outer + Bell sweep (they act as one continuous strip):
//   OUTER_BELL_DIR =  1 → outer tip (bottom) → outer root → bell outer rim → bell inner edge
//   OUTER_BELL_DIR = -1 → reverse of the above
//
// Jelly 0 (hero) uses its own segment sizes from cfg.jelly0.
// All other jellies use cfg.hardware.
// ─────────────────────────────────────────────────────────────────────────────

const INNER_DIR      =  1;
const OUTER_BELL_DIR =  1;

const SWEEP_DURATION = 4.0;
const PAUSE_FULL     = 1.5;
const PAUSE_EMPTY    = 1.0;
const CYCLE = PAUSE_FULL + SWEEP_DURATION + PAUSE_EMPTY;

// ─────────────────────────────────────────────────────────────────────────────

// Returns 0→1: when this LED turns off (0 = first, 1 = last).
// Picks cfg.jelly0 or cfg.hardware based on jellyId.
function offTime(desc: LEDDescriptor): number {
  const isHero = desc.jellyId === 0;

  const inner_max        = isHero ? cfg.jelly0.inner_leds - 1        : cfg.hardware.inner_leds - 1;
  const outer_count      = isHero ? cfg.jelly0.outer_leds             : cfg.hardware.outer_leds;
  const bell_count       = isHero ? cfg.jelly0.bell_leds              : cfg.hardware.bell_leds;
  const outer_bell_total = outer_count + bell_count - 1;

  if (desc.segment === "inner") {
    const t = desc.posInSegment / inner_max;
    return INNER_DIR === 1 ? t : 1 - t;
  }

  // Outer + Bell treated as one combined strip.
  // outer: pos 0 = root (top), pos max = tip (bottom) → reversed for bottom-first
  // bell:  continues after outer root, ending at bell inner edge
  let combinedPos: number;
  if (desc.segment === "outer") {
    combinedPos = outer_count - 1 - desc.posInSegment; // tip = 0, root = outer_count-1
  } else {
    combinedPos = outer_count + (bell_count - 1 - desc.posInSegment); // outer-rim = outer_count, inner-edge = outer_bell_total
  }

  const t = combinedPos / outer_bell_total;
  return OUTER_BELL_DIR === 1 ? t : 1 - t;
}

// ─────────────────────────────────────────────────────────────────────────────

export const directionTest = {
  name: "directionTest",

  update(leds: LED[], time: number) {
    const cycleTime = time % CYCLE;

    let sweepProgress: number;
    if (cycleTime < PAUSE_FULL) {
      sweepProgress = 0;
    } else if (cycleTime < PAUSE_FULL + SWEEP_DURATION) {
      sweepProgress = (cycleTime - PAUSE_FULL) / SWEEP_DURATION;
    } else {
      sweepProgress = 1;
    }

    for (const led of leds) {
      const desc = getLEDDescriptor(led.id);
      const on = sweepProgress <= offTime(desc);

      led.color.setRGB(on ? 1 : 0, on ? 1 : 0, on ? 1 : 0);
      led.intensity = on ? 1.0 : 0.0;
    }
  },
};
