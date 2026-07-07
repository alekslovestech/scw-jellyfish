#include "config.h"
#include "color_utils.h"

// rainbowWave — ported from visualizer-ts/src/animations/rainbowWave.ts.
//
// Hue varies by arm angle (radial rainbow) and flows along each tentacle, with a
// slow global spin and a gentle brightness pulse. Speeds are dialled well below
// the TS values for a slow, calm drift. Inner-tentacle flow uses PHYSICAL HEIGHT
// so the fold doesn't mirror the gradient (bell/outer use t; they don't fold).
void rainbowWave() {
  const float SPIN_SPEED   = 0.03f;  // global hue rotation (was 0.08; slower)
  const float FLOW_SPEED   = 0.15f;  // hue flow along tentacles (was 0.4; slower)
  const float JELLY_OFFSET = 0.07f;  // hue offset per jelly
  const float ANGLE_SPREAD = 0.35f;  // hue span across a full ring of arms
  const float PULSE_SPEED  = 0.40f;  // brightness breathe (was 1.2; slower)
  const float PULSE_DEPTH  = 0.25f;  // how much the pulse dims
  const float MAX_DEPTH    = 1.8f;   // top (y=0) to lowest inner tip (~ -1.8)

  float time = millis() / 1000.0f;
  float jellyHue = _jellyId * JELLY_OFFSET;
  float timeSpin = time * SPIN_SPEED;
  float pulse = 1.0f - PULSE_DEPTH * (0.5f + 0.5f * sinf(time * PULSE_SPEED * TWO_PI));

  for (int s = 0; s < 8; s++) {
    float angleHue = (s / 8.0f) * ANGLE_SPREAD;   // each arm a different hue

    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
      int group; float t;
      fsSegmentForPos(p, &group, &t);

      // flow position along the tentacle: 1 at tip, 0 at root.
      float flowT;
      if (group == FS_SEG_INNER) {
        flowT = fsClamp01(-ledPos[s][p].y / MAX_DEPTH); // depth: tip=1, root=0
      } else {
        flowT = t;
      }

      float flowHue = flowT * 0.5f;
      float timeFlow = flowT * time * FLOW_SPEED * 0.1f;

      float hue = fmodf(angleHue + flowHue + jellyHue + timeSpin + timeFlow, 1.0f);

      RgbColor c = HslColor(hue, 1.0f, 0.5f * pulse);
      strips[s].SetPixelColor(p, rgbWithIntensity(c, pulse));
    }
  }
}
