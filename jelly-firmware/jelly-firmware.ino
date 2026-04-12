#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <math.h>

#include "version.h"
#define PIN_WS2812B 4
#define NUM_PIXELS  50

Adafruit_NeoPixel ws2812b(NUM_PIXELS, PIN_WS2812B, NEO_BRG + NEO_KHZ800);
WebServer server(80);
Preferences prefs;

String deviceName;
String chipIdHex;
bool identifyRequested = false;
bool webServerStarted  = false;
bool mdnsStarted       = false;
unsigned long lastWiFiAttempt = 0;

// ── Device identity ───────────────────────────────────────────────────────────

String getChipIdHex() {
  uint64_t chipid = ESP.getEfuseMac();
  char buf[13];
  snprintf(buf, sizeof(buf), "%04X%08X",
           (uint16_t)(chipid >> 32),
           (uint32_t)chipid);
  return String(buf);
}

String getMacAddressString() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[18];
  snprintf(buf, sizeof(buf),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           (uint8_t)(mac >> 40), (uint8_t)(mac >> 32),
           (uint8_t)(mac >> 24), (uint8_t)(mac >> 16),
           (uint8_t)(mac >> 8),  (uint8_t)(mac));
  return String(buf);
}

String sanitizeDeviceName(String name) {
  name.trim();
  name.toLowerCase();
  String out = "";
  for (size_t idx = 0; idx < name.length(); idx++) {
    char c = name[idx];
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    if (ok)       out += c;
    else if (c == ' ') out += '-';
  }
  while (out.indexOf("--") >= 0) out.replace("--", "-");
  if (out.length() == 0) {
    out = "esp-" + chipIdHex.substring(chipIdHex.length() - 6);
    out.toLowerCase();
  }
  return out;
}

void loadIdentity() {
  chipIdHex = getChipIdHex();
  prefs.begin("device", false);
  deviceName = prefs.getString("name", "");
  if (deviceName.length() == 0) {
    deviceName = "esp-" + chipIdHex.substring(chipIdHex.length() - 6);
    deviceName.toLowerCase();
    prefs.putString("name", deviceName);
  }
  prefs.end();
}

void saveDeviceName(const String& newName) {
  String cleaned = sanitizeDeviceName(newName);
  prefs.begin("device", false);
  prefs.putString("name", cleaned);
  prefs.end();
  deviceName = cleaned;
}

// ── Main ──────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(200);

  loadIdentity();

  ws2812b.begin();
  ws2812b.clear();
  ws2812b.show();

  connectToAnyWiFi();
  lastWiFiAttempt = millis();

  Serial.println("Boot complete");
  Serial.print("Running firmware ");
  Serial.println(FW_VERSION);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Open: http://");
    Serial.println(WiFi.localIP());
    Serial.print("Or:   http://");
    Serial.print(deviceName);
    Serial.println(".local");
  } else {
    Serial.println("Running offline; LEDs active, Wi-Fi will retry later.");
  }
}

void loop() {
  if (webServerStarted && WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }

  ensureWiFi();

  if (identifyRequested) {
    identifyRequested = false;
    runIdentifySequence();
  }

  ledsTick();
}
