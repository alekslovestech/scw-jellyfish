import { LED } from "../core/ledSystem";
import { LEDDescriptor, getLEDDescriptor } from "../ledMap";
import { cfg } from "../config";

// --- CONFIGURATION ---
const SWEEP_DURATION  = 3.0; 
const RETURN_DURATION = 3.0; 
const PAUSE_FULL      = 0.06; 
const PAUSE_EMPTY     = 0.06; 
const CYCLE = PAUSE_FULL + SWEEP_DURATION + PAUSE_EMPTY + RETURN_DURATION;

const INNER_KEEP_ON_PCT = 0.5; 
const OUTER_KEEP_ON_PCT = 0.35; 

// This function now maps 0 -> 1 ONLY across the "sweepable" area
function offTime(desc: LEDDescriptor): number {
  const isHero = desc.jellyId === 0;
  const inner_count = isHero ? cfg.jelly0.inner_leds : cfg.hardware.inner_leds;
  const outer_count = isHero ? cfg.jelly0.outer_leds : cfg.hardware.outer_leds;

  if (desc.segment === "inner") {
    const t = desc.posInSegment / (inner_count - 1);
    // Normalize so the 20% mark is actually "1.0"
    return Math.min(t / INNER_KEEP_ON_PCT, 1.0);
  }

  if (desc.segment === "outer") {
    // outer pos 0 = top, pos max = tip. We want tip (bottom) to be 0.
    const t = ( (outer_count - 1) - desc.posInSegment ) / (outer_count - 1);
    
    // We want the sweep to end at the 20% mark from the top.
    // In our bottom-to-top 't', that mark is (1.0 - 0.2) = 0.8.
    return Math.min(t / (1.0 - OUTER_KEEP_ON_PCT), 1.0);
  }

  return 1.0; // Bell is always 1.0 (end of sweep)
}

export const movementSimulation = {
  name: "movementSimulation",

  update(leds: LED[], time: number) {
    const cycleTime = time % CYCLE;
    let sweepProgress: number;

    if (cycleTime < PAUSE_FULL) {
      sweepProgress = 0; 
    } 
    else if (cycleTime < PAUSE_FULL + SWEEP_DURATION) {
      sweepProgress = (cycleTime - PAUSE_FULL) / SWEEP_DURATION;
    } 
    else if (cycleTime < PAUSE_FULL + SWEEP_DURATION + PAUSE_EMPTY) {
      sweepProgress = 1.0; 
    } 
    else {
      const returnTime = cycleTime - (PAUSE_FULL + SWEEP_DURATION + PAUSE_EMPTY);
      sweepProgress = 1.0 - (returnTime / RETURN_DURATION);
    }

    for (const led of leds) {
      const desc = getLEDDescriptor(led.id);
      const isHero = desc.jellyId === 0;
      const inner_count = isHero ? cfg.jelly0.inner_leds : cfg.hardware.inner_leds;
      const outer_count = isHero ? cfg.jelly0.outer_leds : cfg.hardware.outer_leds;

      let alwaysOn = false;
      if (desc.segment === "bell") {
        alwaysOn = true; 
      } else if (desc.segment === "inner") {
        if (desc.posInSegment >= inner_count * INNER_KEEP_ON_PCT) alwaysOn = true;
      } else if (desc.segment === "outer") {
        if (desc.posInSegment <= outer_count * OUTER_KEEP_ON_PCT) alwaysOn = true;
      }

      // The key: sweepProgress (0->1) now maps perfectly to the sweepable area
      const on = alwaysOn || (sweepProgress < offTime(desc));

      led.color.setRGB(0, on ? 1 : 0, 0);
      led.intensity = on ? 1.0 : 0.0;
    }
  },
};