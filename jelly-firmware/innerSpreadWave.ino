#include "config.h"
#include "color_utils.h"

// innerSpreadWave — ported from visualizer-ts/src/animations/innerSpreadWave.ts.
//
// A traveling sinusoidal "worm" runs along the inner -> bell -> outer path. Inner
// tentacles show a red worm on black; bell/outer sit blue and flash red as the
// crest passes. The inner path position uses PHYSICAL HEIGHT so the folded inner
// tentacle doesn't scramble the wave (bell/outer don't fold, so they use t).
void innerSpreadWave() {
  const float WORM_FREQ   = 17.0f;  // spatial frequency along the full path
  const float SPEED       = 1.2f;   // wave travel speed (was 5 in TS; slowed down)
  const float JELLY_PHASE = 0.8f;   // phase offset per jelly so a fleet ripples
  const float MAX_DEPTH   = 1.8f;   // top (y=0) to lowest inner tip (~ -1.8)

  float time = millis() / 1000.0f;
  float jellyOffset = _jellyId * JELLY_PHASE;

  for (int s = 0; s < 8; s++) {
    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
      int group; float t;
      fsSegmentForPos(p, &group, &t);

      // Continuous 0->1 position: inner tip(0) -> inner root(0.6) -> bell rim(0.8) -> outer tip(1).
      float pos;
      if (group == FS_SEG_INNER) {
        float depth = fsClamp01(-ledPos[s][p].y / MAX_DEPTH); // 0 root(top) .. 1 tip(bottom)
        pos = (1.0f - depth) * 0.6f;                          // tip -> 0, root -> 0.6
      } else if (group == FS_SEG_BELL) {
        pos = 0.6f + t * 0.2f;
      } else { // outer
        pos = 0.8f + t * 0.2f;
      }

      float wave = sinf(pos * WORM_FREQ - time * SPEED + jellyOffset);
      if (wave < 0.0f) wave = 0.0f;

      if (group == FS_SEG_INNER) {
        // Red worm on inner; dark between pulses (R = wave^2, matching the TS look).
        strips[s].SetPixelColor(p, rgbWithIntensity(wave, 0, 0, wave));
      } else {
        // Bell/outer: blue at rest, red as the crest passes through.
        strips[s].SetPixelColor(p, rgbWithIntensity(wave, 0, 1.0f - wave));
      }
    }
  }
}
