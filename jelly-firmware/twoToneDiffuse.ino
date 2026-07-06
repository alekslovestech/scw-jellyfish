// twoToneDiffuse — inner tentacles and outer (bell included) each hold a solid
// color; both drift through the full hue wheel (one lap / 20s), outer always
// 180° from inner. The inner/outer switch is eased via fsInnerOuterMix() (leds.ino).
void twoToneDiffuse() {
  const float CYCLE_SECONDS = 20.0f;

  float time = millis() / 1000.0f;
  float innerHue = fmodf(time / CYCLE_SECONDS, 1.0f);
  float outerHue = fmodf(innerHue + 0.5f, 1.0f);

  RgbColor innerColor = RgbColor(HslColor(innerHue, 1.0f, 0.5f));
  RgbColor outerColor = RgbColor(HslColor(outerHue, 1.0f, 0.5f));

  for (int s = 0; s < 8; s++) {
    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
      float mix = fsInnerOuterMix(p);
      uint8_t r = fsToByte(innerColor.R + (outerColor.R - innerColor.R) * mix);
      uint8_t g = fsToByte(innerColor.G + (outerColor.G - innerColor.G) * mix);
      uint8_t b = fsToByte(innerColor.B + (outerColor.B - innerColor.B) * mix);
      strips[s].SetPixelColor(p, RgbColor(r, g, b));
    }
  }
}
