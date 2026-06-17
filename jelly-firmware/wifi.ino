// ── Credentials ───────────────────────────────────────────────────────────────

struct WifiCredential {
  const char* ssid;
  const char* password;
};

WifiCredential wifiList[] = {
  {"Nanonet2",              "Sgrunterundt"   },
  {"TP-Link_2.4GHz_9EC673", ""               },
  {"Airties_Air4960R_CK74", "kptfyk9397"     },  
};

const unsigned long WIFI_CONNECT_TIMEOUT_MS = 3000;
const unsigned long WIFI_RETRY_INTERVAL_MS  = 300000;

String currentSSID = "";

// ── Connection ────────────────────────────────────────────────────────────────

// Only the preferred network (wifiList[0]) is ever tried — reorder the list
// above to change which one that is.
bool connectPreferredWiFi() {
  const WifiCredential& net = wifiList[0];
  Serial.print("Trying Wi-Fi: ");
  Serial.println(net.ssid);

  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(deviceName.c_str());
  WiFi.begin(net.ssid, net.password);

  unsigned long start = millis();
  while (millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    if (WiFi.status() == WL_CONNECTED) {
      currentSSID = net.ssid;
      Serial.print("Connected to "); Serial.println(net.ssid);
      Serial.print("IP: ");          Serial.println(WiFi.localIP());
      startMDNSIfNeeded();
      setupWebServer();
      return true;
    }
    delay(250);
  }

  currentSSID = "";
  Serial.print("Failed: "); Serial.println(net.ssid);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  Serial.println("No Wi-Fi, continuing offline.");
  return false;
}

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWiFiAttempt < WIFI_RETRY_INTERVAL_MS) return;
  lastWiFiAttempt = millis();
  connectPreferredWiFi();
}

void startMDNSIfNeeded() {
  if (WiFi.status() != WL_CONNECTED || mdnsStarted) return;
  if (MDNS.begin(deviceName.c_str())) {
    mdnsStarted = true;
    Serial.printf("mDNS started: http://%s.local\n", deviceName.c_str());
  } else {
    Serial.println("mDNS failed to start");
  }
}

// ── Web server + OTA (REST API for the TypeScript portal) ──────────────────────

// The HTML UI and portal logic now live in visualizer-ts (served at /wifi). The
// device only exposes the JSON/OTA REST endpoints below, with permissive CORS so
// the browser-hosted portal can call them cross-origin.

void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleOptions() {
  addCorsHeaders();
  server.send(200, "text/plain", "");
}

String wifiStatusText() {
  if (WiFi.status() == WL_CONNECTED)
    return "Connected to " + currentSSID + " (" + WiFi.localIP().toString() + ")";
  return "Not connected";
}

void handleStatus() {
  addCorsHeaders();

  String hostname = deviceName + ".local";
  String response = "{";
  response += "\"deviceName\":\"" + deviceName + "\",";
  response += "\"firmwareVersion\":\"" FW_VERSION "\",";
  response += "\"chipId\":\"" + chipIdHex + "\",";
  response += "\"macAddress\":\"" + getMacAddressString() + "\",";
  response += "\"wifiStatus\":\"" + wifiStatusText() + "\",";
  response += "\"hostname\":\"" + hostname + "\"";
  response += "}";

  server.send(200, "application/json", response);
}

void handleRename() {
  addCorsHeaders();
  if (!server.hasArg("name")) {
    server.send(400, "application/json", "{\"error\":\"Missing 'name' field\"}");
    return;
  }
  String cleaned = sanitizeDeviceName(server.arg("name"));
  saveDeviceName(cleaned);
  server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Saved device name. Rebooting...\"}");
  delay(1000);
  ESP.restart();
}

void handleIdentify() {
  addCorsHeaders();
  identifyRequested = true;
  server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Identify sequence requested.\"}");
}

void setupWebServer() {
  if (webServerStarted) return;

  server.on("/wifi/status", HTTP_GET, handleStatus);
  server.on("/wifi/status", HTTP_OPTIONS, handleOptions);
  server.on("/wifi/rename", HTTP_POST, handleRename);
  server.on("/wifi/rename", HTTP_OPTIONS, handleOptions);
  server.on("/wifi/identify", HTTP_POST, handleIdentify);
  server.on("/wifi/identify", HTTP_OPTIONS, handleOptions);

  server.on("/wifi/update", HTTP_POST, []() {
    addCorsHeaders();
    bool ok = !Update.hasError();
    server.send(200, "application/json", ok ? "{\"status\":\"ok\",\"message\":\"Update successful. Rebooting...\"}" : "{\"status\":\"error\",\"message\":\"Update failed.\"}");
    delay(1000);
    if (ok) ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update start: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) Serial.printf("Update success: %u bytes\n", upload.totalSize);
      else Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      Update.abort();
      Serial.println("Update aborted");
    }
  });
  server.on("/wifi/update", HTTP_OPTIONS, handleOptions);

  server.begin();
  webServerStarted = true;
  Serial.println("Web server started");
}
