import { starRain } from "./animations/starRain";
import { LEDAnimation } from "./animations/types";

export const cfg = {
  R_OUTER: 2.0,
  R_INNER: 0.76,
  LEDS_PER_EDGE: 5,
  NUM_EDGES: 10,
  NUM_POINTS: 5,
  colors: {
    background: 0x000010,
  },
  animation: starRain as LEDAnimation,
};
