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

  // const bell = pane.addFolder({ title: "Bell" });
  // bell
  //   .addBinding(cfg.bell, "radius", { min: 0.5, max: 5.0, step: 0.1 })
  //   .on("change", redraw);

  // const tent = pane.addFolder({ title: "Outer tentacles" });
  // tent
  //   .addBinding(cfg.tentacles, "count", { min: 1, max: 32, step: 1 })
  //   .on("change", redraw);
  // tent
  //   .addBinding(cfg.tentacles, "radius", { min: 0.01, max: 0.3, step: 0.01 })
  //   .on("change", redraw);
  // tent
  //   .addBinding(cfg.tentacles, "length", { min: 0.5, max: 8.0, step: 0.1 })
  //   .on("change", redraw);
  // tent
  //   .addBinding(cfg.tentacles, "wave_amplitude", {
  //     min: 0.0,
  //     max: 0.5,
  //     step: 0.01,
  //     label: "wave amp",
  //   })
  //   .on("change", redraw);
  // tent
  //   .addBinding(cfg.tentacles, "wave_frequency", {
  //     min: 0.0,
  //     max: 5.0,
  //     step: 0.25,
  //     label: "wave freq",
  //   })
  //   .on("change", redraw);

  // const cent = pane.addFolder({ title: "Central tentacles" });
  // cent
  //   .addBinding(cfg.central, "count", { min: 1, max: 16, step: 1 })
  //   .on("change", redraw);
  // cent
  //   .addBinding(cfg.central, "radius", { min: 0.01, max: 0.5, step: 0.01 })
  //   .on("change", redraw);
  // cent
  //   .addBinding(cfg.central, "length", { min: 0.5, max: 10.0, step: 0.1 })
  //   .on("change", redraw);
  // cent
  //   .addBinding(cfg.central, "wave_amplitude", {
  //     min: 0.0,
  //     max: 0.5,
  //     step: 0.01,
  //     label: "wave amp",
  //   })
  //   .on("change", redraw);
  // cent
  //   .addBinding(cfg.central, "wave_frequency", {
  //     min: 0.0,
  //     max: 5.0,
  //     step: 0.25,
  //     label: "wave freq",
  //   })
  //   .on("change", redraw);
  // cent
  //   .addBinding(cfg.central, "ring_radius", {
  //     min: 0.0,
  //     max: 2.0,
  //     step: 0.05,
  //     label: "ring radius",
  //   })
  //   .on("change", redraw);

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
