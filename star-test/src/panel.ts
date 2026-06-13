import { Pane } from "tweakpane";
import { cfg } from "./config";
import { AnimationManager } from "./core/animationManager";
import { starPulse } from "./animations/starPulse";
import { starChase } from "./animations/starChase";
import { starPoints } from "./animations/starPoints";
import { starRain } from "./animations/starRain";
import { starFire } from "./animations/starFire";
import { diagTest, diagState } from "./animations/diagTest";
import { starPyramid } from "./animations/starPyramid";

const animations = { starPyramid, diagTest, starFire, starRain, starPulse, starChase, starPoints };

export function buildPanel(animationManager: AnimationManager): void {
  const pane = new Pane({ title: "Star" }) as any;

  const listBlade = pane
    .addBlade({
      view: "list",
      label: "Animation",
      options: Object.keys(animations).map((key) => ({ text: key, value: key })),
      value: "starRain",
    });

  const stageMonitor = pane.addBinding(diagState, "stage", {
    label: "Stage",
    readonly: true,
  });
  stageMonitor.hidden = true;

  listBlade.on("change", (event: any) => {
    const anim = animations[event.value as keyof typeof animations];
    cfg.animation = anim;
    animationManager.set(anim);
    stageMonitor.hidden = event.value !== "diagTest";
  });
}
