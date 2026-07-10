import { Pane } from "tweakpane";
import { cfg } from "./config";
import { movementSimulation } from "./animations/movementSimulation";
import { innerSpreadWave } from "./animations/innerSpreadWave";
import { fireSteady } from "./animations/fireSteady";
import { AnimationManager } from "./core/animationManager";
import { effects } from "./animations/effects";


// The three base animations we build the control panel around.
const animations = {
  movementSimulation,
  innerSpreadWave,
  fireSteady,
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
      value: "movementSimulation",
    })
    .on("change", (event: any) => {
      const selectedAnimation = animations[event.value as keyof typeof animations];
      animationManager.set(selectedAnimation);
      redraw();
    });

  // ── Colours ──────────────────────────────────────────────────────────────
  // Live two-colour palette shared by the palette-driven bases
  // (movementSimulation: inner→outer gradient; innerSpreadWave: worm/rest).
  // Values are normalised 0–1 RGB, so the pickers use float mode. The active
  // base reads cfg.palette every frame, so no rebuild is needed.
  const colors = pane.addFolder({ title: "Colors" });
  colors.addBinding(cfg.palette, "inner", { label: "inner tips", color: { type: "float" } });
  colors.addBinding(cfg.palette, "outer", { label: "outer tips", color: { type: "float" } });

  // Three-colour gradient for the fireSteady base (base → mid → tips).
  const fireColors = pane.addFolder({ title: "Fire Colors" });
  fireColors.addBinding(cfg.firePalette, "c1", { label: "base", color: { type: "float" } });
  fireColors.addBinding(cfg.firePalette, "c2", { label: "mid", color: { type: "float" } });
  fireColors.addBinding(cfg.firePalette, "c3", { label: "tips", color: { type: "float" } });

  // ── Effects ──────────────────────────────────────────────────────────────
  // Each effect gets its own folder: an enabled toggle plus a slider for every
  // tunable param it declares. Toggling one off runs its own cleanup (if any) so
  // any shared state it was driving doesn't stick.
  const fx = pane.addFolder({ title: "Effects" });
  effects.forEach((effect) => {
    const folder = fx.addFolder({ title: effect.name });
    folder.addBinding(effect, "enabled", { label: "enabled" }).on(
      "change",
      (event: any) => {
        if (!event.value) effect.onDisable?.();
      },
    );
    if (effect.params && effect.controls) {
      const p = effect.params;
      effect.controls.forEach((ctrl) => {
        if (ctrl.options) {
          // Dropdown: Tweakpane wants an { label: value } map.
          const opts = Object.fromEntries(ctrl.options.map((o) => [o, o]));
          folder.addBinding(p, ctrl.key, { label: ctrl.label, options: opts });
        } else {
          folder.addBinding(p, ctrl.key, {
            label: ctrl.label,
            min: ctrl.min,
            max: ctrl.max,
            step: ctrl.step,
          });
        }
      });
    }
  });

  // Sizes / Levels controls are hidden for now (see git history to restore).
}
