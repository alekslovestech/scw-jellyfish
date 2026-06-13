import { LED } from "../core/ledSystem";
import { LEDAnimation } from "./types";
import { getLEDDescriptor } from "../ledMap";
import { cfg } from "../config";

const POINT_DURATION = 0.4;

export const starPoints: LEDAnimation = {
  name: "starPoints",

  update(leds: LED[], time: number) {
    const cycle = (time % (cfg.NUM_POINTS * POINT_DURATION)) / POINT_DURATION;
    const activePoint = Math.floor(cycle) % cfg.NUM_POINTS;
    const fade = 1 - (cycle % 1);

    for (const led of leds) {
      const { starPoint } = getLEDDescriptor(led.id);
      const isActive = starPoint === activePoint;

      led.color.setRGB(1, 0.9, 0.2);
      led.intensity = isActive ? fade : 0.05;
    }
  },
};
