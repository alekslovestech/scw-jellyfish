#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <math.h>

#define FW_VERSION "v1.0.5"
#define PIN_WS2812B 4
#define NUM_PIXELS 50

struct WifiCredential {
  const char* ssid;
  const char* password;
};

WifiCredential wifiList[] = {
  {"Nanonet2", "Sgrunterundt"},
  {"TP-Link_2.4GHz_9EC673", ""},
  {"Airties_Air4960R_CK74", "kptfyk9397"}
};

const int WIFI_COUNT = sizeof(wifiList) / sizeof(wifiList[0]);
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 8000;
const unsigned long WIFI_RETRY_INTERVAL_MS = 30000;

WebServer server(80);
Preferences prefs;
Adafruit_NeoPixel ws2812b(NUM_PIXELS, PIN_WS2812B, NEO_BRG + NEO_KHZ800);

String deviceName;
String chipIdHex;
bool identifyRequested = false;
bool webServerStarted = false;
bool mdnsStarted = false;

int i = 0;
unsigned long lastTime = 0;
unsigned long lastWiFiAttempt = 0;

String currentSSID = "";

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
           (uint8_t)(mac >> 40),
           (uint8_t)(mac >> 32),
           (uint8_t)(mac >> 24),
           (uint8_t)(mac >> 16),
           (uint8_t)(mac >> 8),
           (uint8_t)(mac));
  return String(buf);
}

String sanitizeDeviceName(String name) {
  name.trim();
  name.toLowerCase();

  String out = "";
  for (size_t idx = 0; idx < name.length(); idx++) {
    char c = name[idx];
    bool ok =
      (c >= 'a' && c <= 'z') ||
      (c >= '0' && c <= '9') ||
      c == '-' || c == '_';

    if (ok) {
      out += c;
    } else if (c == ' ') {
      out += '-';
    }
  }

  while (out.indexOf("--") >= 0) {
    out.replace("--", "-");
  }

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

String htmlEscape(const String& in) {
  String out = in;
  out.replace("&", "&amp;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  out.replace("\"", "&quot;");
  return out;
}

String wifiStatusText() {
  if (WiFi.status() == WL_CONNECTED) {
    return "Connected to " + currentSSID + " (" + WiFi.localIP().toString() + ")";
  }
  return "Not connected";
}

String buildHtmlPage(const String& message = "") {
  String html;
  html += "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>ESP32 OTA</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;max-width:760px;margin:24px auto;padding:0 16px;line-height:1.5;}";
  html += "h1,h2{margin-bottom:8px;}";
  html += ".card{border:1px solid #ccc;border-radius:12px;padding:16px;margin:16px 0;}";
  html += "input[type=text],input[type=file]{width:100%;padding:10px;margin:8px 0;}";
  html += "input[type=submit],button{padding:10px 16px;font-size:16px;cursor:pointer;}";
  html += ".muted{color:#555;}";
  html += ".msg{padding:10px;border-radius:8px;background:#f2f2f2;margin:12px 0;}";
  html += "code{background:#f7f7f7;padding:2px 6px;border-radius:6px;}";
  html += "</style></head><body>";

  html += "<h1>ESP32-S3 OTA Control</h1>";

  if (message.length() > 0) {
    html += "<div class='msg'>" + htmlEscape(message) + "</div>";
  }

  html += "<div class='card'>";
  html += "<h2>Device info</h2>";
  html += "<p><b>Name:</b> " + htmlEscape(deviceName) + "</p>";
  html += "<p><b>Firmware:</b> " FW_VERSION "</p>";
  html += "<p><b>Chip ID:</b> " + chipIdHex + "</p>";
  html += "<p><b>MAC:</b> " + getMacAddressString() + "</p>";
  html += "<p><b>Wi-Fi:</b> " + htmlEscape(wifiStatusText()) + "</p>";
  if (WiFi.status() == WL_CONNECTED) {
    html += "<p><b>Hostname:</b> <code>" + htmlEscape(deviceName) + ".local</code></p>";
  }
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Identify this device</h2>";
  html += "<form method='POST' action='/identify'>";
  html += "<input type='submit' value='Blink LEDs'>";
  html += "</form>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Rename device</h2>";
  html += "<form method='POST' action='/rename'>";
  html += "<input type='text' name='name' value='" + htmlEscape(deviceName) + "'>";
  html += "<input type='submit' value='Save name and reboot'>";
  html += "</form>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Firmware update</h2>";
  html += "<p class='muted'>Upload the compiled <code>.bin</code> file from Arduino IDE.</p>";
  html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
  html += "<input type='file' name='update'>";
  html += "<input type='submit' value='Install update'>";
  html += "</form>";
  html += "</div>";

  html += "</body></html>";
  return html;
}

void handleRoot() {
  server.send(200, "text/html", buildHtmlPage());
}

void handleRename() {
  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "Missing 'name' field");
    return;
  }

  String cleaned = sanitizeDeviceName(server.arg("name"));
  saveDeviceName(cleaned);

  server.send(200, "text/html", buildHtmlPage("Saved device name as '" + cleaned + "'. Rebooting..."));
  delay(1000);
  ESP.restart();
}

