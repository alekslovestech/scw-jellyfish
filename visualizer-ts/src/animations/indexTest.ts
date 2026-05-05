import { LED } from "../core/ledSystem";
import { LEDSegment, getLEDDescriptor } from "../ledMap";

type RGB = [number, number, number];

// ─────────────────────────────────────────────────────────────────────────────
// BASE COLOR — one per jellyfish (13 total, matches default scene count).
// This color is applied to ALL segments of that jellyfish unless overridden.
// ─────────────────────────────────────────────────────────────────────────────
const JELLY_COLORS: Record<number, RGB> = {
  0:  [1.0, 0.0, 0.0], 
  1:  [0.0, 1.0, 0.0], 
  2:  [0.0, 1.0, 0.0], 
  3:  [0.0, 1.0, 0.0], 
  4:  [0.0, 1.0, 0.0], 
  5:  [0.0, 1.0, 0.0], 
  6:  [0.0, 1.0, 0.0], 
  7:  [0.0, 1.0, 0.0], 
  8:  [0.0, 1.0, 0.0], 
  9:  [0.0, 1.0, 0.0], 
  10: [0.0, 1.0, 0.0], 
  11: [0.0, 1.0, 0.0], 
  12: [1.0, 1.0, 1.0], 
};

// ─────────────────────────────────────────────────────────────────────────────
// SEGMENT OVERRIDES — optional per-jelly, per-segment color.
// Only define entries where you want a color different from the base.
//
// Example: jelly 0 → inner=blue, outer=red, bell=green
// ─────────────────────────────────────────────────────────────────────────────
const SEGMENT_COLORS: Partial<Record<number, Partial<Record<LEDSegment, RGB>>>> = {
  1: {
    inner: [1.0, 0.0, 1.0], 
    outer: [0.0, 1.0, 0.0], 
    bell:  [0.0, 0.0, 1.0], 
  },
  // Add more overrides here:
  // 3: { inner: [1.0, 0.0, 0.5] },
};

// ─────────────────────────────────────────────────────────────────────────────

function getColor(jellyId: number, segment: LEDSegment): RGB {
  return SEGMENT_COLORS[jellyId]?.[segment] ?? JELLY_COLORS[jellyId] ?? [1.0, 1.0, 1.0];
}

export const indexTest = {
  name: "indexTest",

  update(leds: LED[], _time: number) {
    for (const led of leds) {
      const desc = getLEDDescriptor(led.id);
      const [r, g, b] = getColor(desc.jellyId, desc.segment);
      led.color.setRGB(r, g, b);
      led.intensity = 1.0;
    }
  },
};
