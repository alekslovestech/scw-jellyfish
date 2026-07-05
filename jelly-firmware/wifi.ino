#include <HTTPClient.h>
#include <esp_wifi.h>
#include <WiFiUdp.h>

WiFiUDP udp;
WiFiUDP scaleUdp;
const int MUSIC_UDP_PORT = 4210;
const int SCALE_UDP_PORT = 4211;

String lastSendStatus = "Ready";
bool hasReceivedNumber = false;
// ── Credentials ───────────────────────────────────────────────────────────────

struct WifiCredential {
  const char* ssid;
  const char* password;
};

WifiCredential wifiList[] = {
 // {"Nanonet2",              "Sgrunterundt"   },
  {"SCW", "Jellyfish"               },
 // {"Airties_Air4960R_CK74", "kptfyk9397"     },
};

const int WIFI_COUNT = sizeof(wifiList) / sizeof(wifiList[0]);
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 5000;
const unsigned long WIFI_RETRY_INTERVAL_MS  = 1000000;

String currentSSID = "";

// ── Connection ────────────────────────────────────────────────────────────────

bool tryConnectSingleWiFi(const char* ssid, const char* password) {
  logPrint("Trying Wi-Fi: ");
  logPrintln(ssid);

  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(deviceName.c_str());
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    if (WiFi.status() == WL_CONNECTED) {
      esp_wifi_set_ps(WIFI_PS_NONE);
      currentSSID = ssid;
      logPrint("Connected to "); logPrintln(ssid);
      logPrint("IP: ");          logPrintln(WiFi.localIP());
      return true;
    }
    delay(250);
  }
  logPrint("Failed: "); logPrintln(ssid);
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
  logPrintln("No Wi-Fi networks available, continuing offline.");
  return false;
}

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWiFiAttempt < WIFI_RETRY_INTERVAL_MS) return;
  lastWiFiAttempt = millis();
  connectToAnyWiFi();
}

void startMDNSIfNeeded() {
  if (WiFi.status() != WL_CONNECTED || mdnsStarted) return;
  if (MDNS.begin(deviceName.c_str())) {
    mdnsStarted = true;

    MDNS.addService("esp32art", "tcp", 80);
    MDNS.addServiceTxt("esp32art", "tcp", "name", deviceName.c_str());
    MDNS.addServiceTxt("esp32art", "tcp", "fw", FW_VERSION);
    MDNS.addServiceTxt("esp32art", "tcp", "chip", chipIdHex.c_str());
    MDNS.addServiceTxt("esp32art", "tcp", "mac", getMacAddressString().c_str());
    MDNS.addServiceTxt("esp32art", "tcp", "ssid", currentSSID.c_str());

    logPrintf("mDNS started: http://%s.local\n", deviceName.c_str());
  } else {
    logPrintln("mDNS failed to start");
  }
}



// UDP server for rapid sharing of eg music volume

void setupMusicUdp() {
  udp.begin(MUSIC_UDP_PORT);
  logPrintf("Music UDP listening on port %d\n", MUSIC_UDP_PORT);
}

void handleMusicUdp() {
  int packetSize = udp.parsePacket();
  if (!packetSize) return;

  char packet[128];
  int len = udp.read(packet, sizeof(packet) - 1);
  if (len <= 0) return;
  packet[len] = '\0';

  char mode[24];
  float level;
  int beat;
  int hue;
  float phase;

  // Expected:
  // heartbeat,0.843,1,0,0.032
  int matched = sscanf(packet, "%23[^,],%f,%d,%d,%f",
                       mode, &level, &beat, &hue, &phase);

  if (matched == 5 && String(mode) == "heartbeat") {
    if (heartbeatLevel==0) logPrint("New heartbeat detected");
    heartbeatLevel = constrain(level, 0.0, 1.0);
    heartbeatBeat = beat > 0;
    heartbeatHue = constrain(hue, 0, 255);
    heartbeatPhase = constrain(phase, 0.0, 1.0);
    lastHeartbeatPacketMs = millis();
  }
}

void setupScaleUdp() {
  scaleUdp.begin(SCALE_UDP_PORT);
  logPrintf("Scale UDP listening on port %d\n", SCALE_UDP_PORT);
}

// ── Web server + OTA 

