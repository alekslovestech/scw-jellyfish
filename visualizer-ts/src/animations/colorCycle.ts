import { LED } from "../core/ledSystem";

export const colorCycle = {
  name: "colorCycle",

  update(leds: LED[], time: number) {
    const t = (time / 3) % 1; // full cycle every 3 seconds

    for (const led of leds) {
      const phase = t * 3;
      led.color.setRGB(
        phase < 1 ? 1 - phase : phase < 2 ? 0 : phase - 2, // R
        phase < 1 ? phase : phase < 2 ? 2 - phase : 0,     // G
        phase < 1 ? 0 : phase < 2 ? phase - 1 : 3 - phase  // B
      );
    }
  },
};