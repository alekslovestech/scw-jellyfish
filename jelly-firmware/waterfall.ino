// waterfall — ported from visualizer-ts/src/animations/waterfall.ts
// Uses fsSegmentForPos()/FS_SEG_*/fsToByte() (shared, in leds.ino / config.h).
void waterfall() {
  const float WAVE_SPEED = 1.5f;
  const float WAVE_WIDTH = 0.35f;
  const float WAVE_SPACING = 0.15f;
  const float COLOR_DARK_B = 0.3f;
  const float COLOR_BRIGHT_B = 1.0f;

  float time = millis() / 1000.0f;

  for (int s = 0; s < 8; s++) {
    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
      int group;
      float t;
      fsSegmentForPos(p, &group, &t);

      float intensity = 0.0f;
      float colorB = COLOR_DARK_B;

      if (group == FS_SEG_BELL && t < 0.5f) {
        intensity = 1.0f;
        colorB = COLOR_BRIGHT_B;
      } else {
        // Inner tentacles: t runs tip->root, so flip it to match root->tip flow.
        float normalizedPosition = (group == FS_SEG_INNER) ? (1.0f - t) : t;
        float wavePhase = fmodf(time * WAVE_SPEED - normalizedPosition, 1.0f + WAVE_SPACING);
        float wavePosition = fmodf(wavePhase, 1.0f);
        if (wavePosition < WAVE_WIDTH) {
          float waveIntensity = 1.0f - wavePosition / WAVE_WIDTH;
          intensity = waveIntensity;
          colorB = COLOR_DARK_B + (COLOR_BRIGHT_B - COLOR_DARK_B) * waveIntensity;
        } else {
          intensity = 0.25f;
          colorB = COLOR_DARK_B;
        }
      }

      // TS bakes intensity into green once via setRGB, then multiplies the whole
      // color by intensity again — so green is 0.2*intensity^2, blue colorB*intensity.
      strips[s].SetPixelColor(p, RgbColor(
        0,
        fsToByte(0.2f * intensity * intensity * 255.0f),
        fsToByte(colorB * intensity * 255.0f)
      ));
    }
  }
}
