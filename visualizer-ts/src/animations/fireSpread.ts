import { LED } from "../core/ledSystem";
import { LEDAnimation } from "./types";

type GradientStop = { pos: number; r: number; g: number; b: number };

const BELL_OUTER: GradientStop[] = [
  { pos: 0.00, r: 1,   g: 1,   b: 1 },
  { pos: 0.20, r: 1,   g: 1,   b: 0 },
  { pos: 0.40, r: 1,   g: 0.2, b: 0 },
  { pos: 0.80, r: 1,   g: 0,   b: 0 },
];

const GREEN_BELL_OUTER: GradientStop[] = [
  { pos: 0.00, r: 1,   g: 1,   b: 1   }, // white — unchanged
  { pos: 0.20, r: 0.6, g: 1,   b: 0   }, // yellow-green
  { pos: 0.40, r: 0.1, g: 0.9, b: 0   }, // green
  { pos: 0.80, r: 0,   g: 0.4, b: 0   }, // dark green
];

const BLUE_BELL_OUTER: GradientStop[] = [
  { pos: 0.00, r: 1,   g: 1,   b: 1   }, // white — unchanged
  { pos: 0.20, r: 0.4, g: 0.8, b: 1   }, // pale cyan
  { pos: 0.40, r: 0.1, g: 0.3, b: 1   }, // electric blue
  { pos: 0.80, r: 0.1, g: 0,   b: 0.6 }, // deep indigo
];

const BLUE_INNER: GradientStop[] = [
  { pos: 0.00, r: 1,   g: 1,   b: 1   }, // white — unchanged
  { pos: 0.20, r: 0.4, g: 0.8, b: 1   }, // pale cyan
  { pos: 0.50, r: 0.1, g: 0.3, b: 1   }, // electric blue
  { pos: 0.90, r: 0.1, g: 0,   b: 0.6 }, // deep indigo
];

const BELL_FRACTION = 0.5;

const INNER: GradientStop[] = [
  { pos: 0.00, r: 1,   g: 1,   b: 1 },
  { pos: 0.20, r: 1,   g: 1,   b: 0 },
  { pos: 0.50, r: 1,   g: 0.2, b: 0 },
  { pos: 0.90, r: 1,   g: 0,   b: 0 },
];

