import { Pane } from "tweakpane";
import { cfg } from "./config";
import { colorCycle } from "./animations/colorCycle";
import { magicDust } from "./animations/magicDust";
import { fallingRain } from "./animations/fallingRain";
import { AnimationManager } from "./core/animationManager";

const animations = {
  colorCycle,
  magicDust,
  fallingRain,
};

export function buildPanel(redraw: () => void, animationManager: AnimationManager): void {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const pane = new Pane({ title: "Jellyfish" }) as any;

  pane
    .addBlade({
      view: "list",
      label: "Animation",
      options: Object.keys(animations).map((key) => ({
        text: key,
        value: key,
      })),
      value: "colorCycle",
    })
    .on("change", (event: any) => {
      const selectedAnimation = animations[event.value as keyof typeof animations];
      cfg.animation = selectedAnimation;
      animationManager.set(selectedAnimation);
      redraw();
    });

  const sizes = pane.addFolder({ title: "Sizes" });
  sizes
    .addBinding(cfg, "size_ratio", {
      min: 0.1,
      max: 0.9,
      step: 0.05,
      label: "size ratio",
    })
    .on("change", redraw);
  sizes
    .addBinding(cfg, "z_offset", {
      min: -10,
      max: 0,
      step: 1,
      label: "Z offset",
    })
    .on("change", redraw);
  cfg.sizes.forEach((entry) => {
    if (entry.level === 0) return;
    const folder = sizes.addFolder({ title: `Level ${entry.level + 1}` });
    folder
      .addBinding(entry, "count", { min: 0, max: 10, step: 1, label: "count" })
      .on("change", redraw);
  });
}
