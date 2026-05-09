import { LED } from "../core/ledSystem";
import { getLEDDescriptor } from "../ledMap";

// ─────────────────────────────────────────────────────────────
// NUCLEAR JELLYFISH SETTINGS
// ─────────────────────────────────────────────────────────────

const CYCLE   = 7.0;          // total cycle length in seconds
const BUILD_F = 0.65;         // fraction of cycle spent charging
const EXPL_F  = 0.08;         // fraction of cycle for the fast detonation
// FADE_F = 1 - BUILD_F - EXPL_F  (the rest)

const JELLY_STAGGER = 0.45;   // seconds between each jelly's detonation
const BUILD_TRAIL   = 0.28;   // how far behind the charge front the inner glow trails

// ─────────────────────────────────────────────────────────────
// "Hot metal" color ramp
//   h=0.00 → black
//   h=0.33 → red
//   h=0.66 → yellow
//   h=1.00 → white
// ─────────────────────────────────────────────────────────────
function hotColor(h: number): [number, number, number] {
  const c = Math.min(1, Math.max(0, h));
  return [
    Math.min(1, c * 3),
    Math.min(1, Math.max(0, c * 3 - 1)),
    Math.min(1, Math.max(0, c * 3 - 2)),
  ];
}

// Smooth bell-curve crest: bright at dist=0 from front, zero at dist=len.
function crest(dist: number, len: number): number {
  if (dist < 0 || dist >= len) return 0;
  return Math.sin((1 - dist / len) * Math.PI * 0.5);
}

// Maps each LED to a continuous 0→1 position along inner tip → bell → outer tip.
// Weighted by LED count on a standard jelly (inner=30, bell=10, outer=10).
function pathPos(segment: string, t: number): number {
  if (segment === "inner") return t * 0.6;
  if (segment === "bell")  return 0.6 + t * 0.2;
  return 0.8 + t * 0.2;
}

export const nuclearJellyfish = {
  name: "nuclearJellyfish",

  update(leds: LED[], time: number) {
    const BUILD   = CYCLE * BUILD_F;
    const EXPLODE = CYCLE * EXPL_F;
    const FADE    = CYCLE * (1 - BUILD_F - EXPL_F);

    for (const led of leds) {
      const { segment, jellyId } = getLEDDescriptor(led.id);
      const t   = led.t ?? 0;
      const pos = pathPos(segment, t);

      // Each jelly detonates JELLY_STAGGER seconds after the previous.
      const localTime = (time - jellyId * JELLY_STAGGER + CYCLE * 100) % CYCLE;

      let heat = 0;

      // ── Phase 1: CHARGE ──────────────────────────────────────────────────────
      // Energy rises slowly through inner tentacles from tip (pos=0) to root
      // (pos=0.6). Cubic ease-in means the front accelerates toward the bell —
      // slow and ominous at first, urgent by the end.
      if (localTime < BUILD) {
        const buildT = localTime / BUILD;                 // 0→1
        const front  = buildT * buildT * buildT * 0.6;   // cubic ease-in, 0→0.6

        if (segment === "inner") {
          // Glowing crest at the front edge
          heat = crest(front - pos, BUILD_TRAIL);
          // Dim smouldering glow behind the already-passed region
          if (pos < front) heat = Math.max(heat, 0.08 * buildT);
        } else {
          // Bell and outer barely glow — ominous anticipation
          heat = 0.04 * buildT * buildT;
        }

      // ── Phase 2: DETONATE ────────────────────────────────────────────────────
      // The pent-up energy releases at once. The bell explodes outward from
      // center (t=0) to rim (t=1), then the outer tentacles cascade downward.
      // The inner tentacles flash white and rapidly die.
      } else if (localTime < BUILD + EXPLODE) {
        const expT = (localTime - BUILD) / EXPLODE; // 0→1

        if (segment === "inner") {
          // White flash on impact, dies within the first third of EXPLODE
          heat = Math.max(0, 1.0 - expT * 3.5);

        } else if (segment === "bell") {
          // Explosive radial sweep: front crosses entire bell very fast
          const front  = 0.6 + expT * 0.35;   // reaches pos=0.95 by expT=1
          const behind = front - pos;
          // Sharp wave crest plus a sustained hot glow on the already-lit region
          heat = Math.max(
            crest(behind, 0.12),
            behind > 0 ? 1.0 - expT * 0.2 : 0
          );

        } else {
          // Outer: follows bell with a short delay, cascades from root to tip
          const delay  = 0.5;
          const outerT = Math.max(0, expT - delay) / (1 - delay);
          const front  = 0.8 + outerT * 0.35;
          const behind = front - pos;
          heat = Math.max(
            crest(behind, 0.12),
            behind > 0 ? (1 - outerT) * 0.9 : 0
          );
        }

        // Global brightness boost at the moment of detonation
        heat = Math.min(1, heat + (1 - expT) * 0.15);

      // ── Phase 3: FADE ────────────────────────────────────────────────────────
      // Everything cools. Bell is the mushroom cap — it holds longest.
      // Outer tentacles are the glowing mushroom stem below it.
      // Inner tentacles extinguish almost immediately.
      } else {
        const fadeT = (localTime - BUILD - EXPLODE) / FADE; // 0→1

        if (segment === "inner") {
          heat = Math.max(0, 0.18 * (1 - fadeT * 3));
        } else if (segment === "bell") {
          heat = Math.max(0, 0.85 * (1 - fadeT));
        } else {
          // Outer: linger as a hot column; tips (t=1) cool slightly faster
          heat = Math.max(0, 0.80 * (1 - t * 0.2) * (1 - fadeT * 0.88));
        }
      }

      const [r, g, b] = hotColor(heat);
      led.color.setRGB(r, g, b);
      led.intensity = heat;
    }
  },
};
