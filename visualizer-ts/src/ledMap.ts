import { cfg } from "./config";

// ─────────────────────────────────────────────────────────────────────────────
// LED Map — physical layout reference
//
// JELLY 0 (hero) — 16 strips × 50 LEDs = 800 LEDs  (IDs 0–799)
//
//   Strips 0–7   → inner only
//     [ INNER TENTACLE (50 LEDs)                                      ]
//       pos 0 = tip (bottom)  →  pos 49 = root (at bell)
//
//   Strips 8–15  → bell + outer
//     [ BELL (25 LEDs)          ][ OUTER TENTACLE (25 LEDs)           ]
//       pos 0 = inner edge         pos 25 = root (bell rim)
//       pos 24 = outer rim         pos 49 = tip (bottom)
//
// STANDARD JELLIES (1–12) — 8 strips × 50 LEDs = 400 LEDs each
//   (IDs start at 800 for jelly 1, +400 per jelly after that)
//
//   Each strip:
//     [ INNER (30) ][ BELL (10) ][ OUTER (10) ]
//       pos 0–29      pos 30–39    pos 40–49
//
// Always use getLEDDescriptor(led.id) — never index LED_MAP directly.
// ─────────────────────────────────────────────────────────────────────────────

export type LEDSegment = "inner" | "bell" | "outer";

export type LEDDescriptor = {
  jellyId: number;       // which jellyfish (0 = hero, 1–12 = standard)
  stripIndex: number;    // which strip within that jellyfish (0–15 for jelly 0, 0–7 for others)
  angle_deg: number;     // rotational angle of this strip (0°, 45°, …, 315°)
  posInStrip: number;    // raw position within the strip (0–49)
  segment: LEDSegment;   // "inner" | "bell" | "outer"
  posInSegment: number;  // position within the segment
  t: number;             // normalized 0→1 within the segment
};

// ── Sizing constants ──────────────────────────────────────────────────────────

const JELLY0_TOTAL =
  (cfg.jelly0.inner_strips + cfg.jelly0.bell_outer_strips) * cfg.jelly0.leds_per_strip; // 800

const STANDARD_TOTAL =
  cfg.hardware.strips_per_jelly * cfg.hardware.leds_per_strip; // 400

// ── Standard jelly map (reference for jellies 1–12) ──────────────────────────

function buildStandardMap(): Record<number, LEDDescriptor> {
  const { strips_per_jelly, leds_per_strip, inner_leds, bell_leds, outer_leds } =
    cfg.hardware;
  const map: Record<number, LEDDescriptor> = {};

  for (let s = 0; s < strips_per_jelly; s++) {
    const angle_deg = Math.round((s / strips_per_jelly) * 360);
    const stripBase = s * leds_per_strip;

    for (let j = 0; j < inner_leds; j++) {
      map[stripBase + j] = {
        jellyId: 0,
        stripIndex: s,
        angle_deg,
        posInStrip: j,
        segment: "inner",
        posInSegment: j,
        t: j / (inner_leds - 1),
      };
    }

    for (let j = 0; j < bell_leds; j++) {
      const posInStrip = inner_leds + j;
      map[stripBase + posInStrip] = {
        jellyId: 0,
        stripIndex: s,
        angle_deg,
        posInStrip,
        segment: "bell",
        posInSegment: j,
        t: j / (bell_leds - 1),
      };
    }

    for (let j = 0; j < outer_leds; j++) {
      const posInStrip = inner_leds + bell_leds + j;
      map[stripBase + posInStrip] = {
        jellyId: 0,
        stripIndex: s,
        angle_deg,
        posInStrip,
        segment: "outer",
        posInSegment: j,
        t: j / (outer_leds - 1),
      };
    }
  }

  return map;
}

// Human-readable reference for the standard jelly layout (0–399).
// jellyId is a placeholder — use getLEDDescriptor() for real lookups.
export const LED_MAP: Record<number, LEDDescriptor> = buildStandardMap();

// ── Jelly 0 descriptor builder ────────────────────────────────────────────────

function buildJelly0Descriptor(ledId: number): LEDDescriptor {
  const { inner_strips, bell_outer_strips, leds_per_strip, inner_leds, bell_leds, outer_leds } =
    cfg.jelly0;

  const innerTotal = inner_strips * leds_per_strip; // 400

  if (ledId < innerTotal) {
    // Inner-only strip (strips 0–7)
    const stripIndex   = Math.floor(ledId / leds_per_strip);
    const posInStrip   = ledId % leds_per_strip;
    const angle_deg    = Math.round((stripIndex / inner_strips) * 360);

    return {
      jellyId: 0,
      stripIndex,
      angle_deg,
      posInStrip,
      segment: "inner",
      posInSegment: posInStrip,
      t: posInStrip / (inner_leds - 1),
    };
  }

  // Bell+outer strip (strips 8–15)
  const adjustedId  = ledId - innerTotal;
  const armIndex    = Math.floor(adjustedId / leds_per_strip); // 0–7
  const posInStrip  = adjustedId % leds_per_strip;
  const stripIndex  = inner_strips + armIndex;
  const angle_deg   = Math.round((armIndex / bell_outer_strips) * 360);

  if (posInStrip < bell_leds) {
    return {
      jellyId: 0,
      stripIndex,
      angle_deg,
      posInStrip,
      segment: "bell",
      posInSegment: posInStrip,
      t: posInStrip / (bell_leds - 1),
    };
  }

  const posInSegment = posInStrip - bell_leds;
  return {
    jellyId: 0,
    stripIndex,
    angle_deg,
    posInStrip,
    segment: "outer",
    posInSegment,
    t: posInSegment / (outer_leds - 1),
  };
}

// ── Public lookup ─────────────────────────────────────────────────────────────

export function getLEDDescriptor(ledId: number): LEDDescriptor {
  if (ledId < JELLY0_TOTAL) {
    return buildJelly0Descriptor(ledId);
  }

  const adjusted = ledId - JELLY0_TOTAL;
  const jellyId  = 1 + Math.floor(adjusted / STANDARD_TOTAL);
  return { ...LED_MAP[adjusted % STANDARD_TOTAL], jellyId };
}
