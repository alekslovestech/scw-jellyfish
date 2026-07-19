#include <HTTPClient.h>
#include <esp_wifi.h>
#include <WiFiUdp.h>
#include "config.h"

WiFiUDP udp;
WiFiUDP scaleUdp;
WiFiUDP globalRippleUdp;

constexpr uint16_t GLOBAL_RIPPLE_UDP_PORT = 4212;

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

String currentSSID = "";

// ── Connection ────────────────────────────────────────────────────────────────

bool tryConnectSingleWiFi(const char* ssid, const char* password) {
  logPrint("Trying Wi-Fi: ");
  logPrintln(ssid);

  WiFi.disconnect(true, true);
  delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(deviceName.c_str());

  IPAddress localIp = getDeviceIpAddress();
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns1(0, 0, 0, 0);
  IPAddress dns2(0, 0, 0, 0);

  if (!WiFi.config(localIp, gateway, subnet, dns1, dns2)) {
    logPrintln("Static IP configuration failed");
    return false;
  }

  logPrintf(
    "Device ID %u, requesting static IP %s\n",
    deviceId,
    localIp.toString().c_str()
  );

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
    String deviceIdText = String(deviceId);
    MDNS.addServiceTxt(
      "esp32art",
      "tcp",
      "id",
      deviceIdText.c_str()
);

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

void handleScaleUdp() {
  int packetSize = scaleUdp.parsePacket();
  if (!packetSize) return;

  char packet[192];
  int len = scaleUdp.read(packet, sizeof(packet) - 1);
  if (len <= 0) return;
  packet[len] = '\0';

  char kind[12];
  char chip[24];
  char name[40];
  float weight;
  float agitation;
  float calmness;
  unsigned long remoteMs;

  int matched = sscanf(
    packet,
    "%11[^,],%23[^,],%39[^,],%f,%f,%f,%lu",
    kind,
    chip,
    name,
    &weight,
    &agitation,
    &calmness,
    &remoteMs
  );

  if (matched != 7) return;
  if (String(kind) != "scale") return;

  String peerChip = String(chip);

  // Ignore our own broadcast.
  if (peerChip == chipIdHex) return;

  int idx = findScalePeerByChip(peerChip);
  if (idx < 0) {
    idx = findFreeScalePeerSlot();
  }

  scalePeers[idx].active = true;
  scalePeers[idx].chip = peerChip;
  scalePeers[idx].name = String(name);
  scalePeers[idx].ip = scaleUdp.remoteIP();

  scalePeers[idx].weight = weight;
  scalePeers[idx].agitation = constrain(agitation, 0.0f, 1.0f);
  scalePeers[idx].calmness = constrain(calmness, 0.0f, 1.0f);

  scalePeers[idx].lastSeenMs = millis();
}


// ── Global ripple UDP --------------------------------------------------------

void setupGlobalRippleUdp() {
  globalRippleUdp.begin(GLOBAL_RIPPLE_UDP_PORT);
  logPrintf("Global ripple UDP listening on port %u\n", GLOBAL_RIPPLE_UDP_PORT);
}

void handleGlobalRippleUdp() {
  int packetSize = globalRippleUdp.parsePacket();
  if (!packetSize) return;

  char packet[160];
  int len = globalRippleUdp.read(packet, sizeof(packet) - 1);
  if (len <= 0) return;
  packet[len] = '\0';

  char kind[20];
  unsigned long eventId;
  float x, y, z, speed;
  int red, green, blue;
  unsigned long durationMs;

  // globalRipple,eventId,x,y,z,r,g,b,speed,durationMs
  int matched = sscanf(
    packet,
    "%19[^,],%lu,%f,%f,%f,%d,%d,%d,%f,%lu",
    kind, &eventId, &x, &y, &z, &red, &green, &blue, &speed, &durationMs
  );

  if (matched != 10 || String(kind) != "globalRipple") return;

  Pos3D origin = {x, y, z};
  RgbColor color(
    constrain(red, 0, 255),
    constrain(green, 0, 255),
    constrain(blue, 0, 255)
  );
  startGlobalRipple((uint32_t)eventId, origin, color, speed, durationMs);
}

void broadcastGlobalRipple(const Pos3D& origin,
                           RgbColor color,
                           float speed,
                           unsigned long durationMs) {
  if (WiFi.status() != WL_CONNECTED) return;

  uint32_t eventId = esp_random();
  if (eventId == 0) eventId = 1;

  String packet;
  packet.reserve(150);
  packet += "globalRipple,";
  packet += String(eventId) + ",";
  packet += String(origin.x, 3) + ",";
  packet += String(origin.y, 3) + ",";
  packet += String(origin.z, 3) + ",";
  packet += String(color.R) + ",";
  packet += String(color.G) + ",";
  packet += String(color.B) + ",";
  packet += String(speed, 3) + ",";
  packet += String(durationMs);

  IPAddress broadcastIp(255, 255, 255, 255);

  // Repeat packets reduce the chance of a single lost UDP datagram. Starting
  // locally after transmission also keeps the sender aligned with receivers.
  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    globalRippleUdp.beginPacket(broadcastIp, GLOBAL_RIPPLE_UDP_PORT);
    globalRippleUdp.write((const uint8_t*)packet.c_str(), packet.length());
    globalRippleUdp.endPacket();
    delay(4);
  }

  startGlobalRipple(eventId, origin, color, speed, durationMs);
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

int findScalePeerByChip(const String& chip) {
  for (int i = 0; i < MAX_SCALE_PEERS; i++) {
    if (scalePeers[i].active && scalePeers[i].chip == chip) {
      return i;
    }
  }
  return -1;
}

int findFreeScalePeerSlot() {
  for (int i = 0; i < MAX_SCALE_PEERS; i++) {
    if (!scalePeers[i].active) {
      return i;
    }
  }

  // If full, overwrite the stalest peer.
  int oldest = 0;
  for (int i = 1; i < MAX_SCALE_PEERS; i++) {
    if (scalePeers[i].lastSeenMs < scalePeers[oldest].lastSeenMs) {
      oldest = i;
    }
  }
  return oldest;
}

void expireScalePeers() {
  unsigned long now = millis();

  for (int i = 0; i < MAX_SCALE_PEERS; i++) {
    if (!scalePeers[i].active) continue;

    if (now - scalePeers[i].lastSeenMs > SCALE_PEER_TIMEOUT_MS) {
      scalePeers[i].active = false;
    }
  }
}

int getActiveScaleCount() {
  int count = 0;

  for (int i = 0; i < MAX_SCALE_PEERS; i++) {
    if (scalePeers[i].active) {
      count++;
    }
  }

  return count;
}

int getTotalActiveScaleCount() {
  return getActiveScaleCount() + (hasScale ? 1 : 0);
}

void broadcastScaleState() {
  if (!hasScale) return;
  if (WiFi.status() != WL_CONNECTED) return;

  unsigned long now = millis();
  if (now - lastScaleBroadcastMs < SCALE_BROADCAST_INTERVAL_MS) return;
  lastScaleBroadcastMs = now;

  IPAddress broadcastIp(255, 255, 255, 255);

  String packet;
  packet.reserve(160);

  // Format:
  // scale,<chip>,<name>,<weight>,<agitation>,<calmness>,<ms>
  packet += "scale,";
  packet += chipIdHex;
  packet += ",";
  packet += deviceName;
  packet += ",";
  packet += String(localWeight, 3);
  packet += ",";
  packet += String(localAgitation, 3);
  packet += ",";
  packet += String(localCalmness, 3);
  packet += ",";
  packet += String(now);

  scaleUdp.beginPacket(broadcastIp, SCALE_UDP_PORT);
  scaleUdp.write((const uint8_t*)packet.c_str(), packet.length());
  scaleUdp.endPacket();
}

bool isValidDeviceId(int id) {
  return id >= MIN_DEVICE_ID && id <= MAX_DEVICE_ID;
}

uint8_t getDeviceIpOctet() {
  if (isValidDeviceId(deviceId)) {
    return 100 + deviceId;
  }

  // Temporary address before an ID has been assigned.
  return UNASSIGNED_IP_OCTET;
}

IPAddress getDeviceIpAddress() {
  return IPAddress(192, 168, 0, getDeviceIpOctet());
}

void saveDeviceId(uint8_t newId) {
  if (!isValidDeviceId(newId)) {
    return;
  }

  prefs.begin("device", false);
  prefs.putUChar("id", newId);
  prefs.end();

  deviceId = newId;

  // Your animation code uses zero-based jelly IDs.
  _jellyId = deviceId - 1;
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
  json += "\"id\":" + String(deviceId) + ",";
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
  json += "\"isBig\":" + String(isBig ? "true" : "false") + ",";
  json += "\"pattern\":\"" + currentPattern + "\",";
  //json += "\"hasNumber\":" + String(hasReceivedNumber ? "true" : "false");
  json += "\"localWeight\":" + String(localWeight, 3);
  json += ",\"localAgitation\":" + String(localAgitation, 3);
  json += ",\"localCalmness\":" + String(localCalmness, 3);
  json += ",\"activeScales\":" + String(getTotalActiveScaleCount());
  json += ",\"posX\":" + String(devicePos.x) + ",";
  json += "\"posY\":" + String(devicePos.y) + ",";
  json += "\"posZ\":" + String(devicePos.z) + ",";
  json += "\"rotationY\":" + String(deviceRotationY);
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
  bool newIsBig = isBig;

  if (server.hasArg("hasScale")) {
    String v = server.arg("hasScale");
    newHasScale = (v == "1" || v == "true" || v == "on");
  }

  if (server.hasArg("isJelly")) {
    String v = server.arg("isJelly");
    newIsJelly = (v == "1" || v == "true" || v == "on");
  }

  if (server.hasArg("isBig")) {
    String v = server.arg("isBig");
    newIsBig = (v == "1" || v == "true" || v == "on");
  }

  saveDeviceConfig(newHasScale, newIsJelly, newIsBig);

  server.send(200, "application/json",
    String("{\"ok\":true,\"hasScale\":") + (hasScale ? "true" : "false") +
    ",\"isJelly\":" + (isJelly ? "true" : "false") +
    ",\"isBig\":" + (isBig ? "true" : "false") + "}"
  );
}

void handlePattern() {
  if (!server.hasArg("pattern")) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing 'pattern' field\"}");
    return;
  }
  savePattern(server.arg("pattern"));
  logPrint("Changed pattern to: ");
  logPrintln(currentPattern);
  server.send(200, "application/json",
    String("{\"ok\":true,\"pattern\":\"") + currentPattern + "\"}");
}