String htmlEscape(const String& in) {
  String out = in;
  out.replace("&", "&amp;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  out.replace("\"", "&quot;");
  return out;
}

String wifiStatusText() {
  if (WiFi.status() == WL_CONNECTED)
    return "Connected to " + currentSSID + " (" + WiFi.localIP().toString() + ")";
  return "Not connected";
}

/*
// ── Send number to the other device ───────────────────────────────────────────
bool sendNumberToPeer(const String& host, float value) {
  if (WiFi.status() != WL_CONNECTED) {
    lastSendStatus = "Wi-Fi not connected";
    return false;
  }

  WiFiClient client;
  HTTPClient http;

  String url = "http://" + host + "/setNumber";
  if (!http.begin(client, url)) {
    lastSendStatus = "Failed to begin HTTP request";
    return false;
  }

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String body = "value=" + String(value);

  int code = http.POST(body);
  String response = http.getString();
  http.end();

  //logPrintf("POST %s -> %d\n", url.c_str(), code);
  //logPrintln(response);

  if (code >= 200 && code < 300) {
    lastSendStatus = "Sent " + String(value) + " to " + host;
    return true;
  } else {
    lastSendStatus = "Send failed, HTTP " + String(code);
    return false;
  }
}
*/

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

  if (message.length() > 0)
    html += "<div class='msg'>" + htmlEscape(message) + "</div>";

  html += "<div class='card'><h2>Device info</h2>";
  html += "<p><b>Name:</b> "     + htmlEscape(deviceName) + "</p>";
  html += "<p><b>Firmware:</b> " FW_VERSION "</p>";
  html += "<p><b>Chip ID:</b> "  + chipIdHex + "</p>";
  html += "<p><b>MAC:</b> "      + getMacAddressString() + "</p>";
  html += "<p><b>Wi-Fi:</b> "    + htmlEscape(wifiStatusText()) + "</p>";
  if (WiFi.status() == WL_CONNECTED)
    html += "<p><b>Hostname:</b> <code>" + htmlEscape(deviceName) + ".local</code></p>";
  html += "</div>";

  html += "<div class='card'><h2>Last received number</h2>";
  html += "<p style='font-size:2rem;margin:0;'><b>";
  html += hasReceivedNumber ? String(_agitation) : String("--");
  html += "</b></p>";
  html += "<p class='muted'>JSON endpoint: <code>/number</code></p>";
  html += "</div>";

  html += "<div class='card'><h2>Send a number</h2>";
  html += "<form method='POST' action='/sendNumber'>";
  html += "<label>Receiver host or IP</label>";
  html += "<input type='text' name='target' value='receiver.local'>";
  html += "<label>Number</label>";
  html += "<input type='number' name='value' value='123'>";
  html += "<input type='submit' value='Send'></form></div>";

  html += "<div class='card'><h2>Last send status</h2>";
  html += "<p>" + htmlEscape(lastSendStatus) + "</p>";
  html += "</div>";

  html += "<div class='card'><h2>Identify this device</h2>";
  html += "<form method='POST' action='/identify'>";
  html += "<input type='submit' value='Blink LEDs'></form></div>";

  html += "<div class='card'><h2>Rename device</h2>";
  html += "<form method='POST' action='/rename'>";
  html += "<input type='text' name='name' value='" + htmlEscape(deviceName) + "'>";
  html += "<input type='submit' value='Save name and reboot'></form></div>";

  html += "<div class='card'><h2>Firmware update</h2>";
  html += "<p class='muted'>Upload the compiled <code>.bin</code> file from Arduino IDE.</p>";
  html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
  html += "<input type='file' name='update'>";
  html += "<input type='submit' value='Install update'></form></div>";

  html += "</body></html>";
  return html;
}

void handleRoot()   { server.send(200, "text/html", buildHtmlPage()); }

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

