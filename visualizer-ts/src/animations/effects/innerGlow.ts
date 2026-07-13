import { Effect } from "../types";
import { getLEDDescriptor } from "../../ledMap";

// --- INNER TENTACLE GLOW ---
// A slow "breathing" glow on ONLY the inner tentacles: every inner LED brightens
// and dims together (in sync, no travelling wave), so the inner tentacles pulse
// as one. The single `speed` param sets how fast the breath cycles.
//
// It MODULATES the base rather than overriding it: it multiplies the inner
// tentacles' brightness, so it keeps whatever colour + reach the base gave them.
// Tagged colourMode "two", so the panel only offers it on the 2-colour bases.

const params = {
  speed: 0.5, // breaths per second (Hz) — cycle time is 1 / speed
  min: 0.3, // brightness at the trough of the breath (0 = fully dark)
  max: 1, // brightness at the crest of the breath (1 = full)
};

const TWO_PI = Math.PI * 2;

export const innerGlow: Effect = {
  name: "innerGlow",
  enabled: false, // off by default — toggle it on in the panel
  colorMode: "two", // inner-tentacle glow for the 2-colour bases

  params,
  controls: [
    // `speed` is breaths/sec, so cycle time = 1 / speed:
    // 0.25 → 4 s per breath (slowest), 2 → 0.5 s per breath (fastest).
    { key: "speed", label: "speed", min: 0.25, max: 2, step: 0.05 },
    // Brightness range the breath sweeps between (trough → crest).
    { key: "min", label: "min bright", min: 0, max: 1, step: 0.05 },
    { key: "max", label: "max bright", min: 0, max: 1, step: 0.05 },
  ],

  apply(leds, time) {
    // One global breath value shared by every inner LED, so they pulse in sync.
    const s = 0.5 + 0.5 * Math.sin(time * params.speed * TWO_PI); // 0..1
    const glow = params.min + (params.max - params.min) * s;

    for (const led of leds) {
      const desc = getLEDDescriptor(led.id);
      if (desc.segment !== "inner") continue; // inner tentacles only
      led.intensity *= glow;
    }
  },
};
