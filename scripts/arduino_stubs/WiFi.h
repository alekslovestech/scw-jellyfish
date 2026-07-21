#pragma once
#include "Arduino.h"
constexpr int WIFI_STA=1;
constexpr int WL_CONNECTED=3;
class IPAddress {
 public:
  IPAddress() = default;
  IPAddress(uint8_t,uint8_t,uint8_t,uint8_t) {}
  uint8_t& operator[](size_t i) { return data_[i]; }
  uint8_t operator[](size_t i) const { return data_[i]; }
  String toString() const { return String("0.0.0.0"); }
 private: uint8_t data_[4]{};
};
class WiFiClass {
 public:
  void persistent(bool) {}
  void mode(int) {}
  void setAutoReconnect(bool) {}
  bool config(const IPAddress&,const IPAddress&,const IPAddress&,const IPAddress& dns = IPAddress(),const IPAddress& dns2 = IPAddress()) { (void)dns; (void)dns2; return true; }
  void setHostname(const char*) {}
  void begin(const char*,const char*) {}
  void disconnect(bool = false, bool = false) {}
  int status() const { return WL_CONNECTED; }
  IPAddress localIP() const { return {}; }
  IPAddress subnetMask() const { return IPAddress(255,255,255,0); }
  String SSID() const { return String(); }
  int RSSI() const { return 0; }
  String macAddress() const { return String(); }
};
extern WiFiClass WiFi;
