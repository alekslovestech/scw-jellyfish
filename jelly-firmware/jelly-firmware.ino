#include <NeoPixelBus.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <math.h>
#include <Arduino.h>
#include "version.h"
#include "config.h"
#include "HX711.h"

// ID 0 means "not configured yet".
uint8_t deviceId = 0;

using StripBus = NeoPixelBus<NeoBrgFeature, NeoEsp32LcdX8Ws2812xMethod>;

float musicLevel = 0.0;
float musicBass = 0.0;
float musicMid = 0.0;
float musicHigh = 0.0;
bool musicBeat = false;

float heartbeatLevel = 0.0;
bool heartbeatBeat = false;
int heartbeatHue = 0;
float heartbeatPhase = 0.0;

unsigned long lastHeartbeatPacketMs = 0;

HX711 scale;
float calibration_factor = -14850;
bool hasScale = false;
bool isJelly = false;
bool isBig = false;
String currentPattern = "demo";   // heartbeat | demo | ripple | fireSpread | waterfall | twoToneDiffuse | fallingRain | movementSimulation | innerSpreadWave | rainbowWave

float localWeight = 0.0f;
float localWeightSmooth = 0.0f;
float localAgitation = 0.0f;
float localCalmness = 1.0f;

unsigned long lastScaleSampleMs = 0;
unsigned long lastScaleBroadcastMs = 0;

struct ScalePeer {
  bool active;
  String chip;
  String name;
  IPAddress ip;

  float weight;
  float agitation;
  float calmness;

  unsigned long lastSeenMs;
};

ScalePeer scalePeers[MAX_SCALE_PEERS];

// Defined here (concatenated first) so fireSpread.ino sees them — variables, unlike
// functions, are not auto-prototyped across the sketch's alphabetical tab order.
int   _ledFrame = 0;          // shared animation frame counter
int   _jellyId = 0;           // which jellyfish this device renders (0 = hero, ignites first)
float _fireStartTime = -1.0f; // fireSpread ignition clock

struct Pos2D {
  float x;
  float y;
};

struct Pos3D {
  float x;
  float y;
  float z;
};

struct Pos3D devicePos = {0.0f, 0.0f, 0.0f};

//positions of (num of strips * number of leds)
struct Pos3D ledPos[NUM_SHORT_STRIPS][NUM_LEDS_PER_STRIP];
struct Pos3D ledPosLong[NUM_LONG_STRIPS][NUM_LEDS_PER_LONG_STRIP];

StripBus strips[NUM_STRIPS] = {
    {NUM_LEDS_PER_STRIP,      PINS[0]},
    {NUM_LEDS_PER_STRIP,      PINS[1]},
    {NUM_LEDS_PER_STRIP,      PINS[2]},
    {NUM_LEDS_PER_STRIP,      PINS[3]},
    {NUM_LEDS_PER_STRIP,      PINS[4]},
    {NUM_LEDS_PER_STRIP,      PINS[5]},
    {NUM_LEDS_PER_STRIP,      PINS[6]},
    {NUM_LEDS_PER_STRIP,      PINS[7]},

    {NUM_LEDS_PER_LONG_STRIP, PINS[8]},
    {NUM_LEDS_PER_LONG_STRIP, PINS[9]},
    {NUM_LEDS_PER_LONG_STRIP, PINS[10]},
    {NUM_LEDS_PER_LONG_STRIP, PINS[11]}
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

  deviceId = prefs.getUChar("id", 0);
  deviceName = prefs.getString("name", "");

  if (deviceName.length() == 0) {
    deviceName = "esp-" + chipIdHex.substring(chipIdHex.length() - 6);
    deviceName.toLowerCase();
    prefs.putString("name", deviceName);
  }

  prefs.end();

  if (isValidDeviceId(deviceId)) {
    _jellyId = deviceId - 1;
  } else {
    _jellyId = 0;
  }
}

void saveDeviceName(const String& newName) {
  String cleaned = sanitizeDeviceName(newName);
  prefs.begin("device", false);
  prefs.putString("name", cleaned);
  prefs.end();
  deviceName = cleaned;
}

