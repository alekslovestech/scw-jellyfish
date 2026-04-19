import { LED } from "../core/ledSystem";

// ─────────────────────────────────────────────────────────────
// RAIN ANIMATION SETTINGS — tweak these to change the effect
// ─────────────────────────────────────────────────────────────

const SPEED = 1.0;
// Base falling speed of the drops. Higher = faster rain.
// Try: 1.0 (slow drizzle) → 5.0 (heavy storm)

const TRAIL_LENGTH = 0.6;
// How long each drop tail is, as a fraction of the scene (0 to 1).
// Try: 0.05 (short sharp drops) → 0.4 (long streaks)

const NUM_DROPS = 30;
// How many drops fall simultaneously per LED column.
// Higher = denser rain, less gaps.
// Try: 3 (sparse) → 20 (torrential)

const SPEED_VARIANCE = 0.9;
// How much each drop's speed varies from the base speed.
// 0.0 = all drops fall at the same speed (looks mechanical)
// 1.0 = drops vary wildly in speed (looks chaotic)
// Try: 0.2 (subtle) → 0.9 (very random)

const DROP_SPACING = 0.5;
// Controls how evenly drops are distributed across the scene.
// Irrational-ish numbers give the most natural spread.
// Try: 0.137 (natural) → 0.5 (evenly spaced, looks uniform)

const SCENE_Z_MIN = -10;
// The minimum Z coordinate in your scene (top of rain).
// Match this to your actual scene. Current setup: levels at 0, -3, -6.
// Try: -5 (shallow) → -20 (deep scene)

const SCENE_Z_MAX = 5;
// The maximum Z coordinate in your scene (bottom of rain).
// Try: 5 → 20

const COLOR_R = 0.0;

const COLOR_G = 0.3;
// Green channel — gives a cyan tint to the blue.
// Try: 0.0 (pure blue) → 0.8 (teal/cyan)

const COLOR_B = 1.0;
// Blue channel. Keep at 1.0 for rain, lower for other effects.

// ─────────────────────────────────────────────────────────────

export const fallingRain = {
  name: "fallingRain",

  update(leds: LED[], time: number) {
    const sceneRange = SCENE_Z_MAX - SCENE_Z_MIN;

    for (const led of leds) {
      const seed = ((led.position.x * 9.7 + led.position.y * 7.3) % 1 + 1) % 1;

      let intensity = 0;

      for (let d = 0; d < NUM_DROPS; d++) {
        const dropSeed = (seed + d * DROP_SPACING) % 1;
        const dropSpeed = SPEED * (1.0 - SPEED_VARIANCE / 2 + dropSeed * SPEED_VARIANCE);
        const dropPhase = (dropSeed + time * dropSpeed) % 1;
        const dropFront = dropPhase;
        const ledZ = (led.position.z - SCENE_Z_MIN) / sceneRange;
        const diff = ledZ - dropFront;

        if (diff > 0 && diff < TRAIL_LENGTH) {
          intensity = Math.max(intensity, 1 - diff / TRAIL_LENGTH);
        }
      }

      led.intensity = intensity;
      led.color.setRGB(
        COLOR_R,
        COLOR_G * intensity,
        COLOR_B
      );
    }
  },
};