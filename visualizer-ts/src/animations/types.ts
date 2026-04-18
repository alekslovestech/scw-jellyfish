import { LED } from "../core/ledSystem";

export type LEDAnimation = {
  name: string;
  update: (leds: LED[], time: number) => void;
};