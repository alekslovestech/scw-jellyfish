import { LED } from "../core/ledSystem";
import { LEDAnimation } from "./types";
import { getLEDDescriptor } from "../ledMap";

// heat 0→1: black → deep red → orange → yellow → white-yellow
function fireColor(heat: number): [number, number, number] {
  if (heat < 0.3) {
    const f = heat / 0.3;
    return [f * 0.9, 0, 0];
  } else if (heat < 0.6) {
    const f = (heat - 0.3) / 0.3;
    return [0.9 + f * 0.1, f * 0.45, 0];
  } else if (heat < 0.85) {
    const f = (heat - 0.6) / 0.25;
    return [1.0, 0.45 + f * 0.45, f * 0.1];
  } else {
    const f = (heat - 0.85) / 0.15;
    return [1.0, 0.9 + f * 0.1, 0.1 + f * 0.5];
  }
}

export const starFire: LEDAnimation = {
  name: "starFire",

  update(leds: LED[], time: number) {
    // Slow breath — whole fire rises and falls together
    const cycle = 0.5 + 0.5 * Math.sin(time * 1.4);

    for (const led of leds) {
      const { edge, t } = getLEDDescriptor(led.id);

      // Outer tips = 1, inner corners = 0, edges interpolate
      const tipness = edge % 2 === 0 ? 1 - t : t;

      // Layered flicker — three sin waves at different speeds per LED
      const flicker =
        0.50 * Math.sin(time * 8.1  + led.id * 2.3) +
        0.30 * Math.sin(time * 15.7 + led.id * 5.1) +
        0.20 * Math.sin(time * 27.3 + led.id * 9.7);
      const flickerNorm = 0.5 + 0.5 * flicker; // 0→1

      // Combine: tips dominate, flicker adds chaos, cycle breathes it
      const heat = tipness * 0.55 + flickerNorm * 0.25 + cycle * 0.20;
      const h = Math.max(0, Math.min(1, heat));

      const [r, g, b] = fireColor(h);
      led.color.setRGB(r, g, b);
      led.intensity = h;
    }
  },
};
