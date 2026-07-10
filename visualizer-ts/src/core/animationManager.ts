import { LED } from "./ledSystem";
import { LEDAnimation, Effect } from "../animations/types";

export class AnimationManager {
  private base?: LEDAnimation;
  private effects: Effect[] = [];

  // Select the base animation (the underlying motion + colour).
  set(animation: LEDAnimation) {
    this.base = animation;
  }

  // Register the stack of effects that layer on top of the base.
  setEffects(effects: Effect[]) {
    this.effects = effects;
  }

  update(leds: LED[], time: number) {
    if (!this.base) return;
    this.base.update(leds, time);
    for (const fx of this.effects) {
      if (fx.enabled) fx.apply(leds, time);
    }
  }
}
