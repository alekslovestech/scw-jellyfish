import { hash01 } from "../hash";

// Shared colour-cycle machinery used by both colorCycle (2-colour bases) and
// fireColorCycle (3-colour fireSteady). Keeps the named cycles and the smooth
// cross-fade math in one place.

export type RGB = { r: number; g: number; b: number };

// Named cycles the user can pick from (plus "random", handled specially).
export const CYCLES: Record<string, RGB[]> = {
  rainbow: [
    { r: 1, g: 0, b: 0 },
    { r: 1, g: 0.5, b: 0 },
    { r: 1, g: 1, b: 0 },
    { r: 0, g: 1, b: 0 },
    { r: 0, g: 1, b: 1 },
    { r: 0, g: 0, b: 1 },
    { r: 0.6, g: 0, b: 1 },
  ],
  fire: [
    { r: 1, g: 0, b: 0 },
    { r: 1, g: 0.4, b: 0 },
    { r: 1, g: 0.9, b: 0 },
  ],
  ocean: [
    { r: 0, g: 0.8, b: 0.75 },
    { r: 0, g: 0.75, b: 1 },
    { r: 0, g: 0.15, b: 1 },
    { r: 0.1, g: 0, b: 0.6 },
  ],
  neon: [
    { r: 1, g: 0, b: 1 },
    { r: 0, g: 1, b: 1 },
    { r: 0.6, g: 1, b: 0 },
    { r: 0.55, g: 0, b: 0.95 },
  ],
};

// Dropdown options for the panel: every named cycle, plus "random".
export const CYCLE_NAMES = [...Object.keys(CYCLES), "random"];

// A stable pseudo-random colour for a given step (so "random" cross-fades
// smoothly rather than flickering every frame).
function randomColor(step: number): RGB {
  return { r: hash01(step, 0, 31), g: hash01(step, 1, 31), b: hash01(step, 2, 31) };
}

function colorAt(cycle: string, step: number): RGB {
  if (cycle === "random") return randomColor(step);
  const list = CYCLES[cycle] ?? CYCLES.rainbow;
  return list[((step % list.length) + list.length) % list.length];
}

function lerp(a: RGB, b: RGB, f: number): RGB {
  return { r: a.r + (b.r - a.r) * f, g: a.g + (b.g - a.g) * f, b: a.b + (b.b - a.b) * f };
}

// Cross-faded colour for the cycle at `time`, shifted by `stepOffset` whole steps.
// stepOffset lets callers read several consecutive colours (e.g. the 3 colours of
// the fire gradient) that all advance together.
export function cycleColor(cycle: string, transition: number, time: number, stepOffset: number): RGB {
  const T = Math.max(0.1, transition);
  const base = Math.floor(time / T);
  const frac = time / T - base;
  const step = base + stepOffset;
  return lerp(colorAt(cycle, step), colorAt(cycle, step + 1), frac);
}

// Write into a palette slot in place, so the object identity the base and the
// panel already hold stays stable.
export function setRGB(target: RGB, src: RGB): void {
  target.r = src.r;
  target.g = src.g;
  target.b = src.b;
}
