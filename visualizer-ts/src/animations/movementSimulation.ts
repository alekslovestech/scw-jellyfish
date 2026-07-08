import { LED } from "../core/ledSystem";
import { LEDDescriptor, getLEDDescriptor } from "../ledMap";
import { cfg } from "../config";

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

// --- OCCASIONAL "INTENSE REACTION" (rare, global, sudden) ---
// Most of the time tentacles drift calmly. Now and then the WHOLE installation
// reacts at once: a fast, sudden flare where every tentacle's sweep snaps out
// into many more pixels, then quickly relaxes back to calm. Rare and synchronized.
const BURST_PERIOD = 6.0; // s — spacing of potential reaction windows
const BURST_CHANCE = 0.4;  // fraction of windows that actually fire ("not often")
const BURST_ATTACK = 0.15; // s — near-instant snap to full intensity
const BURST_DECAY  = 1.2;  // s — quick relax back to calm
const INNER_BURST_KEEP = 0.55; // inner reach during a burst (vs calm INNER_KEEP_ON_PCT)
const OUTER_BURST_KEEP = 0.25; // outer reach during a burst (vs calm OUTER_KEEP_ON_PCT)

// Deterministic hash of (a, b, salt) → float in [0,1). Integer math only.
function hash01(a: number, b: number, salt: number): number {
  let h = (Math.imul(a, 73856093) ^ Math.imul(b, 19349663) ^ Math.imul(salt, 83492791)) >>> 0;
  h = Math.imul(h ^ (h >>> 15), 0x2c1b3c6d);
  h ^= h >>> 12;
  h = Math.imul(h ^ (h >>> 4), 0x297a2d39);
  h ^= h >>> 15;
  return (h >>> 0) / 4294967296;
}

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

// Global "intense reaction" strength (0 = calm, 1 = full flare), shared by every
// tentacle so the whole installation reacts together. Only some windows fire, and
// each firing is a fast snap up followed by a quick decay — sudden, not gradual.
function globalBurst(time: number): number {
  const windowIndex = Math.floor(time / BURST_PERIOD);

  // Only a fraction of windows fire a reaction.
  if (hash01(windowIndex, 0, 7) >= BURST_CHANCE) return 0;

  // Randomize the exact moment within the window so it isn't clockwork.
  const slack = BURST_PERIOD - (BURST_ATTACK + BURST_DECAY);
  const start = hash01(windowIndex, 1, 7) * Math.max(0, slack);
  const local = (time - windowIndex * BURST_PERIOD) - start;

  if (local < 0) return 0;
  if (local < BURST_ATTACK) return local / BURST_ATTACK;      // fast snap up
  const d = local - BURST_ATTACK;
  if (d < BURST_DECAY) return 1 - d / BURST_DECAY;            // quick relax
  return 0;
}

// Continuous position 0→1 along the full inner→bell→outer path, so colour can
// diffuse smoothly instead of switching at the segment boundary.
//   inner tip (0.0) → inner root (0.6) → bell rim (0.8) → outer tip (1.0)
function pathPos(segment: string, t: number): number {
  if (segment === "inner") return t * 0.6;        // t=0 tip → t=1 root
  if (segment === "bell")  return 0.6 + t * 0.2;  // t=0 inner edge → t=1 outer rim
  return 0.8 + t * 0.2;                            // t=0 root → t=1 outer tip
}

// Orange (inner tips) → green (outer tips), blended along the path.
const ORANGE = { r: 1, g: 0.5, b: 0 };
const GREEN  = { r: 0, g: 1, b: 0 };

export const movementSimulation = {
  name: "movementSimulation",

  update(leds: LED[], time: number) {
    // One reaction value for the whole installation this frame.
    const burst = globalBurst(time);

    for (const led of leds) {
      const desc = getLEDDescriptor(led.id);
      const p = tentacleParams(desc.jellyId, desc.stripIndex);
      const isHero = desc.jellyId === 0;
      const inner_count = isHero ? cfg.jelly0.inner_leds : cfg.hardware.inner_leds;
      const outer_count = isHero ? cfg.jelly0.outer_leds : cfg.hardware.outer_leds;

      const sweepProgress = tentacleSweep(time, p.phase, p.speedMult);

      // During a reaction, every tentacle's sweep spreads into more pixels.
      const innerKeep = p.innerKeep + (INNER_BURST_KEEP - p.innerKeep) * burst;
      const outerKeep = p.outerKeep + (OUTER_BURST_KEEP - p.outerKeep) * burst;

      let alwaysOn = false;
      if (desc.segment === "bell") {
        alwaysOn = true;
      } else if (desc.segment === "inner") {
        if (desc.posInSegment >= inner_count * innerKeep) alwaysOn = true;
      } else if (desc.segment === "outer") {
        if (desc.posInSegment <= outer_count * outerKeep) alwaysOn = true;
      }

      const on = alwaysOn || (sweepProgress < offTime(desc, innerKeep, outerKeep));

      // Diffuse orange (inner tips) → green along the path. The curve pushes the
      // blend toward green early, so green already starts within the inner segment
      // and only the very tips stay orange.
      const pos = pathPos(desc.segment, desc.t);
      const mix = Math.pow(pos, 0.35);
      led.color.setRGB(
        ORANGE.r + (GREEN.r - ORANGE.r) * mix,
        ORANGE.g + (GREEN.g - ORANGE.g) * mix,
        ORANGE.b + (GREEN.b - ORANGE.b) * mix,
      );
      led.intensity = on ? 1.0 : 0.0;
    }
  },
};
