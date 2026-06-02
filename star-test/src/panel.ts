import { Pane } from "tweakpane";
import { cfg } from "./config";
import { AnimationManager } from "./core/animationManager";
import { starPulse } from "./animations/starPulse";
import { starChase } from "./animations/starChase";
import { starPoints } from "./animations/starPoints";

const animations = { starPulse, starChase, starPoints };

export function buildPanel(animationManager: AnimationManager): void {
  const pane = new Pane({ title: "Star" }) as any;

  pane
    .addBlade({
      view: "list",
      label: "Animation",
      options: Object.keys(animations).map((key) => ({ text: key, value: key })),
      value: "starPulse",
    })
    .on("change", (event: any) => {
      const anim = animations[event.value as keyof typeof animations];
      cfg.animation = anim;
      animationManager.set(anim);
    });
}
