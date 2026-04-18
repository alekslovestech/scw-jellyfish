import { LED } from "./ledSystem";
import { LEDAnimation } from "../animations/types";

export class AnimationManager {
  private current?: LEDAnimation;

  set(animation: LEDAnimation) {
    this.current = animation;
  }

  update(leds: LED[], time: number) {
    if (!this.current) return;
    this.current.update(leds, time);
  }
}