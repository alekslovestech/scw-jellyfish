#pragma once

#include <NeoPixelBus.h>

// Apply intensity/brightness to a color (visualizer color * intensity).
RgbColor rgbWithIntensity(RgbColor c, float intensity);
RgbColor rgbWithIntensity(float r, float g, float b, float intensity = 1.0f);
RgbColor wheel(uint8_t pos);