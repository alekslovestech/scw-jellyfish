#pragma once

#include <WebServer.h>
#include "Geometry.h"
#include "InteractionEngine.h"
#include "NetworkManager.h"
#include "PatternEngine.h"
#include "SensorModel.h"
#include "Settings.h"
#include "ShowClock.h"

namespace jelly {

class WebApi {
 public:
  WebApi(
      SettingsStore& settings,
      Geometry& geometry,
      SensorModel& sensor,
      InteractionEngine& interaction,
      PatternEngine& renderer,
      NetworkManager& network,
      ShowClock& clock)
      : settingsStore_(settings),
        settings_(settings.data()),
        geometry_(geometry),
        sensor_(sensor),
        interaction_(interaction),
        renderer_(renderer),
        network_(network),
        clock_(clock),
        server_(config::kHttpPort) {}

  void begin();
  void tick(uint64_t localNowMs);

 private:
  void configureRoutes();
  void sendStatus();
  void sendLog();
  void sendJson(bool ok, int statusCode = 200, const String& message = String());
  void handleRename();
  void handleDeviceId();
  void handleHardwareConfig();
  void handlePosition();
  void handlePattern();
  void handlePatternParameters();
  void handleShow();
  void handleSensorTuning();
  void handleInteractionTuning();
  void handleTare();
  void handleSetNumber();
  void handleTestWave();
  void handleUpdateFinished();
  void handleUpdateUpload();
  String sanitizeName(const String& value) const;
  bool argBool(const char* name, bool fallback = false);
  float argFloat(const char* name, float fallback);
  uint32_t argUInt(const char* name, uint32_t fallback);

  SettingsStore& settingsStore_;
  DeviceSettings& settings_;
  Geometry& geometry_;
  SensorModel& sensor_;
  InteractionEngine& interaction_;
  PatternEngine& renderer_;
  NetworkManager& network_;
  ShowClock& clock_;
  WebServer server_;
  bool restartPending_ = false;
  uint64_t restartAtLocalMs_ = 0;
  bool updateSucceeded_ = false;
  bool routesConfigured_ = false;
  bool serverStarted_ = false;
  float debugNumber_ = 0.0f;
  bool hasDebugNumber_ = false;
};

}  // namespace jelly
