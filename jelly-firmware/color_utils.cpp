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

// convert a hue wheel position (0-255) into RGB via NeoPixelBus HslColor.
RgbColor wheel(uint8_t pos) {
  float hue = (255 - (pos % 256)) / 255.0f;
  return RgbColor(HslColor(hue, 1.0f, 0.5f));
}

//map integer from 0 to 665 to a color palete going from dark red through yellow to white
RgbColor fireColor(int i) { 
    if (i < 0) return RgbColor(0,0,0);
    else if (i < 255) return RgbColor(i, 0, 0);
    else if (i < 410) return RgbColor(255, i-255, 0);
    else if (i < 510) return RgbColor(255, i-255, i-410);
    else if (i < 665) return RgbColor(255, 255, i-410);
    else return RgbColor(255, 255, 255);
}

