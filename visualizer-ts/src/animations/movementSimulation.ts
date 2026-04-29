import { LED } from "../core/ledSystem";

const SPEED = 0.6;
const WAVE_SOFTNESS = 0.1;

const PAUSE_FULL = 0.4;


const COLOR_R = 0.0;
const COLOR_G = 1.0;
const COLOR_B = 0.0;

export const movementSimulation = {
  name: "movementSimulation",

  update(leds: LED[], time: number) {
    const sweepDuration = 1.0 / SPEED;

    // shorter cycle (no OFF phase)
    const cycleDuration = (sweepDuration * 2) + PAUSE_FULL;
    const cycleTime = time % cycleDuration;

    const tentacleStart = 0.4; //  only bottom part moves

    for (const led of leds) {
      const t = led.t ?? 0;

      let intensity = 1.0; // default always ON

      //  ONLY animate tentacles
      if (t > tentacleStart) {
        const localProgress = (t - tentacleStart) / (1.0 - tentacleStart);

        if (cycleTime < sweepDuration) {
          // --- FILLING ---
          const waveFront = cycleTime * SPEED;
          intensity = Math.min(
            1.0,
            Math.max(0.0, (waveFront - localProgress) / WAVE_SOFTNESS)
          );

        } else if (cycleTime < sweepDuration + PAUSE_FULL) {
          // --- FULL ---
          intensity = 1.0;

        } else {
          // --- DRAINING ---
          const drainTime = cycleTime - sweepDuration - PAUSE_FULL;
          const waveFront = drainTime * SPEED;

          intensity = Math.min(
            1.0,
            Math.max(0.0, (1.0 - localProgress - waveFront) / WAVE_SOFTNESS)
          );
        }

        // keep tentacles from going completely dark
        intensity = 0.3 + (0.7 * intensity);
      }

      //  HEAD stays fully ON
      led.intensity = intensity;
      led.color.setRGB(COLOR_R, COLOR_G * intensity, COLOR_B);
    }
  },
};