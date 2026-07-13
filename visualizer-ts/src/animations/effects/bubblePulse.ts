import * as THREE from "three";
import { Effect } from "../types";
import { LED } from "../../core/ledSystem";

// --- BUBBLE PULSE ---
// An overlay effect that behaves like innerSpreadWave — a bright travelling crest
// with dark between the pulses — but the crest travels RADIALLY: it sweeps OUT
// from the middle of the installation to the edge and back IN, over and over,
// instead of up the tentacles. You see a glowing spherical shell swell outward
// through the jellies and then draw back in.
//
// The breath is ASYMMETRIC: a quick swell out, a slow relaxation back
// (expandTime < contractTime), the way a jellyfish bell contracts fast and
// relaxes slowly. Two knobs shape how far the shell travels, as fractions of the
// installation's radius (0 = centre, 1 = outer edge):
//   peak (X)    — the outermost radius the shell reaches.
//   contract (Y)— how far it pulls back in from the peak each breath.
// The shell breathes between trough = max(0, X - Y) and peak = X. So Y = X means
// it fully collapses to the centre and is reborn each breath; Y < X leaves a
// residual ring at radius X - Y that keeps breathing.
//
// `wobble` warps the radius per-direction so the shell is a lopsided, undulating
// blob instead of a perfect sphere — that's what sells "organic".
//
// All radii are NORMALISED to the installation's own size (measured from the LED
// positions), so it always sweeps the whole thing regardless of world scale.
//
// HARDWARE TODO: this needs a 3-D position per LED (distance-from-centre). The
// visualizer has led.position; the Arduino firmware addresses LEDs by strip/
// segment index. To port this, the firmware needs a per-LED coordinate table
// (or a distance approximation derived from the strip layout).

const params = {
  peak: 1.0, // X — outermost radius reached (0–1 of installation radius)
  contract: 1.0, // Y — how far it pulls back (Y = peak → full collapse to centre)
  expandTime: 1.2, // s — fast swell outward
  contractTime: 3.5, // s — slow relaxation back in (keep > expandTime)
  thickness: 0.18, // W — shell half-width as a fraction of radius (fatter = softer)
  wobble: 0.12, // radius warp per direction (fraction of radius); 0 = perfect sphere
  brightness: 1, // strength of the crest (0–1)
};

// Fixed bubble colour — a cool white-cyan.
const BUBBLE = { r: 0.6, g: 0.9, b: 1.0 };

// Smoothstep 0→1 (clamped) — eases each half of the breath so the turnarounds at
// the trough and the peak aren't abrupt.
function ease(x: number): number {
  const t = Math.min(1, Math.max(0, x));
  return t * t * (3 - 2 * t);
}

// Installation centre (centroid of all LEDs) + its max radius, so every distance
// can be normalised to 0..1. Computed once and re-computed if the LED set changes
// (e.g. after a rebuild). Centroid keeps the shell symmetric "from the middle".
let center = new THREE.Vector3();
let maxR = 1;
let cachedCount = -1;
function measure(leds: LED[]): void {
  if (leds.length === cachedCount) return;
  center.set(0, 0, 0);
  for (const led of leds) center.add(led.position);
  if (leds.length) center.divideScalar(leds.length);
  maxR = 0;
  for (const led of leds) maxR = Math.max(maxR, led.position.distanceTo(center));
  if (maxR < 1e-4) maxR = 1;
  cachedCount = leds.length;
}

// The breathing shell radius (normalised 0..1) at this instant. Oscillates between
// trough and peak: a fast eased swell out (expandTime), then a slow eased draw
// back in (contractTime). Continuous and periodic across the cycle boundary.
function breathRadius(time: number): number {
  const peak = params.peak;
  const trough = Math.max(0, params.peak - params.contract);
  const span = peak - trough;

  const cycle = params.expandTime + params.contractTime;
  const u = (((time % cycle) + cycle) % cycle);

  if (u < params.expandTime) {
    return trough + span * ease(u / params.expandTime); // fast swell: trough → peak
  }
  return peak - span * ease((u - params.expandTime) / params.contractTime); // slow: peak → trough
}

export const bubblePulse: Effect = {
  name: "bubblePulse",
  enabled: false, // off by default — toggle it on in the panel

  params,
  controls: [
    { key: "peak", label: "expand (X)", min: 0, max: 1, step: 0.05 },
    { key: "contract", label: "contract (Y)", min: 0, max: 1, step: 0.05 },
    { key: "expandTime", label: "expand time (s)", min: 0.1, max: 6, step: 0.1 },
    { key: "contractTime", label: "contract time (s)", min: 0.1, max: 10, step: 0.1 },
    { key: "thickness", label: "shell thickness", min: 0.02, max: 0.5, step: 0.01 },
    { key: "wobble", label: "wobble", min: 0, max: 0.5, step: 0.02 },
    { key: "brightness", label: "brightness", min: 0, max: 1, step: 0.05 },
  ],

  apply(leds, time) {
    measure(leds);
    const R = breathRadius(time); // normalised shell radius this frame
    const invW = 1 / Math.max(0.01, params.thickness);

    for (const led of leds) {
      const d = led.position.distanceTo(center) / maxR; // normalised distance 0..1

      // Per-direction radius warp: a few sinusoids of the normalised direction
      // that drift with time, so the shell bulges/dimples and undulates as it
      // breathes. Range ≈ ±wobble. Cheap and Arduino-portable (no noise tables).
      const nx = (led.position.x - center.x) / maxR;
      const ny = (led.position.y - center.y) / maxR;
      const nz = (led.position.z - center.z) / maxR;
      const wob =
        (params.wobble / 3) *
        (Math.sin(nx * 8.0 + time * 0.7) +
          Math.sin(ny * 10.0 - time * 0.9) +
          Math.sin(nz * 9.0 + time * 0.5));

      // Sharp travelling crest (like innerSpreadWave's worm): bright on the shell,
      // dark off it. A Gaussian in normalised-distance gives the soft-edged band.
      const off = (d + wob - R) * invW;
      const crest = params.brightness * Math.exp(-off * off);
      if (crest <= 0.01) continue; // off the shell — leave the base untouched

      // Assert like innerSpreadWave: at the crest, take over the colour fully and
      // drive the brightness. Between shells (crest→0) the base shows through.
      led.color.setRGB(
        led.color.r + (BUBBLE.r - led.color.r) * crest,
        led.color.g + (BUBBLE.g - led.color.g) * crest,
        led.color.b + (BUBBLE.b - led.color.b) * crest,
      );
      led.intensity = Math.max(led.intensity, crest);
    }
  },
};
