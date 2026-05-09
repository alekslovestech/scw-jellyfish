import { LED } from "../core/ledSystem";

// ─────────────────────────────────────────────────────────────
// WATERFALL ANIMATION SETTINGS
// ─────────────────────────────────────────────────────────────

const WAVE_SPEED = 1.5;
// How fast waves travel down the tentacles (0.5 = slow, 2.0 = fast)

const WAVE_WIDTH = 0.35;
// Width of each wave pulse (0.1 = thin wave, 0.4 = wide wave)

const WAVE_SPACING = 0.15;
// Time between consecutive waves (0.2 = rapid fire, 0.8 = slow rhythm)

const COLOR_DARK_B = 0.3;
// Dark blue channel for inactive parts

const COLOR_BRIGHT_B = 1.0;
// Bright blue channel for wave peak

export const waterfall = {
  name: "waterfall",

  update(leds: LED[], time: number) {
    for (const led of leds) {
      let intensity = 0;
      let colorB = COLOR_DARK_B;

      // Only animate tentacles and bell; center might not be visible
      if (led.group === "bell" || led.group === "outer" || led.group === "inner") {
        // Each LED has a `t` value: 0 (start of segment) → 1 (end of segment)
        // For bell: t = 0 is inner edge, t = 1 is outer edge (start point)
        // For outer tentacles: t = 0 is root at bell, t = 1 is tip (travel down)
        // For inner tentacles: t = 0 is tip, t = 1 is root at bell

        // Inner half of bell is always "on"
        if (led.group === "bell" && (led.t ?? 0) < 0.5) {
          intensity = 1.0;
          colorB = COLOR_BRIGHT_B;
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
          const wavePhase = (time * WAVE_SPEED - normalizedPosition) % (1 + WAVE_SPACING);
          
          // Each wave is a pulse
          let waveIntensity = 0;
          const wavePosition = wavePhase % 1;
          
          if (wavePosition < WAVE_WIDTH) {
            // We're inside a wave pulse
            waveIntensity = 1 - wavePosition / WAVE_WIDTH;
            intensity = waveIntensity;
            colorB = COLOR_DARK_B + (COLOR_BRIGHT_B - COLOR_DARK_B) * waveIntensity;
          } else {
            // Dim glow between waves
            intensity = 0.25;
            colorB = COLOR_DARK_B;
          }
        }
      }

      led.intensity = intensity;
      led.color.setRGB(0.0, 0.2 * intensity, colorB);
    }
  }
};