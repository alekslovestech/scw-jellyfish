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

using StripBus = NeoPixelBus<NeoGrbFeature, NeoEsp32LcdX8Ws2812xMethod>;


const int HX711_DOUT = 4;
const int HX711_SCK  = 5;
float calibration_factor = -14505;

constexpr uint16_t NUM_LEDS_PER_STRIP = 50;
constexpr uint16_t NUM_STRIPS = 16;

constexpr uint8_t PIN0  = 4;
constexpr uint8_t PIN1  = 5;
constexpr uint8_t PIN2  = 6;
constexpr uint8_t PIN3  = 7;
constexpr uint8_t PIN4  = 8;
constexpr uint8_t PIN5  = 9;
constexpr uint8_t PIN6  = 10;
constexpr uint8_t PIN7  = 11;
constexpr uint8_t PIN8  = 12;
constexpr uint8_t PIN9  = 13;
constexpr uint8_t PIN10 = 14;
constexpr uint8_t PIN11 = 15;
constexpr uint8_t PIN12 = 16;
constexpr uint8_t PIN13 = 17;
constexpr uint8_t PIN14 = 18;
constexpr uint8_t PIN15 = 21;

StripBus strip0(NUM_LEDS_PER_STRIP, PIN0);
StripBus strip1(NUM_LEDS_PER_STRIP, PIN1);
StripBus strip2(NUM_LEDS_PER_STRIP, PIN2);
StripBus strip3(NUM_LEDS_PER_STRIP, PIN3);
StripBus strip4(NUM_LEDS_PER_STRIP, PIN4);
StripBus strip5(NUM_LEDS_PER_STRIP, PIN5);
StripBus strip6(NUM_LEDS_PER_STRIP, PIN6);
StripBus strip7(NUM_LEDS_PER_STRIP, PIN7);
StripBus strip8(NUM_LEDS_PER_STRIP, PIN8);
StripBus strip9(NUM_LEDS_PER_STRIP, PIN9);
StripBus strip10(NUM_LEDS_PER_STRIP, PIN10);
StripBus strip11(NUM_LEDS_PER_STRIP, PIN11);
StripBus strip12(NUM_LEDS_PER_STRIP, PIN12);
StripBus strip13(NUM_LEDS_PER_STRIP, PIN13);
StripBus strip14(NUM_LEDS_PER_STRIP, PIN14);
StripBus strip15(NUM_LEDS_PER_STRIP, PIN15);

std::array<StripBus*, NUM_STRIPS> strips = {
  &strip0, &strip1, &strip2, &strip3,
  &strip4, &strip5, &strip6, &strip7,
  &strip8, &strip9, &strip10, &strip11,
  &strip12, &strip13, &strip14, &strip15
};

template <typename Fn>
void forEachStrip(Fn&& fn) {
  for (auto* strip : strips) {
    fn(*strip);
  }
}

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
  
  forEachStrip([](StripBus& strip) {
    strip.Begin();
    strip.Show();   // optional, but often useful to start with LEDs off
  });

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
