// Add gently glowing random pixels on top of the existing frame.
//
// newStartsPerSecond:
//   Average number of new pixels started each second.
//
// risePerSecond:
//   Brightness increase per second, on a 0..255 scale.
//   Example: 100 reaches full brightness in about 2.55 seconds.
//
// fadePerSecond:
//   Brightness decrease per second, on a 0..255 scale.
//
// peakBrightness:
//   Maximum brightness reached by each glow.
void overlayRandomPixelGlows(float newStartsPerSecond,
                             float risePerSecond,
                             float fadePerSecond,
                             float peakBrightness) {
  static unsigned long previousMs = millis();
  static float startAccumulator = 0.0f;

  const unsigned long now = millis();
  float deltaSeconds = (now - previousMs) / 1000.0f;
  previousMs = now;

  // Avoid a huge jump after blocking operations such as identify mode.
  deltaSeconds = constrain(deltaSeconds, 0.0f, 0.1f);

  newStartsPerSecond = max(0.0f, newStartsPerSecond);
  risePerSecond = max(0.01f, risePerSecond);
  fadePerSecond = max(0.01f, fadePerSecond);
  peakBrightness = constrain(peakBrightness, 0.0f, 255.0f);

  // Accumulate fractional starts so frequency remains independent of frame rate.
  startAccumulator += newStartsPerSecond * deltaSeconds;

  while (startAccumulator >= 1.0f) {
    startRandomPixelGlow();
    startAccumulator -= 1.0f;
  }

  // Add some randomness to the exact start times while retaining approximately
  // the requested average frequency.
  if (startAccumulator > 0.0f) {
    const float chance = startAccumulator;

    if (random(10000) < chance * 10000.0f) {
      startRandomPixelGlow();
      startAccumulator = 0.0f;
    }
  }

  const uint8_t stripCount = activeStripCount();
  const uint16_t pixelCount = activeLedCountPerStrip();

  for (uint8_t strip = 0; strip < stripCount; strip++) {
    for (uint16_t pixel = 0; pixel < pixelCount; pixel++) {
      PixelGlow& glow = pixelGlows[strip][pixel];

      if (glow.phase == GLOW_INACTIVE) {
        continue;
      }

      if (glow.phase == GLOW_RISING) {
        glow.brightness += risePerSecond * deltaSeconds;

        if (glow.brightness >= peakBrightness) {
          glow.brightness = peakBrightness;
          glow.phase = GLOW_FADING;
        }
      } else {
        glow.brightness -= fadePerSecond * deltaSeconds;

        if (glow.brightness <= 0.0f) {
          glow.brightness = 0.0f;
          glow.phase = GLOW_INACTIVE;
          continue;
        }
      }

      const RgbColor fullColor = wheel(glow.hue);
      const RgbColor glowColor =
          rgbWithIntensity(fullColor, glow.brightness / 255.0f);

      const RgbColor base =
          strips[strip].GetPixelColor(pixel);

      strips[strip].SetPixelColor(
          pixel,
          addRgbSaturated(base, glowColor)
      );
    }
  }
}

void startRandomPixelGlow() {
  const uint8_t stripCount = activeStripCount();
  const uint16_t pixelCount = activeLedCountPerStrip();

  // Try several random pixels so an already-active pixel does not prevent
  // a new glow from starting.
  for (uint8_t attempt = 0; attempt < 12; attempt++) {
    const uint8_t strip = random(stripCount);
    const uint16_t pixel = random(pixelCount);

    PixelGlow& glow = pixelGlows[strip][pixel];

    if (glow.phase == GLOW_INACTIVE) {
      glow.hue = random(256);
      glow.brightness = 0.0f;
      glow.phase = GLOW_RISING;
      return;
    }
  }
}