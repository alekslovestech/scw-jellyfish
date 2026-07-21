#pragma once
#include "Arduino.h"
struct NeoBrgFeature {};
struct X8Ws2812xMethod {};
class RgbColor {
 public:
  uint8_t R=0,G=0,B=0;
  RgbColor() = default;
  explicit RgbColor(uint8_t v):R(v),G(v),B(v){}
  RgbColor(uint8_t r,uint8_t g,uint8_t b):R(r),G(g),B(b){}
};
template <typename Feature, typename Method>
class NeoPixelBus {
 public:
  NeoPixelBus(uint16_t, uint8_t) {}
  void Begin() {}
  void ClearTo(const RgbColor&) {}
  void Show() {}
  void SetPixelColor(uint16_t, const RgbColor&) {}
  RgbColor GetPixelColor(uint16_t) const { return {}; }
};
