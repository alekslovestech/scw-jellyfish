import { LED } from "../core/ledSystem";
import { LEDAnimation } from "./types";
import { getLEDDescriptor } from "../ledMap";

const SPEED         = 0.3;
const SPEED_VAR     = 0.6;
const NUM_DROPS     = 4;
const DROP_SPACING  = 0.25;
const TRAIL_LENGTH  = 0.1;

export const starRain: LEDAnimation = {
  name: "starRain",

  update(leds: LED[], time: number) {
    for (const led of leds) {
      const { tPerim } = getLEDDescriptor(led.id);

      let intensity = 0;

      for (let d = 0; d < NUM_DROPS; d++) {
        const dropSeed   = (d * 0.618033) % 1;
        const dropSpeed  = SPEED * (1 - SPEED_VAR / 2 + dropSeed * SPEED_VAR);
        const dropOffset = (dropSeed + d * DROP_SPACING) % 1;
        const dropFront  = (dropOffset + time * dropSpeed) % 1;

        const diff = dropFront - tPerim;
        if (diff > 0 && diff < TRAIL_LENGTH) {
          const frac = 1 - diff / TRAIL_LENGTH;
          intensity = Math.max(intensity, frac);
        }
      }

      // Flicker: small high-freq noise on top
      const flicker = 0.85 + 0.15 * Math.sin(time * 60 + led.id * 7.3);

      led.color.setRGB(0, 0.15, 1);
      led.intensity = intensity * flicker;
    }
  },
};
