import { LED } from "../core/ledSystem";
import { LEDAnimation } from "./types";
import { getLEDDescriptor } from "../ledMap";

// Matches fireColor in starFire.ts exactly
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

const STAGE1_COLORS: [number, number, number][] = [
  [1, 0, 0],     // red
  [0, 1, 0],     // green
  [0, 0, 1],     // blue
  [1, 1, 0],     // yellow
  [1, 1, 1],     // white
  [1, 0.45, 0],  // orange
];
const COLOR_NAMES = ["red", "green", "blue", "yellow", "white", "orange"];

const S1 = STAGE1_COLORS.length * 1.5;  // 9s
const S2 = 5;
const S3 = 5;
const TOTAL = S1 + S2 + S3;

export const diagState = { stage: "" };

export const diagTest: LEDAnimation = {
  name: "diagTest",

  update(leds: LED[], time: number) {
    const t = time % TOTAL;
    const totalLeds = leds.length;

    if (t < S1) {
      const colorIdx = Math.floor(t / 1.5) % STAGE1_COLORS.length;
      diagState.stage = `Stage 1 — ${COLOR_NAMES[colorIdx]}`;
      const [r, g, b] = STAGE1_COLORS[colorIdx];
      for (const led of leds) {
        led.color.setRGB(r, g, b);
        led.intensity = 1;
      }
    } else if (t < S1 + S2) {
      diagState.stage = "Stage 2 — heat ramp (cold→hot left→right)";
      for (const led of leds) {
        const heat = led.id / (totalLeds - 1);
        const [r, g, b] = fireColor(heat);
        led.color.setRGB(r, g, b);
        led.intensity = heat < 0.05 ? heat / 0.05 : 1;
      }
    } else {
      diagState.stage = "Stage 3 — tipness (bright=tip, dark=inner corner)";
      for (const led of leds) {
        const { edge, t: edgeT } = getLEDDescriptor(led.id);
        const tipness = edge % 2 === 0 ? 1 - edgeT : edgeT;
        led.color.setRGB(1, 1, 1);
        led.intensity = tipness;
      }
    }
  },
};
