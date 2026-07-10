import { LEDDescriptor } from "../ledMap";
import { cfg } from "../config";
import { hash01 } from "./hash";
import { fxState } from "../core/fxState";

// ── Shared "reach" engine ─────────────────────────────────────────────────────
// The per-tentacle sweep + reach logic that defines which LEDs are lit, and how
// the "intense reaction" flare extends that reach. Extracted so every base can
// share the *exact same motion and reaction*: movementSimulation, innerSpreadWave
// and fireSteady all call isLit() and differ only in colour/texture.

// --- CONFIGURATION ---
const SWEEP_DURATION  = 7.0; // slow, wave-like rise
const RETURN_DURATION = 7.0; // slow, wave-like fall
const PAUSE_FULL      = 0.06;
const PAUSE_EMPTY     = 0.06;
const CYCLE = PAUSE_FULL + SWEEP_DURATION + PAUSE_EMPTY + RETURN_DURATION;

// Center values — each tentacle jitters slightly around these (see tentacleParams).
const INNER_KEEP_ON_PCT = 0.13; // only the bottom ~6 px of the inner tip sweep up
const OUTER_KEEP_ON_PCT = 0.70;

// --- ORGANIC MOTION (subtle) ---
// Each physical tentacle (jellyId + stripIndex) gets its own phase, speed and
// reach so they drift out of lockstep. Values come from a deterministic integer
// hash — stable frame-to-frame, reproducible, and portable to the Arduino firmware.
const SPEED_SPREAD = 0.24; // ±12% cycle-speed variation between tentacles
const KEEP_JITTER  = 0.08; // ±0.04 variation in how far each tentacle sweeps

// --- RESPONSE TO "INTENSE REACTION" ---
// The intenseReaction effect drives a global excitement value (0 = calm, 1 = full
// flare) into fxState. When it rises, every tentacle's sweep snaps out into many
// more pixels; these are the reach targets at full excitement.
const INNER_BURST_KEEP = 0.55; // inner reach at full excitement
const OUTER_BURST_KEEP = 0.25; // outer reach at full excitement

type TentacleParams = {
  phase: number;      // seconds into the cycle this tentacle starts
  speedMult: number;  // per-tentacle cycle-speed multiplier
  innerKeep: number;  // per-tentacle inner keep-on fraction
  outerKeep: number;  // per-tentacle outer keep-on fraction
};

const paramCache = new Map<number, TentacleParams>();

function tentacleParams(jellyId: number, stripIndex: number): TentacleParams {
  const key = jellyId * 100 + stripIndex;
  let p = paramCache.get(key);
  if (!p) {
    const r0 = hash01(jellyId, stripIndex, 0);
    const r1 = hash01(jellyId, stripIndex, 1);
    const r2 = hash01(jellyId, stripIndex, 2);
    const r3 = hash01(jellyId, stripIndex, 3);
    p = {
      phase: r0 * CYCLE,
      speedMult: 1 + (r1 - 0.5) * SPEED_SPREAD,
      innerKeep: INNER_KEEP_ON_PCT + (r2 - 0.5) * KEEP_JITTER,
      outerKeep: OUTER_KEEP_ON_PCT + (r3 - 0.5) * KEEP_JITTER,
    };
    paramCache.set(key, p);
  }
  return p;
}

// Smoothstep — eases the sweep so the turnarounds at each end aren't abrupt.
function ease(p: number): number {
  const c = Math.min(1, Math.max(0, p));
  return c * c * (3 - 2 * c);
}

// Per-tentacle sweep progress (0→1), on its own phase-shifted / speed-scaled clock.
function tentacleSweep(time: number, phase: number, speedMult: number): number {
  const localTime = (((time * speedMult + phase) % CYCLE) + CYCLE) % CYCLE;
  let sweepProgress: number;

  if (localTime < PAUSE_FULL) {
    sweepProgress = 0;
  } else if (localTime < PAUSE_FULL + SWEEP_DURATION) {
    sweepProgress = (localTime - PAUSE_FULL) / SWEEP_DURATION;
  } else if (localTime < PAUSE_FULL + SWEEP_DURATION + PAUSE_EMPTY) {
    sweepProgress = 1.0;
  } else {
    const returnTime = localTime - (PAUSE_FULL + SWEEP_DURATION + PAUSE_EMPTY);
    sweepProgress = 1.0 - returnTime / RETURN_DURATION;
  }

  return ease(sweepProgress);
}

// Maps 0 -> 1 ONLY across the "sweepable" area, using this tentacle's keep fractions.
function offTime(desc: LEDDescriptor, innerKeep: number, outerKeep: number): number {
  const isHero = desc.jellyId === 0;
  const inner_count = isHero ? cfg.jelly0.inner_leds : cfg.hardware.inner_leds;
  const outer_count = isHero ? cfg.jelly0.outer_leds : cfg.hardware.outer_leds;

  if (desc.segment === "inner") {
    const t = desc.posInSegment / (inner_count - 1);
    return Math.min(t / innerKeep, 1.0);
  }

  if (desc.segment === "outer") {
    // outer pos 0 = top, pos max = tip. We want tip (bottom) to be 0.
    const t = ((outer_count - 1) - desc.posInSegment) / (outer_count - 1);
    return Math.min(t / (1.0 - outerKeep), 1.0);
  }

  return 1.0; // Bell is always 1.0 (end of sweep)
}

// Is this LED lit right now? Combines the per-tentacle sweep with the reach that
// widens during an intense reaction (fxState.excitement). This is the single
// source of truth for the tentacle motion + reaction across all bases.
export function isLit(desc: LEDDescriptor, time: number): boolean {
  const p = tentacleParams(desc.jellyId, desc.stripIndex);
  const isHero = desc.jellyId === 0;
  const inner_count = isHero ? cfg.jelly0.inner_leds : cfg.hardware.inner_leds;
  const outer_count = isHero ? cfg.jelly0.outer_leds : cfg.hardware.outer_leds;
  const excitement = fxState.excitement;

  const sweepProgress = tentacleSweep(time, p.phase, p.speedMult);

  // During a reaction, every tentacle's sweep spreads into more pixels.
  const innerKeep = p.innerKeep + (INNER_BURST_KEEP - p.innerKeep) * excitement;
  const outerKeep = p.outerKeep + (OUTER_BURST_KEEP - p.outerKeep) * excitement;

  let alwaysOn = false;
  if (desc.segment === "bell") {
    alwaysOn = true;
  } else if (desc.segment === "inner") {
    if (desc.posInSegment >= inner_count * innerKeep) alwaysOn = true;
  } else if (desc.segment === "outer") {
    if (desc.posInSegment <= outer_count * outerKeep) alwaysOn = true;
  }

  return alwaysOn || (sweepProgress < offTime(desc, innerKeep, outerKeep));
}
