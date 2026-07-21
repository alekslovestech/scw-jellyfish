#pragma once
#include "Arduino.h"
class Preferences {
 public:
  bool begin(const char*, bool=false) { return true; }
  void end() {}
  bool isKey(const char*) { return false; }
  uint8_t getUChar(const char*, uint8_t value=0) { return value; }
  unsigned int getUInt(const char*, unsigned int value=0) { return value; }
  bool getBool(const char*, bool value=false) { return value; }
  float getFloat(const char*, float value=0) { return value; }
  String getString(const char*, const String& value=String()) { return value; }
  size_t putUChar(const char*, uint8_t) { return 1; }
  size_t putUInt(const char*, unsigned int) { return 1; }
  size_t putBool(const char*, bool) { return 1; }
  size_t putFloat(const char*, float) { return 1; }
  size_t putString(const char*, const String&) { return 1; }
};