void handleGlobalRipple() {
  Pos3D origin = {0.0f, 0.0f, 0.0f};
  int red = 0;
  int green = 0;
  int blue = 255;
  float speed = 1.0f;
  unsigned long durationMs = 30000;

  if (server.hasArg("posX")) origin.x = server.arg("posX").toFloat();
  if (server.hasArg("posY")) origin.y = server.arg("posY").toFloat();
  if (server.hasArg("posZ")) origin.z = server.arg("posZ").toFloat();
  if (server.hasArg("r")) red = server.arg("r").toInt();
  if (server.hasArg("g")) green = server.arg("g").toInt();
  if (server.hasArg("b")) blue = server.arg("b").toInt();
  if (server.hasArg("speed")) speed = server.arg("speed").toFloat();
  if (server.hasArg("durationMs")) {
    durationMs = (unsigned long)server.arg("durationMs").toInt();
  }

  speed = constrain(speed, 0.05f, 20.0f);
  durationMs = constrain(durationMs, 1000UL, 120000UL);
  RgbColor color(
    constrain(red, 0, 255),
    constrain(green, 0, 255),
    constrain(blue, 0, 255)
  );

  broadcastGlobalRipple(origin, color, speed, durationMs);

  server.send(200, "application/json",
    String("{\"ok\":true,\"posX\":") + String(origin.x, 3) +
    ",\"posY\":" + String(origin.y, 3) +
    ",\"posZ\":" + String(origin.z, 3) +
    ",\"r\":" + String(color.R) +
    ",\"g\":" + String(color.G) +
    ",\"b\":" + String(color.B) +
    ",\"speed\":" + String(speed, 3) +
    ",\"durationMs\":" + String(durationMs) + "}"
  );
}

