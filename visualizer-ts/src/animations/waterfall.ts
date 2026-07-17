import { LED } from "../core/ledSystem";
import { cfg } from "../config";

// ─────────────────────────────────────────────────────────────
// WATERFALL ANIMATION SETTINGS
// ─────────────────────────────────────────────────────────────

const WAVE_SPEED = 1.5;
// How fast waves travel down the tentacles (0.5 = slow, 2.0 = fast)

const WAVE_WIDTH = 0.35;
// Width of each wave pulse (0.1 = thin wave, 0.4 = wide wave)

const WAVE_SPACING = 0.15;
// Time between consecutive waves (0.2 = rapid fire, 0.8 = slow rhythm)

// Speed multiplier on the falling waves (1 = the original WAVE_SPEED cadence).
const params = { speed: 1 };

export const waterfall = {
  name: "waterfall",
  colorMode: "two" as const,
  // waterfall drives its own wave brightness and ignores fxState.excitement, so
  // the global intenseReaction flare has no effect here. innerGlow only reads well
  // over movementSimulation. Hide both in the panel.
  excludeEffects: ["intenseReaction", "innerGlow"],
  params,
  controls: [{ key: "speed", label: "speed", min: 0.1, max: 3, step: 0.05 }],

  update(leds: LED[], time: number) {
    // Two-colour palette (editable live from the panel):
    //   inner = the rest/base colour between waves,
    //   outer = the wave-crest colour at each pulse peak.
    // `mix` (0→1) is the wave intensity used both to blend the two colours and
    // to drive brightness, so a crest reads as `outer` and the lulls as `inner`.
    const rest = cfg.palette.inner;
    const crest = cfg.palette.outer;

    for (const led of leds) {
      let intensity = 0;
      let mix = 0;

      // Only animate tentacles and bell; center might not be visible
      if (led.group === "bell" || led.group === "outer" || led.group === "inner") {
        // Each LED has a `t` value: 0 (start of segment) → 1 (end of segment)
        // For bell: t = 0 is inner edge, t = 1 is outer edge (start point)
        // For outer tentacles: t = 0 is root at bell, t = 1 is tip (travel down)
        // For inner tentacles: t = 0 is tip, t = 1 is root at bell

        // Inner half of bell is always "on"
        if (led.group === "bell" && (led.t ?? 0) < 0.5) {
          intensity = 1.0;
          mix = 1.0;
        } else {
          // Outer half of bell and all tentacles animate with waves
          let normalizedPosition = 0;

          if (led.group === "bell") {
            // Wave starts at the bell, use t directly
            normalizedPosition = led.t ?? 0;
          } else if (led.group === "outer") {
            // Wave travels down from root (t=0) to tip (t=1)
            normalizedPosition = led.t ?? 0;
          } else if (led.group === "inner") {
            // Wave travels down from root (t=1) to tip (t=0)
            normalizedPosition = 1 - (led.t ?? 0);
          }

          // Create waves using time and position
          const wavePhase = (time * WAVE_SPEED * params.speed - normalizedPosition) % (1 + WAVE_SPACING);

          // Each wave is a pulse
          const wavePosition = wavePhase % 1;

          if (wavePosition < WAVE_WIDTH) {
            // We're inside a wave pulse
            const waveIntensity = 1 - wavePosition / WAVE_WIDTH;
            intensity = waveIntensity;
            mix = waveIntensity;
          } else {
            // Dim glow between waves
            intensity = 0.25;
            mix = 0;
          }
        }
      }

      led.intensity = intensity;
      led.color.setRGB(
        rest.r + (crest.r - rest.r) * mix,
        rest.g + (crest.g - rest.g) * mix,
        rest.b + (crest.b - rest.b) * mix,
      );
    }
  }
};