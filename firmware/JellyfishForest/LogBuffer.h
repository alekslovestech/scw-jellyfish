#pragma once

#include <Arduino.h>
#include <stdarg.h>

namespace jelly {

class LogBuffer {
 public:
  void begin(unsigned long baud = 115200);
  void print(const String& value);
  void println(const String& value);
  void printf(const char* format, ...);
  String snapshot() const;

 private:
  void append(const String& value);
  void trim();
  String data_;
};

extern LogBuffer Log;

}  // namespace jelly