void loadDeviceConfig() {
  prefs.begin("config", false);
  hasScale = prefs.getBool("hasScale", false);
  isJelly  = prefs.getBool("isJelly", false);
  isBig = prefs.getBool("isBig, false");
  currentPattern = prefs.getString("pattern", "demo");
  devicePos.x = prefs.getFloat("posX", 0.0);
  devicePos.y = prefs.getFloat("posY", 0.0);
  devicePos.z = prefs.getFloat("posZ", 0.0);
  prefs.end();
}

void saveDeviceConfig(bool newHasScale, bool newIsJelly, bool newIsBig) {
  prefs.begin("config", false);
  prefs.putBool("hasScale", newHasScale);
  prefs.putBool("isJelly", newIsJelly);
  prefs.putBool("isBig", newIsBig);
  prefs.end();

  hasScale = newHasScale;
  isJelly = newIsJelly;
  isBig = newIsBig;
}

void savePattern(const String& p) {
  prefs.begin("config", false);
  prefs.putString("pattern", p);
  prefs.end();
  currentPattern = p;
}

void saveDevicePos(Pos3D pos) {
  prefs.begin("config", false);

  prefs.putFloat("posX", pos.x);
  prefs.putFloat("posY", pos.y);
  prefs.putFloat("posZ", pos.z);

  prefs.end();

  devicePos = pos;
}

uint8_t activeStripCount()
{
    return isBig ? NUM_STRIPS : NUM_SHORT_STRIPS;
}

void setupStrips()
{
    const uint8_t count = activeStripCount();

    for (uint8_t s = 0; s < count; s++) {
        strips[s].Begin();
        strips[s].ClearTo(RgbColor(0));
    }

    // Calling Show() on every initialized member completes
    // the parallel-group update.
    for (uint8_t s = 0; s < count; s++) {
        strips[s].Show();
    }
}

bool initScaleWithTimeout(unsigned long timeoutMs) {
  scale.begin(HX711_DOUT, HX711_SCK);

  unsigned long start = millis();
  while (!scale.is_ready()) {
    if (millis() - start >= timeoutMs) {
      logPrintln("HX711 not ready. Disabling scale for this boot.");
      return false;
    }

    // Keep Wi-Fi/web server responsive while waiting.
    if (webServerStarted && WiFi.status() == WL_CONNECTED) {
      server.handleClient();
    }

    delay(10);
  }

  scale.set_scale(calibration_factor);

  logPrintln("HX711 ready. Taring scale...");

  // HX711 library tare() can block because it reads multiple samples.
  // Since is_ready() succeeded, this should usually be safe, but keep reads low.
  scale.tare(3);

  logPrintln("Scale initialized.");
  return true;
}

// ── Main ──────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(200);

  loadIdentity();
  loadDeviceConfig();

  logPrint("Config hasScale: ");
  logPrintln(hasScale ? "true" : "false");

  logPrint("Config isJelly: ");
  logPrintln(isJelly ? "true" : "false");

  logPrint("Config isBig: ");
  logPrintln(isBig ? "true" : "false");

  logPrint("Device position: ");
  logPrint(devicePos.x);
  logPrint(", ");
  logPrint(devicePos.y);
  logPrint(", ");
  logPrintln(devicePos.z);


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

  if (hasScale) {
    bool scaleOk = initScaleWithTimeout(HX711_READY_TIMEOUT_MS);

    if (!scaleOk) {
      // Important: do NOT overwrite the saved hasScale setting here.
      // This keeps the dashboard showing the intended config, but prevents
      // the current boot from hanging.
      hasScale = false;
    }
  }

  logPrintln("Calculating LED positions");
  for (int s = 0; s < 8; s++) {
    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
        ledPos[s][p] = smallJellyFind3Dpos(p, s);
    }
  }

  setupMusicUdp();
  setupScaleUdp();

}

void loop() {
  if (webServerStarted && WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }

  ensureWiFi();
  handleMusicUdp();

  updateLocalScaleState();
  broadcastScaleState();
  handleScaleUdp();
  expireScalePeers();

  if (identifyRequested) {
    identifyRequested = false;
    runIdentifySequence();
  }
  ledsTick();
  //logPrintln(scale.get_units(1)); 
  //logPrintf("RSSI: %d dBm\n", WiFi.RSSI());
}
