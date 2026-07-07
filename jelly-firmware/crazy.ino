void crazy()
{

    for (uint8_t s = 0; s < NUM_STRIPS; s++) {
      StripBus& strip = strips[s];

      for (uint16_t p = 0; p < NUM_LEDS_PER_STRIP; p++) {

        // Equivalent to: if random.random() < 0.1
        if (random(1000) < 100) {
          strip.SetPixelColor(p, wheel(random(256)));
        } else {
          strip.SetPixelColor(p, RgbColor(0, 0, 0));
        }
      }
    }
}