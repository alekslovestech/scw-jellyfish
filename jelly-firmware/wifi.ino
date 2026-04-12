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

const int WIFI_COUNT = sizeof(wifiList) / sizeof(wifiList[0]);
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 8000;
const unsigned long WIFI_RETRY_INTERVAL_MS  = 30000;

String currentSSID = "";

// ── Connection ────────────────────────────────────────────────────────────────

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
      Serial.print("Connected to "); Serial.println(ssid);
      Serial.print("IP: ");          Serial.println(WiFi.localIP());
      return true;
    }
    delay(250);
  }

  Serial.print("Failed: "); Serial.println(ssid);
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
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWiFiAttempt < WIFI_RETRY_INTERVAL_MS) return;
  lastWiFiAttempt = millis();
  connectToAnyWiFi();
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

// ── Web server + OTA ──────────────────────────────────────────────────────────

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

void handleIdentify() {
  identifyRequested = true;
  server.send(200, "text/html", buildHtmlPage("Identify sequence requested."));
}

void setupWebServer() {
  if (webServerStarted) return;

  server.on("/",        HTTP_GET,  handleRoot);
  server.on("/rename",  HTTP_POST, handleRename);
  server.on("/identify",HTTP_POST, handleIdentify);

  server.on("/update", HTTP_POST, []() {
    bool ok = !Update.hasError();
    server.send(200, "text/html", buildHtmlPage(ok ? "Update successful. Rebooting..." : "Update failed."));
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

  server.begin();
  webServerStarted = true;
  Serial.println("Web server started");
}
