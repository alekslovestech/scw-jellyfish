import { LED } from "../core/ledSystem";

const SPEED = 0.5;          // Higher = faster fill
const WAVE_SOFTNESS = 0.1;  // Blurriness of the wave edge

const PAUSE_FULL = 0.3;     // Wait while all are lit
const PAUSE_EMPTY = 0.3;    // Wait while all are dark

const COLOR_R = 0.0;
const COLOR_G = 1.0;
const COLOR_B = 0.0;

export const synchronizedJellyWave = {
  name: "synchronizedJellyWave",

  update(leds: LED[], time: number) {
    // 1. TIMING LOGIC
    // We define how long one "sweep" takes (0.0 to 1.0)
    const sweepDuration = 1.0 / SPEED;
    const cycleDuration = (sweepDuration * 2) + PAUSE_FULL + PAUSE_EMPTY;
    const cycleTime = time % cycleDuration;

    for (const led of leds) {
      // 2. THE SECRET INGREDIENT: led.t
      // led.t is 0.0 at the top of the bell and 1.0 at the bottom of the tentacles.
      // Because we use this instead of led.position.z, every jelly behaves identically.
      const localProgress = led.t ?? 0;
      
      let intensity: number = 0;

      // 3. PHASE LOGIC
      if (cycleTime < sweepDuration) {
        // --- PHASE 1: FILLING ---
        const waveFront = cycleTime * SPEED;
        intensity = Math.min(1.0, Math.max(0.0, (waveFront - localProgress) / WAVE_SOFTNESS));

      } else if (cycleTime < sweepDuration + PAUSE_FULL) {
        // --- PHASE 2: ALL ON ---
        intensity = 1.0;

      } else if (cycleTime < (sweepDuration * 2) + PAUSE_FULL) {
        // --- PHASE 3: DRAINING ---
        const drainTime = cycleTime - sweepDuration - PAUSE_FULL;
        const waveFront = drainTime * SPEED;
        // This makes the light retreat from the head down to the tails
        intensity = Math.min(1.0, Math.max(0.0, (1.0 - localProgress - waveFront) / WAVE_SOFTNESS));

      } else {
        // --- PHASE 4: ALL OFF ---
        intensity = 0.0;
      }

      // 4. APPLY TO LED
      led.intensity = intensity;
      led.color.setRGB(COLOR_R, COLOR_G * intensity, COLOR_B);
    }
  },
};