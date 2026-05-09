import { LED } from "../core/ledSystem";
import { getLEDDescriptor } from "../ledMap";

// ─────────────────────────────────────────────────────────────
// RAINBOW WAVE SETTINGS
// ─────────────────────────────────────────────────────────────

const SPIN_SPEED = 0.08;
// How fast the rainbow rotates around the jellyfish radially.
// Try: 0.03 (glacial) → 0.3 (fast spin)

const FLOW_SPEED = 0.4;
// How fast the rainbow flows along each tentacle (tip → root).
// Try: 0.1 (slow trickle) → 1.0 (fast rush)

const JELLY_OFFSET = 0.07;
// Hue offset between adjacent jellyfish.
// Try: 0.0 (all in sync) → 0.5 (opposite phase)

const ANGLE_SPREAD = 0.35;
// How much of the hue wheel a full 360° sweep covers.
// 1.0 = full spectrum per rotation, 0.35 = a narrower arc.

const PULSE_SPEED = 1.2;
// Speed of the brightness pulse (breathe effect).

const PULSE_DEPTH = 0.25;
// How much the pulse dims at its trough (0 = no dim, 1 = goes dark).

// ─────────────────────────────────────────────────────────────

export const rainbowWave: { name: string; update: (leds: LED[], time: number) => void } = {
  name: "rainbowWave",

  update(leds: LED[], time: number) {
    for (const led of leds) {
      const d = getLEDDescriptor(led.id);

      // Radial hue: different strips (angles) get different hues.
      const angleHue = (d.angle_deg / 360) * ANGLE_SPREAD;

      // Flow hue: rainbow drifts along tentacle length over time.
      // Inner tentacles: t=0 at tip, reverse so colors flow tip→root.
      // Outer tentacles: t=0 at root already.
      const flowT = d.segment === "inner" ? 1 - d.t : d.t;
      const flowHue = flowT * 0.5;

      // Per-jelly offset so neighboring jellies are slightly phase-shifted.
      const jellyHue = d.jellyId * JELLY_OFFSET;

      // Time drift: slow global spin + faster tentacle flow.
      const timeSpin = time * SPIN_SPEED;
      const timeFlow = flowT * time * FLOW_SPEED * 0.1;

      const hue = (angleHue + flowHue + jellyHue + timeSpin + timeFlow) % 1.0;

      // Gentle brightness pulse so the whole installation breathes.
      const pulse = 1.0 - PULSE_DEPTH * (0.5 + 0.5 * Math.sin(time * PULSE_SPEED * Math.PI * 2));

      led.color.setHSL(hue, 1.0, 0.5 * pulse);
      led.intensity = pulse;
    }
  },
};