void handleIdentify() {
  identifyRequested = true;
  server.send(200, "text/html", buildHtmlPage("Identify sequence requested."));
}

void setupWebServer() {
  if (webServerStarted) {
    return;
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/rename", HTTP_POST, handleRename);
  server.on("/identify", HTTP_POST, handleIdentify);

  server.on("/update", HTTP_POST, []() {
    bool ok = !Update.hasError();
    server.send(200, "text/html", buildHtmlPage(ok ? "Update successful. Rebooting..." : "Update failed."));
    delay(1000);
    if (ok) {
      ESP.restart();
    }
  }, []() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update start: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      size_t written = Update.write(upload.buf, upload.currentSize);
      if (written != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("Update success: %u bytes\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      Update.abort();
      Serial.println("Update aborted");
    }
  });

  server.begin();
  webServerStarted = true;
  Serial.println("Web server started");
}

void startMDNSIfNeeded() {
  if (WiFi.status() != WL_CONNECTED || mdnsStarted) {
    return;
  }

  if (MDNS.begin(deviceName.c_str())) {
    mdnsStarted = true;
    Serial.printf("mDNS started: http://%s.local\n", deviceName.c_str());
  } else {
    Serial.println("mDNS failed to start");
  }
}

bool tryConnectSingleWiFi(const char* ssid, const char* password) {
  Serial.print("Trying Wi-Fi: ");
  Serial.println(ssid);

  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(deviceName.c_str());
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    if (WiFi.status() == WL_CONNECTED) {
      currentSSID = ssid;
      Serial.print("Connected to ");
      Serial.println(ssid);
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      return true;
    }
    delay(250);
  }

  Serial.print("Failed: ");
  Serial.println(ssid);
  return false;
}

bool connectToAnyWiFi() {
  for (int n = 0; n < WIFI_COUNT; n++) {
    if (tryConnectSingleWiFi(wifiList[n].ssid, wifiList[n].password)) {
      startMDNSIfNeeded();
      setupWebServer();
      return true;
    }
  }

  currentSSID = "";
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  Serial.println("No Wi-Fi networks available, continuing offline.");
  return false;
}

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (millis() - lastWiFiAttempt < WIFI_RETRY_INTERVAL_MS) {
    return;
  }

  lastWiFiAttempt = millis();
  connectToAnyWiFi();
}

void runIdentifySequence() {
  for (int k = 0; k < 5; k++) {
    ws2812b.fill(ws2812b.Color(180, 180, 180));
    ws2812b.show();
    delay(180);

    ws2812b.clear();
    ws2812b.show();
    delay(180);
  }
}

void drawFrame() {
  for (int p = 0; p < NUM_PIXELS; p++) {
     float val = float(max(double(0),sin(p * 0.832 - i * 0.03) * 10));
     int value = int(val*val);
    ws2812b.setPixelColor(p, ws2812b.Color(0,value, value));
  }
  ws2812b.show();
  i++;
}

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

  if (millis() - lastTime > 30) {
    lastTime = millis();
    drawFrame();
  }
}