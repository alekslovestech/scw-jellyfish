import { Effect } from "../types";
import { cfg } from "../../config";
import { CYCLE_NAMES, cycleColor, setRGB } from "./cyclePalette";

// --- COLOUR CYCLE (2-colour bases) ---
// Slowly cross-fades the 2-colour palette (cfg.palette) through a chosen cycle
// (or randomly), so movementSimulation and innerSpreadWave shift colour over
// time. inner = the current colour, outer = the colour it's fading toward — a
// live gradient that itself cycles. fireSteady uses its own fireColorCycle.
//
// While this effect is ON it overrides the manual inner/outer colour pickers;
// turn it off to go back to hand-set colours.

const params = {
  cycle: "rainbow", // which named cycle, or "random"
  transition: 4, // seconds to fade from one colour to the next
};

export const colorCycle: Effect = {
  name: "colorCycle",
  enabled: false, // off by default — toggle it on in the panel
  colorMode: "two", // drives the 2-colour bases (movementSimulation, innerSpreadWave)

  params,
  controls: [
    { key: "cycle", label: "cycle", options: CYCLE_NAMES },
    { key: "transition", label: "transition (s)", min: 0.5, max: 20, step: 0.5 },
  ],

  apply(_leds, time) {
    const current = cycleColor(params.cycle, params.transition, time, 0);
    const next = cycleColor(params.cycle, params.transition, time, 1);
    setRGB(cfg.palette.inner, current);
    setRGB(cfg.palette.outer, next);
  },
};