void handleStatus() {
  String json = "{";
  json += "\"name\":\"" + deviceName + "\",";
  json += "\"firmware\":\"" FW_VERSION "\",";
  json += "\"chip\":\"" + chipIdHex + "\",";
  json += "\"mac\":\"" + getMacAddressString() + "\",";
  json += "\"ssid\":\"" + currentSSID + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + ",";
  //json += "\"number\":" + String(_agitation) + ",";
  json += "\"hasScale\":" + String(hasScale ? "true" : "false") + ",";
  json += "\"isJelly\":" + String(isJelly ? "true" : "false") + ",";
  //json += "\"hasNumber\":" + String(hasReceivedNumber ? "true" : "false");
  json += "\"posX\":" + String(devicePos.x) + ",";
  json += "\"posY\":" + String(devicePos.y) + ",";
  json += "\"posZ\":" + String(devicePos.z);
  json += "}";

  server.send(200, "application/json", json);
}

void handleIdentify() {
  identifyRequested = true;
  server.send(200, "text/html", buildHtmlPage("Identify sequence requested."));
}

/*void handleSendNumber() {
  if (!server.hasArg("target") || !server.hasArg("value")) {
    server.send(400, "text/plain", "Missing 'target' or 'value' field");
    return;
  }

  String target = server.arg("target");
  long value = server.arg("value").toInt();

  bool ok = sendNumberToPeer(target, value);
  server.send(200, "text/html",
              buildHtmlPage(ok ? ("Sent " + String(value) + " to " + target)
                               : ("Failed to send " + String(value) + " to " + target)));
}

void handleSetNumber() {
  if (!server.hasArg("value")) {
    server.send(400, "text/plain", "Missing 'value' field");
    return;
  }

  _agitation = server.arg("value").toFloat();
  hasReceivedNumber = true;

  logPrintf("Received number: %ld\n", _agitation);
  server.send(200, "text/html",
              buildHtmlPage("Received number: " + String(_agitation)));
}

void handleGetNumber() {
  String json = String("{\"number\":") + String(_agitation) +
                ",\"hasNumber\":" + (hasReceivedNumber ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}*/

void handleLog() {
  server.send(200, "text/plain", serialLog);
}

void handleConfig() {
  bool newHasScale = hasScale;
  bool newIsJelly = isJelly;

  if (server.hasArg("hasScale")) {
    String v = server.arg("hasScale");
    newHasScale = (v == "1" || v == "true" || v == "on");
  }

  if (server.hasArg("isJelly")) {
    String v = server.arg("isJelly");
    newIsJelly = (v == "1" || v == "true" || v == "on");
  }

  saveDeviceConfig(newHasScale, newIsJelly);

  server.send(200, "application/json",
    String("{\"ok\":true,\"hasScale\":") + (hasScale ? "true" : "false") +
    ",\"isJelly\":" + (isJelly ? "true" : "false") + "}"
  );
}

void handlePosition() {
  struct Pos3D newPos = devicePos;

  if (server.hasArg("posX")) {
    newPos.x = server.arg("posX").toFloat();
  }

  if (server.hasArg("posY")) {
    newPos.y = server.arg("posY").toFloat();
  }

  if (server.hasArg("posZ")) {
    newPos.z = server.arg("posZ").toFloat();
  }

  saveDevicePos(newPos);

  server.send(200, "application/json",
    String("{\"ok\":true,\"posX\":") + String(devicePos.x, 3) +
    ",\"posY\":" + String(devicePos.y, 3) +
    ",\"posZ\":" + String(devicePos.z, 3) + "}"
  );
}

void setupWebServer() {
  if (webServerStarted) return;

  server.on("/",        HTTP_GET,  handleRoot);
  server.on("/rename",  HTTP_POST, handleRename);
  server.on("/identify",HTTP_POST, handleIdentify);
  //server.on("/sendNumber", HTTP_POST, handleSendNumber);
  //server.on("/number", HTTP_GET, handleGetNumber);
  //server.on("/setNumber", HTTP_POST, handleSetNumber);
  server.on("/log", HTTP_GET, handleLog);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/config", HTTP_POST, handleConfig);

  server.on("/update", HTTP_POST, []() {
    bool ok = !Update.hasError();
    server.send(200, "text/html", buildHtmlPage(ok ? "Update successful. Rebooting..." : "Update failed."));
    delay(1000);
    if (ok) ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      logPrintf("Update start: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) logPrintf("Update success: %u bytes\n", upload.totalSize);
      else Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      Update.abort();
      logPrintln("Update aborted");
    }
  });

  server.begin();
  webServerStarted = true;
  logPrintln("Web server started");
}



