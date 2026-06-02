import { LED } from "../core/ledSystem";
import { LEDAnimation } from "./types";
import { getLEDDescriptor } from "../ledMap";

const TRAIL = 0.15;
const SPEED = 0.4;

export const starChase: LEDAnimation = {
  name: "starChase",

  update(leds: LED[], time: number) {
    const head = (time * SPEED) % 1;

    for (const led of leds) {
      const { tPerim } = getLEDDescriptor(led.id);
      const diff = (head - tPerim + 1) % 1;
      const intensity = diff < TRAIL ? 1 - diff / TRAIL : 0;

      led.color.setRGB(1, 0.85, 0.1);
      led.intensity = intensity;
    }
  },
};
