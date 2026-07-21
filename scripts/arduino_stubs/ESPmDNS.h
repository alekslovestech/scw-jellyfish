#pragma once
#include "Arduino.h"
class MDNSClass {
 public:
  void end() {}
  bool begin(const char*) { return true; }
  void addService(const char*,const char*,uint16_t) {}
  void addServiceTxt(const char*,const char*,const char*,const String&) {}
};
extern MDNSClass MDNS;
