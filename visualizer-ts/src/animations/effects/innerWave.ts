import { Effect } from "../types";
import { getLEDDescriptor } from "../../ledMap";

// --- INNER TENTACLE WAVE ---
// Makes ONLY the inner tentacles undulate: a sine wave of brightness travels down
// the length of each inner tentacle. `spread` decides whether neighbouring
// tentacles ripple out of phase or all wave together, and `reach` limits how far
// up the tentacle (from the tip) the wave is active.
//
// It MODULATES the base rather than overriding it: it keeps whatever colour the
// base gave the inner tentacles and multiplies their brightness by the wave.
// Because it multiplies, it also preserves the base's reach — so the
// intenseReaction flare still shows through.

const params = {
  speed: 2, // how fast the wave travels down the tentacle
  waves: 2, // number of crests along the tentacle length
  reach: 1, // how far up the tentacle the wave reaches (0 = tips only, 1 = whole)
  stripsPerWave: 2, // adjacent tentacles per full phase cycle (~2 = tight ripple)
  spread: 1, // 0 = all tentacles in sync, 1 = fully out of sync
  depth: 1, // brightness modulation depth (0 = flat, 1 = full dark troughs)
};

const TWO_PI = Math.PI * 2;

// Smoothstep 0→1 between edges (clamped).
function smoothstep(e0: number, e1: number, x: number): number {
  const t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0)));
  return t * t * (3 - 2 * t);
}

export const innerWave: Effect = {
  name: "innerWave",
  enabled: false, // off by default — toggle it on in the panel

  params,
  controls: [
    { key: "speed", label: "speed", min: 0, max: 10, step: 0.5 },
    { key: "waves", label: "waves", min: 0.5, max: 6, step: 0.5 },
    { key: "reach", label: "reach", min: 0, max: 1, step: 0.05 },
    { key: "stripsPerWave", label: "strips/wave", min: 1, max: 8, step: 1 },
    { key: "spread", label: "spread (sync)", min: 0, max: 1, step: 0.05 },
    { key: "depth", label: "depth", min: 0, max: 1, step: 0.05 },
  ],

  apply(leds, time) {
    // Per-tentacle phase offset, scaled by `spread` (0 = in sync, 1 = full offset).
    const perStrip = (TWO_PI / Math.max(1, params.stripsPerWave)) * params.spread;

    for (const led of leds) {
      const desc = getLEDDescriptor(led.id);
      if (desc.segment !== "inner") continue; // inner tentacles only

      // Position along the tentacle: t = 0 tip (bottom), 1 = root (top).
      // The wave runs down the length; each tentacle is offset by `spread`.
      const phase =
        params.waves * TWO_PI * (1 - desc.t) -
        params.speed * time +
        desc.stripIndex * perStrip;
      const s = 0.5 + 0.5 * Math.sin(phase); // 0..1

      // Limit the wave to the lower `reach` fraction (from the tip up); above it
      // the tentacle stays at its base brightness (no modulation).
      const reachMask = 1 - smoothstep(params.reach - 0.1, params.reach, desc.t);
      const effDepth = params.depth * reachMask;

      // Multiply the base's brightness — keeps its colour and its reach.
      led.intensity *= 1 - effDepth * (1 - s);
    }
  },
};
