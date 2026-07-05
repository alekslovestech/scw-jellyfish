#include "config.h"

// Sensor demo pattern — fills each strip up to the current _agitation level
// (driven by the load cell) with a cycling hue.
void sensorDemo() {
  for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
    for (int s = 0; s<8 ; s++) {
      if (p < _agitation) strips[s].SetPixelColor(p, wheel(_ledFrame%256));
      else strips[s].SetPixelColor(p, RgbColor(0,0,0));
    }
  }
  for (int s = 0; s < NUM_STRIPS; s++) {
    strips[s].Show();
  };
}
