import { LED } from "../core/ledSystem";
import { LEDDescriptor, getLEDDescriptor } from "../ledMap";
import { cfg } from "../config";

// ─────────────────────────────────────────────────────────────
// DUAL-TONE FLOW
//
// Each jellyfish wears a two-colour palette: the inner tentacles glow in one
// colour, the bell + outer tentacles in another. The colour diffuses smoothly
// along the inner→bell→outer path, and the bell/outer colour bleeds a little
// way down into the inner tentacles (never a hard boundary).
//
// Bell and outer keep the calm sweeping movement from movementSimulation.
// ─────────────────────────────────────────────────────────────

type RGB = { r: number; g: number; b: number };

// Hand-picked two-colour palettes (inner → outer) chosen to sit well together.
// Cycled across jellies by jellyId, so each jelly gets its own pairing.
const PALETTES: { inner: RGB; outer: RGB }[] = [
  // --- intense greens / oranges / yellows (fully saturated, no washout) ---
  { inner: { r: 1.00, g: 0.20, b: 0.00  }, outer: { r: 1.00, g: 0.00, b: 0.00 } }, // green → yellow
  { inner: { r: 1.00, g: 0.90, b: 0.00 }, outer: { r: 1.00, g: 0.35, b: 0.00 } }, // yellow → orange
  { inner: { r: 1.00, g: 0.45, b: 0.00 }, outer: { r: 0.10, g: 0.85, b: 0.10 } }, // orange → green
  { inner: { r: 0.60, g: 0.95, b: 0.00 }, outer: { r: 1.00, g: 0.50, b: 0.00 } }, // lime → orange
  { inner: { r: 0.00, g: 0.70, b: 0.15 }, outer: { r: 1.00, g: 0.75, b: 0.00 } }, // deep green → gold
  { inner: { r: 1.00, g: 0.60, b: 0.00 }, outer: { r: 0.70, g: 0.95, b: 0.00 } }, // amber → chartreuse
];

// --- Calm sweep (same feel as movementSimulation) ---
const SWEEP_DURATION  = 7.0;
const RETURN_DURATION = 7.0;
const PAUSE_FULL      = 0.06;
const PAUSE_EMPTY     = 0.06;
const CYCLE = PAUSE_FULL + SWEEP_DURATION + PAUSE_EMPTY + RETURN_DURATION;

const INNER_KEEP_ON_PCT = 0.13; // only the bottom ~few px of the inner tip sweep
const OUTER_KEEP_ON_PCT = 0.70;
const SPEED_SPREAD = 0.24;      // ±12% per-tentacle cycle-speed variation

// Where the bell/outer colour bleeds down into the inner tentacles (path pos).
// Below BLEND_START = pure inner colour; above BLEND_END = pure outer colour.
const BLEND_START = 0.35; // partway up the inner tentacle
const BLEND_END   = 0.72; // around the bell rim

// Deterministic hash of (a, b, salt) → float in [0,1). Integer math only.
function hash01(a: number, b: number, salt: number): number {
  let h = (Math.imul(a, 73856093) ^ Math.imul(b, 19349663) ^ Math.imul(salt, 83492791)) >>> 0;
  h = Math.imul(h ^ (h >>> 15), 0x2c1b3c6d);
  h ^= h >>> 12;
  h = Math.imul(h ^ (h >>> 4), 0x297a2d39);
  h ^= h >>> 15;
  return (h >>> 0) / 4294967296;
}

type TentacleParams = { phase: number; speedMult: number };
const paramCache = new Map<number, TentacleParams>();

function tentacleParams(jellyId: number, stripIndex: number): TentacleParams {
  const key = jellyId * 100 + stripIndex;
  let p = paramCache.get(key);
  if (!p) {
    p = {
      phase: hash01(jellyId, stripIndex, 0) * CYCLE,
      speedMult: 1 + (hash01(jellyId, stripIndex, 1) - 0.5) * SPEED_SPREAD,
    };
    paramCache.set(key, p);
  }
  return p;
}

// Smoothstep between two edges.
function smoothstep(edge0: number, edge1: number, x: number): number {
  const t = Math.min(1, Math.max(0, (x - edge0) / (edge1 - edge0)));
  return t * t * (3 - 2 * t);
}

// Per-tentacle sweep progress (0→1), eased, on its own phase-shifted clock.
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

  return smoothstep(0, 1, sweepProgress);
}

// Maps 0 -> 1 across the "sweepable" area of each segment.
function offTime(desc: LEDDescriptor): number {
  const isHero = desc.jellyId === 0;
  const inner_count = isHero ? cfg.jelly0.inner_leds : cfg.hardware.inner_leds;
  const outer_count = isHero ? cfg.jelly0.outer_leds : cfg.hardware.outer_leds;

  if (desc.segment === "inner") {
    const t = desc.posInSegment / (inner_count - 1);
    return Math.min(t / INNER_KEEP_ON_PCT, 1.0);
  }

  if (desc.segment === "outer") {
    const t = ((outer_count - 1) - desc.posInSegment) / (outer_count - 1);
    return Math.min(t / (1.0 - OUTER_KEEP_ON_PCT), 1.0);
  }

  return 1.0; // Bell is always 1.0 (end of sweep)
}

// Continuous position 0→1 along the full inner→bell→outer path.
//   inner tip (0.0) → inner root (0.6) → bell rim (0.8) → outer tip (1.0)
function pathPos(segment: string, t: number): number {
  if (segment === "inner") return t * 0.6;        // t=0 tip → t=1 root
  if (segment === "bell")  return 0.6 + t * 0.2;  // t=0 inner edge → t=1 outer rim
  return 0.8 + t * 0.2;                            // t=0 root → t=1 outer tip
}

export const dualToneFlow = {
  name: "dualToneFlow",

  update(leds: LED[], time: number) {
    for (const led of leds) {
      const desc = getLEDDescriptor(led.id);
      const p = tentacleParams(desc.jellyId, desc.stripIndex);
      const isHero = desc.jellyId === 0;
      const inner_count = isHero ? cfg.jelly0.inner_leds : cfg.hardware.inner_leds;
      const outer_count = isHero ? cfg.jelly0.outer_leds : cfg.hardware.outer_leds;

      const palette = PALETTES[desc.jellyId % PALETTES.length];

      // --- Movement: calm sweep (bell always on) ---
      const sweepProgress = tentacleSweep(time, p.phase, p.speedMult);

      let alwaysOn = false;
      if (desc.segment === "bell") {
        alwaysOn = true;
      } else if (desc.segment === "inner") {
        if (desc.posInSegment >= inner_count * INNER_KEEP_ON_PCT) alwaysOn = true;
      } else if (desc.segment === "outer") {
        if (desc.posInSegment <= outer_count * OUTER_KEEP_ON_PCT) alwaysOn = true;
      }

      const on = alwaysOn || (sweepProgress < offTime(desc));

      // --- Colour: diffuse inner colour → outer colour along the path. The
      // outer colour bleeds partway down into the inner tentacles. ---
      const pos = pathPos(desc.segment, desc.t);
      const mix = smoothstep(BLEND_START, BLEND_END, pos);
      led.color.setRGB(
        palette.inner.r + (palette.outer.r - palette.inner.r) * mix,
        palette.inner.g + (palette.outer.g - palette.inner.g) * mix,
        palette.inner.b + (palette.outer.b - palette.inner.b) * mix,
      );
      led.intensity = on ? 1.0 : 0.0;
    }
  },
};
