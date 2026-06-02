import { LED } from "../core/ledSystem";
import { LEDAnimation } from "./types";

type GradientStop = { pos: number; r: number; g: number; b: number };

// ── Gradients ─────────────────────────────────────────────────────────────────

const BELL_OUTER: GradientStop[] = [
  { pos: 0.00, r: 1, g: 1,   b: 1 },
  { pos: 0.20, r: 1, g: 1,   b: 0 },
  { pos: 0.40, r: 1, g: 0.2, b: 0 },
  { pos: 0.80, r: 1, g: 0,   b: 0 },
];

const BELL_FRACTION = 0.5;

const INNER: GradientStop[] = [
  { pos: 0.00, r: 1, g: 1,   b: 1 },
  { pos: 0.20, r: 1, g: 1,   b: 0 },
  { pos: 0.50, r: 1, g: 0.2, b: 0 },
  { pos: 0.90, r: 1, g: 0,   b: 0 },
];

// ── Tuning ────────────────────────────────────────────────────────────────────

const WOBBLE_AMP    = 0.10;
const WOBBLE_FREQ   = 5.0;
const WAVE_SPEED    = 10.5;

const FLICKER_AMP   = 0.9;
const FLICKER_FREQ  = 3.0;
const FLICKER_PHASE = 0.53;

// ── Scaling ───────────────────────────────────────────────────────────────────

const BASE_SCALE    = 0.40;
const MAX_SCALE     = 1.00;
const GROW_DURATION = 10;
const FADE_WIDTH    = 0.20;

// ── Fire breathing ────────────────────────────────────────────────────────────

const BREATH_AMP  = 0.12;
const BREATH_FREQ = 0.75;
const STRIP_AMP   = 0.09;
const STRIP_FREQ  = 1.5;
const PHI         = 2.3999;

// ── Explosion ─────────────────────────────────────────────────────────────────
// Three phases:
//   1. CHARGE  — jelly rapidly brightens and whitens (energy gathering)
//   2. FLASH   — white+red sweep races from bell outward, very brief
//   3. CUT     — instantly back to BASE_SCALE, no fade
//
// Total duration = CHARGE_DURATION + FLASH_DURATION (kept short and snappy).

const EXPLOSION_MIN_SCALE   = 0.70; // scale required before explosion can trigger
const EXPLOSION_PROBABILITY = 0.0008; // per frame per eligible jelly (~3 %/s at 60 fps)
const EXPLOSION_COOLDOWN    = 5.0;  // minimum seconds between explosions per jelly
const CHARGE_DURATION       = 0.40; // seconds of energy-gathering before the flash
const FLASH_DURATION        = 0.20; // seconds for the sweep to travel bell→tips
const EXPLOSION_DURATION    = CHARGE_DURATION + FLASH_DURATION;
const FLASH_TRAIL           = 0.25; // how far behind the sweep front LEDs stay lit (fromRoot units)

// ── Helpers ───────────────────────────────────────────────────────────────────

function sample(stops: GradientStop[], pos: number): [number, number, number] {
  const p = Math.max(0, Math.min(1, pos));
  if (p <= stops[0].pos) return [stops[0].r, stops[0].g, stops[0].b];
  const last = stops[stops.length - 1];
  if (p >= last.pos) return [last.r, last.g, last.b];
  for (let i = 1; i < stops.length; i++) {
    if (p <= stops[i].pos) {
      const a = stops[i - 1], b = stops[i];
      const f = (p - a.pos) / (b.pos - a.pos);
      return [a.r + (b.r - a.r) * f, a.g + (b.g - a.g) * f, a.b + (b.b - a.b) * f];
    }
  }
  return [last.r, last.g, last.b];
}

function jellyIdFromLedId(ledId: number): number {
  return ledId < 800 ? 0 : 1 + Math.floor((ledId - 800) / 400);
}

// ── Per-jelly state ───────────────────────────────────────────────────────────

let startTime = -1;
const jellyResetElapsed: number[] = new Array(13).fill(0);
const explosionStart: number[]    = new Array(13).fill(-Infinity);
const explosionEnd: number[]      = new Array(13).fill(-Infinity);

// ── Animation ─────────────────────────────────────────────────────────────────

