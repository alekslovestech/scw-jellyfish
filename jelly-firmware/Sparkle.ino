
static float spark[NUM_STRIPS][NUM_LEDS_PER_STRIP];

void sparkle()
{
    if (random(1000) < 200) {
      uint8_t s = random(NUM_STRIPS);
      uint16_t p = random(NUM_LEDS_PER_STRIP);
      spark[s][p] = 255.0f;
    }

    if (random(1000) < 200) {
      uint8_t s = random(NUM_STRIPS);
      uint16_t p = random(NUM_LEDS_PER_STRIP);
      spark[s][p] = 255.0f;
    }

    // Fade and write pixels
    for (uint8_t s = 0; s < NUM_STRIPS; s++) {
      StripBus& strip = strips[s];

      for (uint16_t p = 0; p < NUM_LEDS_PER_STRIP; p++) {
        spark[s][p] *= 0.9f;

        uint8_t brightness = (uint8_t)spark[s][p];
        strip.SetPixelColor(p, RgbColor(brightness, brightness, brightness));
      }
    }
  }
