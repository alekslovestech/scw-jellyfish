#pragma once
#include "Arduino.h"
class HX711 {
 public:
  void begin(uint8_t, uint8_t) {}
  bool is_ready() const { return true; }
  void set_scale(float=1.0f) {}
  void tare(uint8_t=10) {}
  float get_units(uint8_t=1) { return 0.0f; }
};
