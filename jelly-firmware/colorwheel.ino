#include "config.h"

// Color wheel pattern — a hue wheel spread across the 8 strips, slowly drifting.
// Uses the wheel() helper in leds.ino.
void showColorWheelAcrossEightStrips()
{
  constexpr uint8_t WHEEL_STRIPS = 8;

  // Slow global drift of the whole wheel
  uint8_t globalShift = millis() / 20;   // smaller = slower, larger = faster

  for (uint8_t s = 0; s < WHEEL_STRIPS; ++s) {
    StripBus& strip = strips[s];

    // Evenly space the strips around the color wheel
    uint8_t stripOffset = (uint8_t)((s * 256) / WHEEL_STRIPS);

    for (uint16_t i = 0; i < NUM_LEDS_PER_STRIP; ++i) {
      uint8_t ledHue = (uint8_t)((i * 256) / NUM_LEDS_PER_STRIP);

      // Combine strip offset + position + slow animation
      uint8_t hue = ledHue + stripOffset + globalShift;

      strip.SetPixelColor(i, wheel(hue));
    }

    strip.Show();
  }
}
