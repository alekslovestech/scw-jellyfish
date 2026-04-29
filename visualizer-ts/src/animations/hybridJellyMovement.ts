import { LED } from "../core/ledSystem";

const SPEED = 0.8;
const WAVE_SOFTNESS = 0.15;
const PAUSE_FULL = 0.2;
const INNER_SPEED = 6;

export const hybridJellyMovement = {
  name: "hybridJellyMovement",

  update(leds: LED[], time: number) {
    const sweepDuration = 1.0 / SPEED;
    const cycleDuration = (sweepDuration * 2) + PAUSE_FULL;
    const cycleTime = time % cycleDuration;

    for (const led of leds) {
      const t = led.t ?? 0;
      const id = led.jellyId ?? 0;

      // ---------------------------
      // 1. BASE BODY (green wave)
      // ---------------------------
      let baseIntensity = 1.0;

      const tentacleStart = 0.4;

      if (t > tentacleStart) {
        const localProgress = (t - tentacleStart) / (1.0 - tentacleStart);

        if (cycleTime < sweepDuration) {
          const waveFront = cycleTime * SPEED;
          baseIntensity = Math.min(
            1.0,
            Math.max(0.0, (waveFront - localProgress) / WAVE_SOFTNESS)
          );

        } else if (cycleTime < sweepDuration + PAUSE_FULL) {
          baseIntensity = 1.0;

        } else {
          const drainTime = cycleTime - sweepDuration - PAUSE_FULL;
          const waveFront = drainTime * SPEED;

          baseIntensity = Math.min(
            1.0,
            Math.max(0.0, (1.0 - localProgress - waveFront) / WAVE_SOFTNESS)
          );
        }

        // slightly reduced so inner effect stands out
        baseIntensity = 0.2 + (0.8 * baseIntensity);
      }

      // ---------------------------
      // 2. OUTER SYSTEM (cool breathing glow)
      // ---------------------------
      const pulse = Math.sin(time * 2 + id) * 0.5 + 0.5;

      if (led.group === "bell" || led.group === "tentacle") {
        led.color.setRGB(
          pulse * 0.75 + baseIntensity * 0.6,
          0,
          0,
        );
      }

      // ---------------------------
      // 3. INNER SYSTEM (bright nerve worm)
      // ---------------------------
      else if (led.group === "central") {
        const wormPosition = t;
        const speed = time * INNER_SPEED;

        const wave = Math.sin(wormPosition * 10 - speed);

        // keep only positive signal
        const raw = Math.max(0, wave);

        // flicker / organic noise
        const noise = Math.sin(time * 12 + id * 10) * 0.15 + 0.85;

        const intensity = raw * noise;

        // boost visibility
        const innerGlow = intensity * 3;

        led.color.setRGB(
          0,         // strong red core
          0,   // slight warmth
          innerGlow,
        );
      }

      // ---------------------------
      // 4. DEFAULT (fallback glow)
      // ---------------------------
      else {
        led.color.setRGB(
          0,
          baseIntensity,
          0
        );
      }

      led.intensity = baseIntensity;
    }
  },
};