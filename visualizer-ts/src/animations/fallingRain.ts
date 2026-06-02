import { LED } from "../core/ledSystem";
import { LEDAnimation } from "./types";

// ── Tuning ────────────────────────────────────────────────────────────────────

const SPEED          = 0.9;  // vertPos units / second
const SPEED_VARIANCE = 0.75; // high variance → drops fall at very different speeds
const NUM_DROPS      = 7;    // more simultaneous drops per strip = dense rain
const DROP_SPACING   = 0.13; // tighter spacing between drops
const TRAIL_LENGTH   = 0.08; // short sharp drops

// Pure blue — no white component
const HEAD_R = 0.0;
const HEAD_G = 0.15;
const HEAD_B = 1.0;

// ── Helpers ───────────────────────────────────────────────────────────────────

// vertPos: 0 = bell center (top), 1 = tentacle tip (bottom).
// Bell + outer form one continuous path; inner is its own path.
function getVertPos(group: string, t: number): number {
  if (group === "bell")  return t * 0.5;       // center(t=0)→rim(t=1) maps to 0→0.5
  if (group === "outer") return 0.5 + t * 0.5; // root(t=0)→tip(t=1) maps to 0.5→1
  return 1 - t;                                 // inner root(t=1)=0, tip(t=0)=1
}

// ── Animation ─────────────────────────────────────────────────────────────────

export const fallingRain: LEDAnimation = {
  name: "fallingRain",

  update(leds: LED[], time: number) {
    for (const led of leds) {
      const group  = led.group ?? "inner";
      const t      = led.t ?? 0;
      const vPos   = getVertPos(group, t);

      // Each physical strip (50 LEDs) is one independent rain column.
      // Golden-ratio spread ensures no two strips share the same phase.
      const stripIndex = Math.floor(led.id / 50);
      const stripSeed  = (stripIndex * 0.618033) % 1;

      let intensity  = 0;
      let headFrac   = 0; // 1 at drop head, 0 at tail — drives colour brightening

      for (let d = 0; d < NUM_DROPS; d++) {
        // Each drop gets its own seed → independent speed variation
        const dropSeed   = (stripSeed + d * 0.618033) % 1;
        const dropSpeed  = SPEED * (1 - SPEED_VARIANCE / 2 + dropSeed * SPEED_VARIANCE);
        const dropOffset = (dropSeed + d * DROP_SPACING) % 1;
        // Front descends from 0→1 (bell→tip) and loops
        const dropFront  = (dropOffset + time * dropSpeed) % 1;

        // diff > 0: this LED is above the drop front (in the trail)
        // diff ∈ (0, TRAIL_LENGTH): LED is lit
        const diff = dropFront - vPos;
        if (diff > 0 && diff < TRAIL_LENGTH) {
          const frac = 1 - diff / TRAIL_LENGTH; // 1 at head, 0 at tail
          if (frac > intensity) {
            intensity = frac;
            headFrac  = frac;
          }
        }
      }

      if (intensity === 0) {
        led.color.setRGB(0, 0, 0);
        led.intensity = 0;
        continue;
      }

      // Head is bright white-cyan, trail fades to darker blue
      led.color.setRGB(
        HEAD_R * headFrac,
        HEAD_G * headFrac,
        HEAD_B
      );
      led.intensity = intensity;
    }
  },
};
