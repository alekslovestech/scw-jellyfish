import { LED } from "./ledSystem";
import { LEDAnimation, Effect } from "../animations/types";
import { cfg } from "../config";

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

    // Master per-segment brightness, applied last so it scales the final result
    // (base + effects) for every animation. `outer` covers the bell as well as
    // the outer tentacles; `inner` scales the inner tentacles.
    const { inner, outer } = cfg.brightness;
    if (inner !== 1 || outer !== 1) {
      for (const led of leds) {
        if (led.group === "inner") led.intensity *= inner;
        else led.intensity *= outer; // outer tentacles + bell
      }
    }
  }
}
