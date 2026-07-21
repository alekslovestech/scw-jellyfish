#include "WebApi.h"

#include <ArduinoJson.h>
#include <Update.h>
#include "LogBuffer.h"
#include "MathUtils.h"
#include "version.h"

namespace jelly {

void WebApi::begin() {
  // Register handlers immediately, but do not open a TCP listener until the
  // Wi-Fi station and lwIP stack are fully connected. Starting WebServer while
  // NetworkManager is resetting/reconfiguring the station can trigger the
  // ESP-IDF "Invalid mbox" assertion in tcpip_send_msg_wait_sem().
  if (!routesConfigured_) {
    configureRoutes();
    routesConfigured_ = true;
  }
  Log.println("HTTP API routes initialized; waiting for Wi-Fi.");
}

void WebApi::tick(uint64_t localNowMs) {
  if (network_.connected()) {
    if (!serverStarted_) {
      server_.begin();
      serverStarted_ = true;
      Log.printf("HTTP API listening on %s:%u\n",
                 network_.ipAddress().c_str(),
                 static_cast<unsigned>(config::kHttpPort));
    }
    server_.handleClient();
  } else if (serverStarted_) {
    // Stop the old listener before NetworkManager tears down and rebuilds lwIP.
    server_.stop();
    serverStarted_ = false;
    Log.println("HTTP API stopped while Wi-Fi reconnects.");
  }
  if (restartPending_ && localNowMs >= restartAtLocalMs_) {
    delay(30);
    ESP.restart();
  }
}

void WebApi::configureRoutes() {
  server_.on("/", HTTP_GET, [this]() {
    server_.send(200, "text/plain", "Jellyfish Forest controller " JELLY_FW_VERSION);
  });
  server_.on("/status", HTTP_GET, [this]() { sendStatus(); });
  server_.on("/number", HTTP_GET, [this]() { sendStatus(); });
  server_.on("/log", HTTP_GET, [this]() { sendLog(); });
  server_.on("/identify", HTTP_POST, [this]() {
    renderer_.identify(monotonicMillis());
    sendJson(true);
  });
  server_.on("/rename", HTTP_POST, [this]() { handleRename(); });
  server_.on("/device-id", HTTP_POST, [this]() { handleDeviceId(); });
  server_.on("/config", HTTP_POST, [this]() { handleHardwareConfig(); });
  server_.on("/position", HTTP_POST, [this]() { handlePosition(); });
  server_.on("/pattern", HTTP_POST, [this]() { handlePattern(); });
  server_.on("/pattern-parameters", HTTP_POST, [this]() { handlePatternParameters(); });
  server_.on("/show", HTTP_POST, [this]() { handleShow(); });
  server_.on("/sensor", HTTP_POST, [this]() { handleSensorTuning(); });
  server_.on("/interaction", HTTP_POST, [this]() { handleInteractionTuning(); });
  server_.on("/tare", HTTP_POST, [this]() { handleTare(); });
  server_.on("/setNumber", HTTP_POST, [this]() { handleSetNumber(); });
  server_.on("/test-wave", HTTP_POST, [this]() { handleTestWave(); });
  server_.on(
      "/update",
      HTTP_POST,
      [this]() { handleUpdateFinished(); },
      [this]() { handleUpdateUpload(); });
  server_.onNotFound([this]() { sendJson(false, 404, "Not found"); });
}

void WebApi::sendStatus() {
  DynamicJsonDocument document(6144);
  const uint64_t localNowMs = monotonicMillis();
  const uint64_t showNowMs = clock_.now(localNowMs);
  const PlatformState& platform = sensor_.state();
  const InstallationState& installation = interaction_.installation(localNowMs);
  const FieldState field = interaction_.sampleField(settings_.position, settings_.interaction);

  document["name"] = settings_.name;
  document["id"] = settings_.deviceId;
  document["mac"] = network_.macAddress();
  document["chip"] = network_.chipId();
  document["firmware"] = JELLY_FW_VERSION;
  document["protocol"] = JELLY_PROTOCOL_VERSION;
  document["ssid"] = network_.ssid();
  document["rssi"] = network_.rssi();
  document["ip"] = network_.ipAddress();
  document["hasScale"] = settings_.hasScale;
  document["isJelly"] = settings_.isJelly;
  document["isBig"] = settings_.isBig;
  document["pattern"] = patternName(settings_.fallbackPattern);
  document["showMode"] = showModeName(settings_.showMode);
  document["localShowOverride"] = settings_.localShowOverride;
  document["localOverrideMode"] = showModeName(settings_.localOverrideMode);
  document["scene"] = renderer_.sceneLabel();
  document["posX"] = settings_.position.x;
  document["posY"] = settings_.position.y;
  document["posZ"] = settings_.position.z;
  document["rotationY"] = settings_.rotationYDegrees;
  document["masterBrightness"] = settings_.masterBrightness;
  document["powerLimitMilliAmps"] = settings_.powerLimitMilliAmps;
  document["number"] = debugNumber_;
  document["hasNumber"] = hasDebugNumber_;
  document["showTimeMs"] = showNowMs;
  document["clockSynchronized"] = clock_.synchronized(localNowMs);
  document["clockOffsetMs"] = clock_.offsetMs();
  document["frames"] = renderer_.renderedFrames();

  JsonObject pattern = document.createNestedObject("patternParameters");
  pattern["brightness"] = settings_.pattern.brightness;
  pattern["speed"] = settings_.pattern.speed;
  pattern["scale"] = settings_.pattern.scale;
  pattern["density"] = settings_.pattern.density;
  pattern["hue"] = settings_.pattern.hue;
  pattern["hue2"] = settings_.pattern.hue2;
  pattern["contrast"] = settings_.pattern.contrast;
  pattern["sparkle"] = settings_.pattern.sparkle;

  JsonObject sensor = document.createNestedObject("sensor");
  sensor["ready"] = sensor_.ready();
  sensor["status"] = sensor_.status();
  sensor["occupied"] = platform.occupied;
  sensor["weightKg"] = platform.weightKg;
  sensor["agitation"] = platform.agitation;
  sensor["calmness"] = platform.calmness;

  JsonObject sensorTuning = document.createNestedObject("sensorTuning");
  sensorTuning["calibrationFactor"] = settings_.sensor.calibrationFactor;
  sensorTuning["occupancyOnKg"] = settings_.sensor.occupancyOnKg;
  sensorTuning["occupancyOffKg"] = settings_.sensor.occupancyOffKg;
  sensorTuning["smoothing"] = settings_.sensor.smoothing;
  sensorTuning["noiseFloorKg"] = settings_.sensor.noiseFloorKg;
  sensorTuning["movementScaleKg"] = settings_.sensor.movementScaleKg;
  sensorTuning["stillnessThreshold"] = settings_.sensor.stillnessThreshold;
  sensorTuning["calmBuildSeconds"] = settings_.sensor.calmBuildSeconds;
  sensorTuning["calmRiseSeconds"] = settings_.sensor.calmRiseSeconds;
  sensorTuning["calmFallSeconds"] = settings_.sensor.calmFallSeconds;
  sensorTuning["sampleIntervalMs"] = settings_.sensor.sampleIntervalMs;

  JsonObject interactionTuning = document.createNestedObject("interactionTuning");
  interactionTuning["expectedPlatformCount"] = settings_.interaction.expectedPlatformCount;
  interactionTuning["influenceRadiusMeters"] = settings_.interaction.influenceRadiusMeters;
  interactionTuning["calmThreshold"] = settings_.interaction.calmThreshold;
  interactionTuning["calmReleaseThreshold"] = settings_.interaction.calmReleaseThreshold;
  interactionTuning["maxAgitationForChorus"] = settings_.interaction.maxAgitationForChorus;
  interactionTuning["allCalmHoldSeconds"] = settings_.interaction.allCalmHoldSeconds;
  interactionTuning["chorusFadeSeconds"] = settings_.interaction.chorusFadeSeconds;
  interactionTuning["activationWaveSpeedMetersPerSecond"] =
      settings_.interaction.activationWaveSpeedMetersPerSecond;
  interactionTuning["activationWaveWidthMeters"] =
      settings_.interaction.activationWaveWidthMeters;
  interactionTuning["activationWaveDurationSeconds"] =
      settings_.interaction.activationWaveDurationSeconds;

  JsonObject localField = document.createNestedObject("field");
  localField["presence"] = field.presence;
  localField["agitation"] = field.agitation;
  localField["calmness"] = field.calmness;
  localField["harmony"] = field.harmony;
  localField["synchronization"] = field.synchronization;
  localField["brightness"] = field.brightness;

  JsonObject global = document.createNestedObject("installation");
  global["seenPlatforms"] = installation.seenPlatforms;
  global["occupiedPlatforms"] = installation.occupiedPlatforms;
  global["meanCalmness"] = installation.meanCalmness;
  global["minimumCalmness"] = installation.minimumCalmness;
  global["maximumAgitation"] = installation.maximumAgitation;
  global["chorus"] = installation.chorus;
  global["allCalm"] = installation.allCalmLatched;
  global["canonical"] = interaction_.canonicalActive(localNowMs);

  String body;
  serializeJson(document, body);
  server_.send(200, "application/json", body);
}

void WebApi::sendLog() {
  // Plain text avoids temporarily duplicating and JSON-escaping the full ring
  // buffer in heap memory. Both the legacy and refactored dashboards accept it.
  server_.send(200, "text/plain; charset=utf-8", Log.snapshot());
}

void WebApi::sendJson(bool ok, int statusCode, const String& message) {
  StaticJsonDocument<512> document;
  document["ok"] = ok;
  if (message.length() > 0) document[ok ? "message" : "error"] = message;
  document["id"] = settings_.deviceId;
  document["name"] = settings_.name;
  document["ip"] = network_.ipAddress();
  document["pattern"] = patternName(settings_.fallbackPattern);
  document["showMode"] = showModeName(settings_.showMode);
  document["localShowOverride"] = settings_.localShowOverride;
  document["localOverrideMode"] = showModeName(settings_.localOverrideMode);
  String body;
  serializeJson(document, body);
  server_.send(statusCode, "application/json", body);
}

String WebApi::sanitizeName(const String& input) const {
  String value = input;
  value.trim();
  value.toLowerCase();
  String output;
  output.reserve(value.length());
  for (size_t index = 0; index < value.length(); ++index) {
    const char c = value[index];
    const bool allowed =
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_';
    if (allowed) output += c;
    else if (c == ' ') output += '-';
  }
  while (output.indexOf("--") >= 0) output.replace("--", "-");
  return output;
}

bool WebApi::argBool(const char* name, bool fallback) {
  if (!server_.hasArg(name)) return fallback;
  const String value = server_.arg(name);
  return value == "1" || value.equalsIgnoreCase("true") || value.equalsIgnoreCase("on");
}

float WebApi::argFloat(const char* name, float fallback) {
  return server_.hasArg(name) ? server_.arg(name).toFloat() : fallback;
}

uint32_t WebApi::argUInt(const char* name, uint32_t fallback) {
  return server_.hasArg(name) ? static_cast<uint32_t>(server_.arg(name).toInt()) : fallback;
}

void WebApi::handleRename() {
  const String name = sanitizeName(server_.arg("name"));
  if (name.length() == 0) {
    sendJson(false, 400, "Name must contain letters, numbers, '-' or '_'");
    return;
  }
  settings_.name = name;
  settingsStore_.saveIdentity();
  network_.refreshMdns(settings_);
  sendJson(true);
}

void WebApi::handleDeviceId() {
  const int value = server_.arg("id").toInt();
  if (value < 1 || value > 32) {
    sendJson(false, 400, "ID must be between 1 and 32");
    return;
  }
  settings_.deviceId = static_cast<uint8_t>(value);
  settingsStore_.saveIdentity();
  sendJson(true, 200, "Saved; rebooting");
  restartPending_ = true;
  restartAtLocalMs_ = monotonicMillis() + 600ULL;
}

void WebApi::handleHardwareConfig() {
  const bool oldBig = settings_.isBig;
  const bool oldScale = settings_.hasScale;
  settings_.hasScale = argBool("hasScale", false);
  settings_.isJelly = argBool("isJelly", false);
  settings_.isBig = argBool("isBig", false);
  settings_.powerLimitMilliAmps = constrain(
      argUInt("powerLimitMilliAmps", settings_.powerLimitMilliAmps),
      500U,
      60000U);
  settingsStore_.saveHardware();
  if (oldBig != settings_.isBig) geometry_.recalculate();
  sendJson(true, 200, oldScale != settings_.hasScale ? "Saved; rebooting to apply scale role" : "Saved");
  if (oldScale != settings_.hasScale) {
    restartPending_ = true;
    restartAtLocalMs_ = monotonicMillis() + 700ULL;
  }
}

void WebApi::handlePosition() {
  settings_.position.x = argFloat("posX", settings_.position.x);
  settings_.position.y = argFloat("posY", settings_.position.y);
  settings_.position.z = argFloat("posZ", settings_.position.z);
  settings_.rotationYDegrees = argFloat("rotationY", settings_.rotationYDegrees);
  settingsStore_.saveTransform();
  sendJson(true);
}

void WebApi::handlePattern() {
  PatternId pattern;
  if (!parsePattern(server_.arg("pattern"), pattern)) {
    sendJson(false, 400, "Unknown pattern");
    return;
  }
  settings_.fallbackPattern = pattern;
  settingsStore_.savePattern();
  sendJson(true);
}

void WebApi::handlePatternParameters() {
  settings_.pattern.brightness = clamp01(argFloat("brightness", settings_.pattern.brightness));
  settings_.pattern.speed = clamp01(argFloat("speed", settings_.pattern.speed));
  settings_.pattern.scale = constrain(argFloat("scale", settings_.pattern.scale), 0.0f, 2.0f);
  settings_.pattern.density = clamp01(argFloat("density", settings_.pattern.density));
  settings_.pattern.hue = argFloat("hue", settings_.pattern.hue);
  settings_.pattern.hue2 = argFloat("hue2", settings_.pattern.hue2);
  settings_.pattern.contrast = clamp01(argFloat("contrast", settings_.pattern.contrast));
  settings_.pattern.sparkle = clamp01(argFloat("sparkle", settings_.pattern.sparkle));
  settingsStore_.savePattern();
  sendJson(true);
}

void WebApi::handleShow() {
  ShowMode mode;
  if (server_.hasArg("mode") && !parseShowMode(server_.arg("mode"), mode)) {
    sendJson(false, 400, "Unknown show mode");
    return;
  }

  // Supplying localOverride means this is a per-controller commissioning
  // command. Omitting it updates the global/fallback mode used by the
  // dashboard persistence path and UDP conductor.
  if (server_.hasArg("localOverride")) {
    settings_.localShowOverride = argBool(
        "localOverride", settings_.localShowOverride);
    if (server_.hasArg("mode")) settings_.localOverrideMode = mode;
  } else if (server_.hasArg("mode")) {
    settings_.showMode = mode;
  }

  settings_.masterBrightness = clamp01(argFloat("masterBrightness", settings_.masterBrightness));
  settingsStore_.saveShow();
  sendJson(true);
}

void WebApi::handleSensorTuning() {
  const float calibration = argFloat(
      "calibrationFactor", settings_.sensor.calibrationFactor);
  if (fabsf(calibration) < 0.001f) {
    sendJson(false, 400, "Calibration factor must be non-zero");
    return;
  }
  settings_.sensor.calibrationFactor = calibration;
  settings_.sensor.occupancyOnKg = max(0.0f, argFloat("occupancyOnKg", settings_.sensor.occupancyOnKg));
  settings_.sensor.occupancyOffKg = min(
      settings_.sensor.occupancyOnKg,
      max(0.0f, argFloat("occupancyOffKg", settings_.sensor.occupancyOffKg)));
  settings_.sensor.smoothing = max(
      0.01f, clamp01(argFloat("smoothing", settings_.sensor.smoothing)));
  settings_.sensor.noiseFloorKg = max(0.0f, argFloat("noiseFloorKg", settings_.sensor.noiseFloorKg));
  settings_.sensor.movementScaleKg = max(0.001f, argFloat("movementScaleKg", settings_.sensor.movementScaleKg));
  settings_.sensor.stillnessThreshold = clamp01(argFloat("stillnessThreshold", settings_.sensor.stillnessThreshold));
  settings_.sensor.calmBuildSeconds = max(1.0f, argFloat("calmBuildSeconds", settings_.sensor.calmBuildSeconds));
  settings_.sensor.calmRiseSeconds = max(0.1f, argFloat("calmRiseSeconds", settings_.sensor.calmRiseSeconds));
  settings_.sensor.calmFallSeconds = max(0.1f, argFloat("calmFallSeconds", settings_.sensor.calmFallSeconds));
  settings_.sensor.sampleIntervalMs = constrain(argUInt("sampleIntervalMs", settings_.sensor.sampleIntervalMs), 20U, 1000U);
  sensor_.applyTuning(settings_.sensor);
  settingsStore_.saveSensor();
  sendJson(true, 200, "Sensor tuning saved and applied");
}

void WebApi::handleInteractionTuning() {
  settings_.interaction.expectedPlatformCount = static_cast<uint8_t>(constrain(
      argUInt("expectedPlatformCount", settings_.interaction.expectedPlatformCount),
      1U,
      static_cast<uint32_t>(config::kMaxPlatforms)));
  settings_.interaction.influenceRadiusMeters = max(
      0.25f,
      argFloat("influenceRadiusMeters", settings_.interaction.influenceRadiusMeters));
  settings_.interaction.calmThreshold = clamp01(argFloat("calmThreshold", settings_.interaction.calmThreshold));
  settings_.interaction.calmReleaseThreshold = min(
      settings_.interaction.calmThreshold,
      clamp01(argFloat("calmReleaseThreshold", settings_.interaction.calmReleaseThreshold)));
  settings_.interaction.maxAgitationForChorus = clamp01(argFloat("maxAgitationForChorus", settings_.interaction.maxAgitationForChorus));
  settings_.interaction.allCalmHoldSeconds = max(0.0f, argFloat("allCalmHoldSeconds", settings_.interaction.allCalmHoldSeconds));
  settings_.interaction.chorusFadeSeconds = max(0.1f, argFloat("chorusFadeSeconds", settings_.interaction.chorusFadeSeconds));
  settings_.interaction.activationWaveSpeedMetersPerSecond = max(
      0.1f,
      argFloat("activationWaveSpeedMetersPerSecond", settings_.interaction.activationWaveSpeedMetersPerSecond));
  settings_.interaction.activationWaveWidthMeters = max(
      0.05f,
      argFloat("activationWaveWidthMeters", settings_.interaction.activationWaveWidthMeters));
  settings_.interaction.activationWaveDurationSeconds = max(
      0.5f,
      argFloat("activationWaveDurationSeconds", settings_.interaction.activationWaveDurationSeconds));
  settingsStore_.saveInteraction();
  sendJson(true);
}

void WebApi::handleTare() {
  const bool ok = sensor_.tare();
  sendJson(ok, ok ? 200 : 409, ok ? "Scale tared" : "Scale unavailable");
}

void WebApi::handleSetNumber() {
  debugNumber_ = argFloat("value", debugNumber_);
  hasDebugNumber_ = true;
  sendJson(true);
}

void WebApi::handleTestWave() {
  ActivationEvent event;
  event.eventId = esp_random() | 1U;
  event.platformId = settings_.deviceId;
  event.origin = {
      argFloat("x", settings_.position.x),
      argFloat("y", settings_.position.y),
      argFloat("z", settings_.position.z),
  };
  event.startShowTimeMs = clock_.now(monotonicMillis());
  renderer_.startActivation(event, event.startShowTimeMs);
  network_.broadcastActivation(event);
  sendJson(true);
}

void WebApi::handleUpdateUpload() {
  HTTPUpload& upload = server_.upload();
  if (upload.status == UPLOAD_FILE_START) {
    updateSucceeded_ = Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (upload.status == UPLOAD_FILE_WRITE && updateSucceeded_) {
    updateSucceeded_ = Update.write(upload.buf, upload.currentSize) == upload.currentSize;
  } else if (upload.status == UPLOAD_FILE_END && updateSucceeded_) {
    updateSucceeded_ = Update.end(true);
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    updateSucceeded_ = false;
  }
}

void WebApi::handleUpdateFinished() {
  if (updateSucceeded_ && !Update.hasError()) {
    sendJson(true, 200, "Firmware accepted; rebooting");
    restartPending_ = true;
    restartAtLocalMs_ = monotonicMillis() + 700ULL;
  } else {
    sendJson(false, 500, "Firmware update failed");
  }
}

}  // namespace jelly
