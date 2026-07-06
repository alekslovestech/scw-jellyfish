#include "color_utils.h"

static uint8_t toByte(float v) {
  if (v <= 0.0f) return 0;
  if (v >= 255.0f) return 255;
  return (uint8_t)v;
}

RgbColor rgbWithIntensity(RgbColor c, float intensity) {
  return RgbColor(toByte(c.R * intensity), toByte(c.G * intensity), toByte(c.B * intensity));
}

RgbColor rgbWithIntensity(float r, float g, float b, float intensity) {
  float s = intensity * 255.0f;
  return RgbColor(toByte(r * s), toByte(g * s), toByte(b * s));
}
