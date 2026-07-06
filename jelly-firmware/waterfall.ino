#include "color_utils.h"

// waterfall — smooth downward-flowing waves, adapted from waterfall.ts.
//
// Driven by PHYSICAL HEIGHT (ledPos[s][p].y) so the folded inner tentacle reads as
// a single downward motion (see note below), and shaped as a raised-cosine PULSE so
// each wave rises from the ambient glow, peaks, and falls back with no hard edge —
// the old sawtooth popped from dim to full instantly at the top, which looked
// mechanical. This version is continuous, so waves are born and die organically.
void waterfall() {
  const float WAVE_SPEED = 0.40f;   // downward speed; lower = slower
  const float WAVE_COUNT = 2.0f;    // how many wave bands span the jelly at once
  const float WAVE_WIDTH = 0.45f;   // bright fraction of each cycle (the rest is the gap)
  const float DIM        = 0.25f;   // ambient glow between waves
  const float COLOR_G        = 0.2f;
  const float COLOR_DARK_B   = 0.3f;
  const float COLOR_BRIGHT_B = 1.0f;
  const float MAX_DEPTH  = 1.8f;    // top (y=0) to lowest inner tip (~ -1.8)

  float time = millis() / 1000.0f;

  for (int s = 0; s < 8; s++) {
    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
      // 0 at the top (bell), ~1 at the lowest tip. Both legs of the folded inner
      // tentacle share a height, so they brighten together = one downward wave.
      float depth = fsClamp01(-ledPos[s][p].y / MAX_DEPTH);

      // Position within the repeating cycle, 0..1. Subtracting depth makes the
      // pattern travel downward as time advances; WAVE_COUNT sets how many bands fit.
      float cyclePos = fmodf(time * WAVE_SPEED - depth * WAVE_COUNT, 1.0f);
      if (cyclePos < 0.0f) cyclePos += 1.0f;

      float intensity = DIM;
      float colorB = COLOR_DARK_B;

      if (cyclePos < WAVE_WIDTH) {
        float u = cyclePos / WAVE_WIDTH;                 // 0..1 across the bright band
        float bump = 0.5f * (1.0f - cosf(TWO_PI * u));   // smooth 0 -> 1 -> 0, flat at both ends
        intensity = DIM + (1.0f - DIM) * bump;
        colorB    = COLOR_DARK_B + (COLOR_BRIGHT_B - COLOR_DARK_B) * bump;
      }

      strips[s].SetPixelColor(p, rgbWithIntensity(0, COLOR_G * intensity, colorB, intensity));
    }
  }
}
