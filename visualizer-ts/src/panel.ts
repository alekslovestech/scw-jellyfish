import { Pane } from "tweakpane";
import { cfg } from "./config";
import { movementSimulation } from "./animations/movementSimulation";
import { innerSpreadWave } from "./animations/innerSpreadWave";
import { fireSteady } from "./animations/fireSteady";
import { waterfall } from "./animations/waterfall";
import { AnimationManager } from "./core/animationManager";
import { effects } from "./animations/effects";
import { innerGlow } from "./animations/effects/innerGlow";
import { LEDAnimation } from "./animations/types";


// The three base animations we build the control panel around.
const animations = {
  movementSimulation,
  innerSpreadWave,
  fireSteady,
  waterfall,
};

export function buildPanel(redraw: () => void, animationManager: AnimationManager): void {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const pane = new Pane({ title: "Jellyfish" }) as any;

  const animationBlade = pane
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
      applyAnimationControls(selectedAnimation);
      redraw();
    });

  // Per-animation controls (e.g. speed). Rebuilt in place whenever the selected
  // animation changes, so only the current base's own params show here.
  const animParams = pane.addFolder({ title: "Animation Settings" });
  function renderAnimParams(anim: LEDAnimation): void {
    // Clear the previous animation's bindings before adding the new one's.
    animParams.children.slice().forEach((child: any) => child.dispose());
    if (!anim.params || !anim.controls) {
      animParams.hidden = true;
      return;
    }
    animParams.hidden = false;
    const p = anim.params;
    anim.controls.forEach((ctrl) => {
      if (ctrl.options) {
        const opts = Object.fromEntries(ctrl.options.map((o) => [o, o]));
        animParams.addBinding(p, ctrl.key, { label: ctrl.label, options: opts });
      } else {
        animParams.addBinding(p, ctrl.key, {
          label: ctrl.label,
          min: ctrl.min,
          max: ctrl.max,
          step: ctrl.step,
        });
      }
    });
  }

  // ── Colours ──────────────────────────────────────────────────────────────
  // Live two-colour palette shared by the palette-driven bases
  // (movementSimulation: inner→outer gradient; innerSpreadWave: worm/rest).
  // Values are normalised 0–1 RGB, so the pickers use float mode. The active
  // base reads cfg.palette every frame, so no rebuild is needed.
  const colors = pane.addFolder({ title: "Colors" });
  colors.addBinding(cfg.palette, "inner", { label: "inner tips", color: { type: "float" } });
  colors.addBinding(cfg.palette, "outer", { label: "outer tips", color: { type: "float" } });

  // Three-colour gradient for the fireSteady base (base → mid → tips).
  // Titled just "Colors" — it's only shown when fireSteady is selected, so it
  // never collides with the two-colour Colors folder above.
  const fireColors = pane.addFolder({ title: "Colors" });
  fireColors.addBinding(cfg.firePalette, "c1", { label: "base", color: { type: "float" } });
  fireColors.addBinding(cfg.firePalette, "c2", { label: "mid", color: { type: "float" } });
  fireColors.addBinding(cfg.firePalette, "c3", { label: "tips", color: { type: "float" } });

  // Master per-segment brightness — applies to every animation (scales the final
  // inner/outer tentacle intensity after the base + effects). Not mode-specific,
  // so it's always shown.
  const brightness = pane.addFolder({ title: "Brightness" });
  const innerBrightness = brightness.addBinding(cfg.brightness, "inner", { label: "inner", min: 0, max: 1, step: 0.05 });
  brightness.addBinding(cfg.brightness, "outer", { label: "outer + bell", min: 0, max: 1, step: 0.05 });

  // While innerGlow is on it governs the inner tentacles' brightness (via its own
  // min/max range), so the inner master slider would just compound with it —
  // disable it to make clear the effect is in charge.
  function syncInnerBrightnessLock(): void {
    innerBrightness.disabled = innerGlow.enabled;
  }

  // ── Effects ──────────────────────────────────────────────────────────────
  // Each effect gets its own folder: an enabled toggle plus a slider for every
  // tunable param it declares. Toggling one off runs its own cleanup (if any) so
  // any shared state it was driving doesn't stick.
  // Every effect's folder is recorded with its effect so applyAnimationControls()
  // can hide the ones that don't fit the selected base (wrong colour mode, or
  // explicitly excluded by the animation).
  const effectFolders: { effect: typeof effects[number]; folder: any }[] = [];
  const fx = pane.addFolder({ title: "Effects" });
  effects.forEach((effect) => {
    const folder = fx.addFolder({ title: effect.name });
    folder.addBinding(effect, "enabled", { label: "enabled" }).on(
      "change",
      (event: any) => {
        if (!event.value) effect.onDisable?.();
        // innerGlow taking over inner brightness → lock/unlock the master slider.
        if (effect === innerGlow) syncInnerBrightnessLock();
      },
    );
    effectFolders.push({ effect, folder });
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

  // Show only the controls that fit the selected base animation:
  //   colours — "two" → Colors folder + colorCycle effect;
  //             "three" → Fire Colors folder + fireColorCycle effect.
  //   effects — hide any whose colour mode doesn't match, plus any the animation
  //             lists in excludeEffects (they'd be inert for that base).
  // A hidden effect is also switched off (running its cleanup) so it doesn't keep
  // driving shared state the visible base ignores.
  function applyAnimationControls(anim: LEDAnimation): void {
    const mode = anim.colorMode;
    renderAnimParams(anim); // swap in this animation's own controls (speed, …)
    colors.hidden = mode !== "two";
    fireColors.hidden = mode !== "three";
    effectFolders.forEach(({ effect, folder }) => {
      const modeMismatch = effect.colorMode !== undefined && effect.colorMode !== mode;
      const excluded = anim.excludeEffects?.includes(effect.name) ?? false;
      const hide = modeMismatch || excluded;
      folder.hidden = hide;
      if (hide && effect.enabled) {
        effect.enabled = false;
        effect.onDisable?.();
        folder.refresh(); // sync the (now hidden) toggle to its off state
      }
    });
    // A base switch may have force-disabled innerGlow above — release the lock.
    syncInnerBrightnessLock();
  }

  // Match the initial selection (movementSimulation → two-colour controls).
  const initial = animations[animationBlade.value as keyof typeof animations];
  applyAnimationControls(initial);

  // Sizes / Levels controls are hidden for now (see git history to restore).
}
