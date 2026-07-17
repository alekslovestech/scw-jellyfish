#include "config.h"
#include "color_utils.h"

// Three lobes between x = -2 and x = 2.
// Returns an intensity from 0.0 to 1.0.
float ripple(float x) {
  if (x <= -2.0f || x >= 2.0f) {
    return 0.0f;
  }

  const float x2 = x * x;
  const float a = x2 - 4.0f;
  const float b = x2 - 1.0f;

  // Original maximum is 256 at x = 0.
  return (a * a * b * b) / 16.0f;
}

// Expanding wavefront radiating outward from a 3-D point.
void rippleOutFromPoint(const Pos3D& startPoint, int frame, RgbColor rgb) {
  const float wavePosition = frame * 0.01f;

  for (int s = 0; s < 8; s++) {
    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
      const float distance = distanceFast(startPoint, ledPos[s][p]);

      const float waveX =
          wavePosition
          - distance * 10.0f
          - 2.0f;

      const float intensity = ripple(waveX);

      strips[s].SetPixelColor(
          p,
          rgbWithIntensity(rgb, intensity)
      );
    }
  }
}