export const fireExplosion: LEDAnimation = {
  name: "fire explosion",

  update(leds: LED[], time: number) {
    if (startTime < 0) startTime = time;
    const elapsed = time - startTime;

    // ── Per-jelly explosion management ────────────────────────────────────────
    for (let j = 0; j < 13; j++) {
      const expElapsed = elapsed - explosionStart[j];

      if (expElapsed < EXPLOSION_DURATION) continue; // still running

      if (expElapsed < EXPLOSION_DURATION + 0.05) {
        // Just finished — cut immediately to BASE_SCALE
        jellyResetElapsed[j] = elapsed;
        explosionEnd[j]      = elapsed;
      }

      const cooldownOk  = (elapsed - explosionEnd[j]) >= EXPLOSION_COOLDOWN;
      const growElapsed = Math.max(0, elapsed - jellyResetElapsed[j]);
      const scale       = BASE_SCALE + (MAX_SCALE - BASE_SCALE) * Math.min(1, growElapsed / GROW_DURATION);

      if (cooldownOk && scale >= EXPLOSION_MIN_SCALE && Math.random() < EXPLOSION_PROBABILITY) {
        explosionStart[j] = elapsed;
      }
    }

    // ── Global breath ─────────────────────────────────────────────────────────
    const globalBreath = BREATH_AMP * (
      0.65 * Math.sin(time * BREATH_FREQ) +
      0.35 * Math.sin(time * BREATH_FREQ * 2.3)
    );

    for (const led of leds) {
      const group   = led.group;
      const t       = led.t ?? 0;
      const jellyId = jellyIdFromLedId(led.id);

      // ── Shared geometry (needed by all branches) ───────────────────────────

      let gradPos: number;
      let gradient: GradientStop[];
      if (group === "bell") {
        gradPos = t * BELL_FRACTION;                       gradient = BELL_OUTER;
      } else if (group === "outer") {
        gradPos = BELL_FRACTION + t * (1 - BELL_FRACTION); gradient = BELL_OUTER;
      } else {
        gradPos = 1 - t;                                   gradient = INNER;
      }

      let fromRoot: number;
      if (group === "bell")       fromRoot = t * 0.5;
      else if (group === "outer") fromRoot = 0.5 + t * 0.5;
      else                        fromRoot = 1 - t;

      // ── Per-jelly scale ────────────────────────────────────────────────────
      const growElapsed  = Math.max(0, elapsed - jellyResetElapsed[jellyId]);
      const growProgress = Math.min(1, growElapsed / GROW_DURATION);
      const currentScale = BASE_SCALE + (MAX_SCALE - BASE_SCALE) * growProgress;

      const stripIndex = Math.floor(led.id / 50);
      const stripPhase = stripIndex * PHI;
      const stripPulse = STRIP_AMP * (
        0.7 * Math.sin(time * STRIP_FREQ + stripPhase) +
        0.3 * Math.sin(time * STRIP_FREQ * 2.1 + stripPhase * 1.6)
      );
      const effectiveScale = Math.min(MAX_SCALE, Math.max(BASE_SCALE, currentScale + globalBreath + stripPulse));
      const segmentAlpha   = Math.max(0, Math.min(1, (effectiveScale - fromRoot) / FADE_WIDTH));

      // ── Explosion branches ─────────────────────────────────────────────────
      const expElapsed = elapsed - explosionStart[jellyId];

      if (expElapsed >= 0 && expElapsed < EXPLOSION_DURATION) {

        if (expElapsed < CHARGE_DURATION) {
          // Phase 1 — CHARGE: fire rapidly brightens and whitens
          const cp       = expElapsed / CHARGE_DURATION;             // 0→1
          const wave     = WOBBLE_AMP * Math.sin(gradPos * WOBBLE_FREQ - time * WAVE_SPEED);
          const [r, g, b] = sample(gradient, gradPos + wave);

          // Shift toward white as charge builds
          const wr = r + (1 - r) * cp * 0.9;
          const wg = g + (1 - g) * cp * 0.9;
          const wb = b + (1 - b) * cp * 0.9;

          // Flicker goes wild and overall intensity ramps beyond normal
          const fastFlicker = 1 - FLICKER_AMP
            * (0.5 + 0.5 * Math.sin(time * FLICKER_FREQ * (1 + 6 * cp) + led.id * FLICKER_PHASE));
          const boost = 1 + 2.0 * cp; // up to 3× brightness

          led.color.setRGB(wr, wg, wb);
          led.intensity = Math.min(1, fastFlicker * boost * Math.max(segmentAlpha, cp));

        } else {
          // Phase 2 — FLASH: white+red sweep races from bell outward
          const sweepFront = (expElapsed - CHARGE_DURATION) / FLASH_DURATION; // 0→1
          const dist       = sweepFront - fromRoot; // positive = swept, negative = not yet

          if (dist >= 0 && dist < FLASH_TRAIL) {
            // On or just behind the sweep front
            const intensity = 1 - dist / FLASH_TRAIL;
            // White at bell (gradPos≈0), red at tips (gradPos≈1)
            const expG = Math.max(0, 1 - gradPos);
            const expB = Math.max(0, 1 - gradPos * 1.5);
            led.color.setRGB(1, expG, expB);
            led.intensity = intensity;
          } else {
            led.color.setRGB(0, 0, 0);
            led.intensity = 0;
          }
        }
        continue;
      }

      // ── Normal fire ───────────────────────────────────────────────────────

      if (segmentAlpha === 0) {
        led.color.setRGB(0, 0, 0);
        led.intensity = 0;
        continue;
      }

      const wave      = WOBBLE_AMP * Math.sin(gradPos * WOBBLE_FREQ - time * WAVE_SPEED);
      const [r, g, b] = sample(gradient, gradPos + wave);
      const flicker   = 1 - FLICKER_AMP * (0.5 + 0.5 * Math.sin(time * FLICKER_FREQ + led.id * FLICKER_PHASE));

      led.color.setRGB(r, g, b);
      led.intensity = flicker * segmentAlpha;
    }
  },
};
