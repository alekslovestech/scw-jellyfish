#include "config.h"

// Demo pattern — a distinct solid colour per strip, for wiring/identification.
void demo() {
  fillStrip(strips[0], RgbColor(MAX_BRIGHTNESS, 0, 0));     // red
  fillStrip(strips[1], RgbColor(0, MAX_BRIGHTNESS, 0));     // green
  fillStrip(strips[2], RgbColor(0, 0, MAX_BRIGHTNESS));     // blue
  fillStrip(strips[3], RgbColor(MAX_BRIGHTNESS, MAX_BRIGHTNESS, 0));   // yellow
  fillStrip(strips[4], RgbColor(0, MAX_BRIGHTNESS, MAX_BRIGHTNESS));   // cyan
  fillStrip(strips[5], RgbColor(MAX_BRIGHTNESS, 0, MAX_BRIGHTNESS));   // magenta
  fillStrip(strips[6], RgbColor(MAX_BRIGHTNESS, MAX_BRIGHTNESS/2, 0));   // orange
  fillStrip(strips[7], RgbColor(MAX_BRIGHTNESS/2, 0, MAX_BRIGHTNESS));   // purple
  for (int i=0; i<8; i++) {
    strips[i].Show();
  }
}
