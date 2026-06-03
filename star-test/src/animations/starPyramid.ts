import { LED } from "../core/ledSystem";
import { LEDAnimation } from "./types";
import { getLEDDescriptor } from "../ledMap";
import { cfg } from "../config";

const POINT_COLORS: [number, number, number][] = [
  [1.0, 0.0, 0.0],  // point 0 — red
  [0.0, 1.0, 0.0],  // point 1 — green
  [0.2, 0.5, 1.0],  // point 2 — blue
  [1.0, 1.0, 0.0],  // point 3 — yellow
  [1.0, 0.0, 1.0],  // point 4 — magenta
];

const FILL_SPEED = 0.4;  // full expand/contract cycle in 2.5s each way

export const starPyramid: LEDAnimation = {
  name: "starPyramid",

  update(leds: LED[], time: number) {
    const maxLevel = cfg.LEDS_PER_EDGE - 1;  // 0..4

    // Triangle wave 0 → maxLevel → 0
    const cycle = (time * FILL_SPEED) % 2;
    const fillLevel = cycle < 1
      ? cycle * maxLevel
      : (2 - cycle) * maxLevel;

    for (const led of leds) {
      const { edge, posInEdge, starPoint } = getLEDDescriptor(led.id);

      // Distance from the outer tip along this arm (0 = closest to tip)
      const distFromTip = edge % 2 === 0
        ? posInEdge
        : cfg.LEDS_PER_EDGE - 1 - posInEdge;

      // Smooth leading edge
      const intensity = Math.max(0, Math.min(1, fillLevel - distFromTip + 0.5));

      const [r, g, b] = POINT_COLORS[starPoint];
      led.color.setRGB(r, g, b);
      led.intensity = intensity;
    }
  },
};
