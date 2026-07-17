import { Effect } from "../types";
import { hash01 } from "../hash";
import { fxState } from "../../core/fxState";

// --- OCCASIONAL "INTENSE REACTION" (rare, global, sudden) ---
// Most of the time the installation drifts calmly. Now and then the WHOLE thing
// reacts at once: a fast, sudden flare, then a quick relax back to calm. This
// effect produces a single global excitement value (0 = calm, 1 = full flare)
// into fxState; the base animation reads it to decide how to react (e.g.
// movementSimulation widens every tentacle's sweep).

// Live-tunable from the panel (see controls below).
const params = {
  period: 6.0, // s — spacing of potential reaction windows
  chance: 0.4, // fraction of windows that actually fire (0–1)
};

// Fixed shape of a single flare — fast snap up, quick relax.
const BURST_ATTACK = 0.15; // s — near-instant snap to full intensity
const BURST_DECAY = 1.2; // s — quick relax back to calm

// Global reaction strength for this instant. Only a `chance` fraction of windows
// fire, and each firing is a fast snap up followed by a quick decay.
function globalBurst(time: number, period: number, chance: number): number {
  const windowIndex = Math.floor(time / period);

  // Only a fraction of windows fire a reaction.
  if (hash01(windowIndex, 0, 7) >= chance) return 0;

  // Randomize the exact moment within the window so it isn't clockwork.
  const slack = period - (BURST_ATTACK + BURST_DECAY);
  const start = hash01(windowIndex, 1, 7) * Math.max(0, slack);
  const local = (time - windowIndex * period) - start;

  if (local < 0) return 0;
  if (local < BURST_ATTACK) return local / BURST_ATTACK; // fast snap up
  const d = local - BURST_ATTACK;
  if (d < BURST_DECAY) return 1 - d / BURST_DECAY; // quick relax
  return 0;
}

export const intenseReaction: Effect = {
  name: "intenseReaction",
  enabled: true,
  params,
  controls: [
    { key: "period", label: "burst period (s)", min: 1, max: 20, step: 0.5 },
    { key: "chance", label: "burst chance", min: 0, max: 1, step: 0.05 },
  ],

  apply(_leds, time) {
    fxState.excitement = globalBurst(time, params.period, params.chance);
  },

  // Switched off → make sure no reaction is left frozen mid-flare.
  onDisable() {
    fxState.excitement = 0;
  },
};
