#pragma once
#include "Arduino.h"
constexpr size_t UPDATE_SIZE_UNKNOWN = 0;
class UpdateClass {
 public:
  bool begin(size_t) { return true; }
  size_t write(uint8_t*, size_t size) { return size; }
  bool end(bool=false) { return true; }
  void abort() {}
  bool hasError() const { return false; }
};
extern UpdateClass Update;
