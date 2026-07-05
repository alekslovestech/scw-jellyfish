#include "config.h"

// Bottom fill pattern — red rises from the bottom of each tentacle over time.
void bottomFill() {
  constexpr uint8_t WHEEL_STRIPS = 8;
  for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
    struct Pos2D pos;
    pos = smallJellyFind2Dpos(p);
    for (int s = 0; s < WHEEL_STRIPS; s++) {
      if (-pos.y*100 > _ledFrame%200) strips[s].SetPixelColor(p, RgbColor(255,0,0));
      else strips[s].SetPixelColor(p, RgbColor(0,0,0));
    };
  };
  for (int s = 0; s < NUM_STRIPS; s++) {
    strips[s].Show();
  };
}