void handlePosition() {
  struct Pos3D newPos = devicePos;
  float newRotationY = deviceRotationY;

  if (server.hasArg("posX")) {
    newPos.x = server.arg("posX").toFloat();
  }

  if (server.hasArg("posY")) {
    newPos.y = server.arg("posY").toFloat();
  }

  if (server.hasArg("posZ")) {
    newPos.z = server.arg("posZ").toFloat();
  }

  if (server.hasArg("rotationY")) {
    newRotationY = server.arg("rotationY").toFloat();
  }

  saveDeviceTransform(newPos, newRotationY);

  server.send(200, "application/json",
    String("{\"ok\":true,\"posX\":") + String(devicePos.x, 3) +
    ",\"posY\":" + String(devicePos.y, 3) +
    ",\"posZ\":" + String(devicePos.z, 3) +
    ",\"rotationY\":" + String(deviceRotationY, 3) + "}"
  );
}

void handleDeviceId() {
  if (!server.hasArg("id")) {
    server.send(
      400,
      "application/json",
      "{\"ok\":false,\"error\":\"Missing 'id' field\"}"
    );
    return;
  }

  int requestedId = server.arg("id").toInt();

  if (!isValidDeviceId(requestedId)) {
    server.send(
      400,
      "application/json",
      String("{\"ok\":false,\"error\":\"ID must be between ") +
      MIN_DEVICE_ID + " and " + MAX_DEVICE_ID + "\"}"
    );
    return;
  }

  saveDeviceId((uint8_t)requestedId);

  IPAddress newIp = getDeviceIpAddress();

  String json = "{";
  json += "\"ok\":true,";
  json += "\"id\":" + String(deviceId) + ",";
  json += "\"ip\":\"" + newIp.toString() + "\",";
  json += "\"rebooting\":true";
  json += "}";

  server.send(200, "application/json", json);

  delay(500);
  ESP.restart();
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
  server.on("/pattern", HTTP_POST, handlePattern);
  server.on("/global-ripple", HTTP_POST, handleGlobalRipple);
  server.on("/position", HTTP_POST, handlePosition);
  server.on("/device-id", HTTP_POST, handleDeviceId);

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



