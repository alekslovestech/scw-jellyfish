#include "config.h"
#include "color_utils.h"

// Three ripples between x = -2 and x = 2. Center wave the biggest. Value range 0..256
float ripple(float x) {
    if (x <= -2.0f || x >= 2.0f) return 0.0f;
    float x2 = x * x;
    return 16.0f * (x2 - 4.0f) * (x2 - 4.0f) * (x2 - 1.0f) * (x2 - 1.0f);
}

// Ripple pattern — expanding wavefront radiating outward from an arbitrary 3-D point.
void rippleOutFromPoint(struct Pos3D& startPoint, int frame, RgbColor rgb) {
  float distance;
  for (int s = 0; s < 8; s++) {
    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
      distance = distanceFast(startPoint, ledPos[s][p]);
      float f = ripple(frame/20.0f-distance*10.0f-2.0f);
      strips[s].SetPixelColor(p, rgbWithIntensity(rgb, f));
    }
  }
}
