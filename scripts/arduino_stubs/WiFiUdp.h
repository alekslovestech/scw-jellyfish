#pragma once
#include "WiFi.h"
class WiFiUDP {
 public:
  void stop() {}
  uint8_t begin(uint16_t) { return 1; }
  int parsePacket() { return 0; }
  int read(char*, int) { return 0; }
  int beginPacket(const IPAddress&, uint16_t) { return 1; }
  size_t write(const uint8_t*, size_t size) { return size; }
  int endPacket() { return 1; }
};
