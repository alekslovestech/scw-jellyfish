import { LED } from "../core/ledSystem";
import { LEDAnimation } from "./types";
import { getLEDDescriptor } from "../ledMap";

export const starPulse: LEDAnimation = {
  name: "starPulse",

  update(leds: LED[], time: number) {
    const pulse = 0.5 + 0.5 * Math.sin(time * 2);

    for (const led of leds) {
      const { isAtOuterTip, isAtInnerCorner, t } = getLEDDescriptor(led.id);

      // Outer tips: bright gold
      // Inner corners: dim blue
      // Edges: interpolate between the two
      const tipness = isAtOuterTip ? 1 : isAtInnerCorner ? 0 : (1 - t);

      led.color.setRGB(
        0.9 * tipness + 0.1 * (1 - tipness),
        0.7 * tipness + 0.1 * (1 - tipness),
        0.1 * tipness + 0.5 * (1 - tipness)
      );
      led.intensity = (0.3 + 0.7 * tipness) * pulse;
    }
  },
};