const GREEN_INNER: GradientStop[] = [
  { pos: 0.00, r: 1,   g: 1,   b: 1   }, // white — unchanged
  { pos: 0.20, r: 0.6, g: 1,   b: 0   }, // yellow-green
  { pos: 0.50, r: 0.1, g: 0.9, b: 0   }, // green
  { pos: 0.90, r: 0,   g: 0.4, b: 0   }, // dark green
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
const GROW_DURATION = 15;
const FADE_WIDTH    = 0.20;

// ── Breathing ─────────────────────────────────────────────────────────────────

const BREATH_AMP  = 0.12;
const BREATH_FREQ = 0.75;
const STRIP_AMP   = 0.09;
const STRIP_FREQ  = 1.5;
const PHI         = 2.3999;
const SHRINK_MAX  = 0.30;

// ── Spread timing ─────────────────────────────────────────────────────────────

const SPREAD_START   = 8;
const SPREAD_STAGGER = 2;

// ── Pre-ignition glow ─────────────────────────────────────────────────────────

const PRE_IGNITION_WINDOW    = 4;
const PRE_IGNITION_INTENSITY = 0.08;
const KINDLE_DURATION        = 2;

// ── Energy build-up ───────────────────────────────────────────────────────────
// Once a jelly finishes growing (kindle + grow complete) it starts gaining energy.
// Energy gradually increases per-strip chaos and flicker amplitude for that jelly only.

const ENERGY_START      = KINDLE_DURATION + GROW_DURATION; // seconds after ignition
const ENERGY_DURATION   = 20;   // seconds to ramp from calm to full energy
const MAX_ENERGY_STRIP  = 0.38; // strip swing at peak — each arm contracts/re-expands like the grow phase
const MAX_ENERGY_SHRINK = 0.62; // at peak, strips can dip all the way back to BASE_SCALE

// ── Green shift ───────────────────────────────────────────────────────────────
// GREEN_START   seconds from animation start before palette begins shifting.
//               Last jelly ignites at 30 s — set anywhere after that.
// GREEN_DURATION  how many seconds the classic->green crossfade takes.
// BLUE_START    must be >= GREEN_START + GREEN_DURATION (after green is fully done).
// BLUE_DURATION   how many seconds the green->blue crossfade takes.
const GREEN_START         = 30;
const GREEN_DURATION      = 20;
const GREEN_JELLY_STAGGER = 1.5; // seconds between each jelly starting its green shift
const BLUE_START          = 55;
const BLUE_DURATION       = 20;
const BLUE_JELLY_STAGGER  = 1.5; // seconds between each jelly starting its blue shift

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

function sampleBlend(
  stopsA: GradientStop[], stopsB: GradientStop[], pos: number, mix: number
): [number, number, number] {
  const [ar, ag, ab] = sample(stopsA, pos);
  const [br, bg, bb] = sample(stopsB, pos);
  return [ar + (br - ar) * mix, ag + (bg - ag) * mix, ab + (bb - ab) * mix];
}

// Jelly 0: IDs 0–799 (800 LEDs). Jellies 1–12: 400 LEDs each starting at 800.
function jellyIdFromLedId(ledId: number): number {
  return ledId < 800 ? 0 : 1 + Math.floor((ledId - 800) / 400);
}

function ignitionTime(jellyId: number): number {
  return jellyId === 0 ? 0 : SPREAD_START + (jellyId - 1) * SPREAD_STAGGER;
}

let startTime = -1;

// ── Animation ─────────────────────────────────────────────────────────────────

export const fireSpread: LEDAnimation = {
  name: "fireSpread",
  colorMode: "three",

  update(leds: LED[], time: number) {
    if (startTime < 0) startTime = time;
    const elapsed = time - startTime;

    const globalBreath = BREATH_AMP * (
      0.65 * Math.sin(time * BREATH_FREQ) +
      0.35 * Math.sin(time * BREATH_FREQ * 2.3)
    );

    for (const led of leds) {
      const group = led.group;
      const t     = led.t ?? 0;

      const jellyId      = jellyIdFromLedId(led.id);
      const iTime        = ignitionTime(jellyId);
      const jellyElapsed = elapsed - iTime;

      // Each jelly starts its colour transition at a staggered time
      const greenProgress = Math.max(0, Math.min(1,
        (elapsed - GREEN_START - jellyId * GREEN_JELLY_STAGGER) / GREEN_DURATION
      ));
      const blueProgress  = Math.max(0, Math.min(1,
        (elapsed - BLUE_START  - jellyId * BLUE_JELLY_STAGGER)  / BLUE_DURATION
      ));

      // Gradient position needed by both pre-glow and fire
      let gradPos: number;
      let gradient: GradientStop[];
      let greenGradient: GradientStop[];
      let blueGradient: GradientStop[];
      if (group === "bell") {
        gradPos = t * BELL_FRACTION;
        gradient = BELL_OUTER; greenGradient = GREEN_BELL_OUTER; blueGradient = BLUE_BELL_OUTER;
      } else if (group === "outer") {
        gradPos = BELL_FRACTION + t * (1 - BELL_FRACTION);
        gradient = BELL_OUTER; greenGradient = GREEN_BELL_OUTER; blueGradient = BLUE_BELL_OUTER;
      } else {
        gradPos = 1 - t;
        gradient = INNER; greenGradient = GREEN_INNER; blueGradient = BLUE_INNER;
      }

      // Bell and outer share a continuous path; inner is independent
      let fromRoot: number;
      if (group === "bell")       fromRoot = t * 0.5;
      else if (group === "outer") fromRoot = 0.5 + t * 0.5;
      else                        fromRoot = 1 - t;

      // ── Pre-ignition warm glow ──────────────────────────────────────────────

      if (jellyElapsed <= 0) {
        const preProgress = Math.max(0, Math.min(1,
          (elapsed - (iTime - PRE_IGNITION_WINDOW)) / PRE_IGNITION_WINDOW
        ));
        if (preProgress > 0) {
          const preScale     = BASE_SCALE * preProgress * preProgress;
          const segmentAlpha = Math.max(0, Math.min(1, (preScale - fromRoot) / FADE_WIDTH));
          if (segmentAlpha > 0) {
            const base      = sampleBlend(gradient, greenGradient, gradPos, greenProgress);
            const [r, g, b] = blueProgress > 0
              ? sampleBlend(greenGradient, blueGradient, gradPos, blueProgress)
              : base;
            led.color.setRGB(r, g, b);
            led.intensity = preProgress * PRE_IGNITION_INTENSITY * segmentAlpha;
          } else {
            led.color.setRGB(0, 0, 0);
            led.intensity = 0;
          }
        } else {
          led.color.setRGB(0, 0, 0);
          led.intensity = 0;
        }
        continue;
      }

      // ── Fire ────────────────────────────────────────────────────────────────

      const growProgress = Math.min(1, Math.max(0, jellyElapsed - KINDLE_DURATION) / GROW_DURATION);
      const currentScale = BASE_SCALE + (MAX_SCALE - BASE_SCALE) * growProgress;

      // Per-jelly energy: ramps once this jelly finishes growing.
      // Only affects strip scale variation — same visual as the initial grow/shrink,
      // but applied independently per arm and becoming more dramatic over time.
      const energyProgress = Math.max(0, Math.min(1, (jellyElapsed - ENERGY_START) / ENERGY_DURATION));
      const jellyStripAmp  = STRIP_AMP  + (MAX_ENERGY_STRIP  - STRIP_AMP)  * energyProgress;
      const jellyShrinkMax = SHRINK_MAX + (MAX_ENERGY_SHRINK - SHRINK_MAX) * energyProgress;

      const stripIndex = Math.floor(led.id / 50);
      const stripPhase = stripIndex * PHI;
      // Three harmonics for more organic, less periodic arm movement
      const stripPulse = jellyStripAmp * (
        0.5 * Math.sin(time * STRIP_FREQ + stripPhase) +
        0.3 * Math.sin(time * STRIP_FREQ * 2.1 + stripPhase * 1.6) +
        0.2 * Math.sin(time * STRIP_FREQ * 3.7 + stripPhase * 0.9)
      );

      const floor          = Math.max(BASE_SCALE, currentScale - jellyShrinkMax);
      const effectiveScale = Math.min(MAX_SCALE, Math.max(floor, currentScale + globalBreath + stripPulse));
      const segmentAlpha   = Math.max(0, Math.min(1, (effectiveScale - fromRoot) / FADE_WIDTH));

      if (segmentAlpha === 0) {
        led.color.setRGB(0, 0, 0);
        led.intensity = 0;
        continue;
      }

      const wave      = WOBBLE_AMP * Math.sin(gradPos * WOBBLE_FREQ - time * WAVE_SPEED);
      const wavePos   = gradPos + wave;
      const [r, g, b] = blueProgress > 0
        ? sampleBlend(greenGradient, blueGradient, wavePos, blueProgress)
        : sampleBlend(gradient,      greenGradient, wavePos, greenProgress);

      // Kindle: ramp flicker and brightness from pre-glow level → full fire.
      // After kindle, jellyFlickerAmp (energy-driven) takes over gradually.
      const kindleProgress = Math.min(1, jellyElapsed / KINDLE_DURATION);
      const flickerAmp     = FLICKER_AMP * kindleProgress;
      const flicker        = 1 - flickerAmp * (0.5 + 0.5 * Math.sin(time * FLICKER_FREQ + led.id * FLICKER_PHASE));
      const baseIntensity  = PRE_IGNITION_INTENSITY + (1 - PRE_IGNITION_INTENSITY) * kindleProgress;

      led.color.setRGB(r, g, b);
      led.intensity = baseIntensity * flicker * segmentAlpha;
    }
  },
};
