#include <NeoPixelBus.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <math.h>
#include <Arduino.h>
#include "version.h"
#include <array>
#include "HX711.h"


using StripBus = NeoPixelBus<NeoBrgFeature, NeoEsp32LcdX8Ws2812xMethod>;

HX711 scale;
const int HX711_DOUT = 13;
const int HX711_SCK  = 14;
float calibration_factor = -14850;
bool hasScale = false;
bool isJelly = false;

constexpr uint16_t NUM_LEDS_PER_STRIP = 50;
constexpr uint16_t NUM_STRIPS = 14;

struct Pos2D {
  float x;
  float y;
};

struct Pos3D {
  float x;
  float y;
  float z;
};

struct Pos3D ledPos[8][NUM_LEDS_PER_STRIP];


int PINS[NUM_STRIPS] = {
  4,5,6,7,9,10,11,12,15,16,17,18
}; // consider skipping pin 8
StripBus strips[] = {
  {NUM_LEDS_PER_STRIP, PINS[0]},
  {NUM_LEDS_PER_STRIP, PINS[1]},
  {NUM_LEDS_PER_STRIP, PINS[2]},
  {NUM_LEDS_PER_STRIP, PINS[3]},
  {NUM_LEDS_PER_STRIP, PINS[4]},
  {NUM_LEDS_PER_STRIP, PINS[5]},
  {NUM_LEDS_PER_STRIP, PINS[6]},
  {NUM_LEDS_PER_STRIP, PINS[7]},
  {NUM_LEDS_PER_STRIP, PINS[8]},
  {NUM_LEDS_PER_STRIP, PINS[9]},
  {NUM_LEDS_PER_STRIP, PINS[10]},
  {NUM_LEDS_PER_STRIP, PINS[11]},
 // {NUM_LEDS_PER_STRIP, PINS[12]},
 // {NUM_LEDS_PER_STRIP, PINS[13]},
 // {NUM_LEDS_PER_STRIP, PINS[14]},
 // {NUM_LEDS_PER_STRIP, PINS[15]},
};

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
  Serial.println("test");
  logPrintln("Test log");
  loadIdentity();

  for (int i=0; i<NUM_STRIPS; i++) {
    strips[i].Begin();
    strips[i].Show();
  } 

  connectToAnyWiFi();
  lastWiFiAttempt = millis();

  logPrintln("Boot complete");
  logPrint("Running firmware ");
  logPrintln(FW_VERSION);
  if (WiFi.status() == WL_CONNECTED) {
    logPrint("Open: http://");
    logPrintln(WiFi.localIP());
    logPrint("Or:   http://");
    logPrint(deviceName);
    logPrintln(".local");
  } else {
    logPrintln("Running offline; LEDs active, Wi-Fi will retry later.");
  }
  //if (deviceName.indexOf("jelly") >= 0) isJelly = true;
  //if (deviceName == "sender") hasScale = true;
  if (hasScale) {
    scale.begin(HX711_DOUT, HX711_SCK);
    scale.set_scale(calibration_factor);
    scale.tare();  // zero the scale
  }

  logPrintln("Calculating LED positions");
  for (int s = 0; s < 8; s++) {
    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
        ledPos[s][p] = smallJellyFind3Dpos(p, s);
    }
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
  //logPrintln(scale.get_units(1)); 
  //logPrintf("RSSI: %d dBm\n", WiFi.RSSI());
}
