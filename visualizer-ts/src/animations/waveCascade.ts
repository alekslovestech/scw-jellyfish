import { LED } from "../core/ledSystem";

// ─────────────────────────────────────────────────────────────
// WAVE ANIMATION SETTINGS
// ─────────────────────────────────────────────────────────────

const SPEED = 0.3;
// How fast the wave front moves.

const WAVE_SOFTNESS = 0.2;
// How soft/gradual the wave edge is (0 = hard snap, 0.2 = smooth).

const SCENE_Z_TOP = 0.25;
const SCENE_Z_BOTTOM = -7.80;

const PAUSE_FULL = 0.3;
// How long (seconds) all LEDs stay fully ON before reversing.

const PAUSE_EMPTY = 0.5;
// How long (seconds) all LEDs stay fully OFF before sweeping down again.

const COLOR_R = 0.0;
const COLOR_G = 1.0;
const COLOR_B = 0.1;

// ─────────────────────────────────────────────────────────────

export const waveCascade = {
  name: "waveCascade",

  update(leds: LED[], time: number) {
    const sceneRange = SCENE_Z_TOP - SCENE_Z_BOTTOM;

    const sweepDuration = 1.0 / SPEED;
    const cycleDuration = sweepDuration * 2 + PAUSE_FULL + PAUSE_EMPTY;
    const cycleTime = time % cycleDuration;

    // Four phases:
    // [0]             sweep down  (off → on)
    // [sweepDuration] pause full
    // [+ PAUSE_FULL]  sweep up    (on → off)
    // [+ sweepDuration] pause empty

    let intensity_fn: (ledZ: number) => number;

    if (cycleTime < sweepDuration) {
      // Phase 1: wave sweeps down, LEDs turn on top → bottom
      const waveFront = cycleTime * SPEED;
      intensity_fn = (ledZ) => Math.min(1.0, Math.max(0.0, (waveFront - ledZ) / WAVE_SOFTNESS));

    } else if (cycleTime < sweepDuration + PAUSE_FULL) {
      // Phase 2: all on
      intensity_fn = (_ledZ) => 1.0;

    } else if (cycleTime < sweepDuration * 2 + PAUSE_FULL) {
      // Phase 3: wave sweeps UP turning LEDs off, bottom → top
      const t = cycleTime - sweepDuration - PAUSE_FULL;
      const waveFront = t * SPEED; // 0→1 but from bottom
      intensity_fn = (ledZ) => Math.min(1.0, Math.max(0.0, ((1.0 - ledZ) - waveFront) / WAVE_SOFTNESS));

    } else {
      // Phase 4: all off
      intensity_fn = (_ledZ) => 0.0;
    }

    for (const led of leds) {
      const ledZ = (SCENE_Z_TOP - led.position.z) / sceneRange;
      const intensity = intensity_fn(ledZ);

      led.intensity = intensity;
      led.color.setRGB(
        COLOR_R,
        COLOR_G * intensity,
        COLOR_B
      );
    }
  },
};