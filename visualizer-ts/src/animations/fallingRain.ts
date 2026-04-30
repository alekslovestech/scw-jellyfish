import { LED } from "../core/ledSystem";

// ─────────────────────────────────────────────────────────────
// RAIN ANIMATION SETTINGS
// ─────────────────────────────────────────────────────────────

const SPEED = 1.0;
// How fast drops fall. Higher = faster.

const TRAIL_LENGTH = 0.2;
// Drop tail length as a fraction of the scene (0 to 1).

const NUM_DROPS = 20;
// Simultaneous drops per LED column.

const SPEED_VARIANCE = 0.6;
// Speed variation between drops. 0 = uniform, 1 = chaotic.

const DROP_SPACING = 0.5;
// Distribution spread of drops across the scene.

const SCENE_Z_TOP = 0.25;
// Top of the scene (close to camera, where rain enters).

const SCENE_Z_BOTTOM = -7.80;
// Bottom of the scene (away from camera, where rain exits).

const PAUSE_DURATION = .2;
// How long (seconds) the scene holds after all drops land before resetting.

const COLOR_R = 0.0;
const COLOR_G = 0.2;
const COLOR_B = 1.0;

// ─────────────────────────────────────────────────────────────

export const fallingRain = {
  name: "fallingRain",

  update(leds: LED[], time: number) {
    const sceneRange = SCENE_Z_TOP - SCENE_Z_BOTTOM; // positive number

    const minSpeedFactor = Math.max(0.01, 1.0 - SPEED_VARIANCE / 2);
    const slowestDropSpeed = SPEED * minSpeedFactor;
    const cycleDuration = (1.0 + TRAIL_LENGTH) / slowestDropSpeed + PAUSE_DURATION;
    const cycleTime = time % cycleDuration;

    for (const led of leds) {
      // seed uses X and Y (the horizontal plane)
      const seed = ((led.position.x * 9.7 + led.position.y * 7.3) % 1 + 1) % 1;

      let intensity = 0;

      // normalize Z: 0 = top (SCENE_Z_TOP), 1 = bottom (SCENE_Z_BOTTOM)
      // rain falls from high Z toward low Z (toward camera away)
      const ledZ = (SCENE_Z_TOP - led.position.z) / sceneRange;

      for (let d = 0; d < NUM_DROPS; d++) {
        const dropSeed = (seed + d * DROP_SPACING) % 1;
        const dropSpeed = SPEED * (1.0 - SPEED_VARIANCE / 2 + dropSeed * SPEED_VARIANCE);
        const dropStartOffset = dropSeed * DROP_SPACING;
        const dropFront = (cycleTime - dropStartOffset) * dropSpeed / (1.0 + TRAIL_LENGTH);

        if (dropFront < 0) continue;

        const clampedFront = Math.min(dropFront, 1.0);
        const diff = ledZ - clampedFront;

        if (diff > -TRAIL_LENGTH && diff <= 0) {
          const dropIntensity = 1 - Math.abs(diff) / TRAIL_LENGTH;
          intensity = Math.max(intensity, dropIntensity);
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