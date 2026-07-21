#include "Settings.h"
#include <Arduino.h>
#include "AppConfig.h"
#include "LogBuffer.h"
#include "MathUtils.h"

namespace jelly {
namespace {

void normalizeSettings(DeviceSettings& value) {
  if (value.deviceId > 32) value.deviceId = 0;
  value.powerLimitMilliAmps = value.powerLimitMilliAmps < 500U
      ? 500U
      : (value.powerLimitMilliAmps > 60000U ? 60000U : value.powerLimitMilliAmps);

  value.pattern.brightness = clamp01(value.pattern.brightness);
  value.pattern.speed = clamp01(value.pattern.speed);
  value.pattern.scale = max(0.0f, min(2.0f, value.pattern.scale));
  value.pattern.density = clamp01(value.pattern.density);
  value.pattern.contrast = clamp01(value.pattern.contrast);
  value.pattern.sparkle = clamp01(value.pattern.sparkle);
  value.masterBrightness = clamp01(value.masterBrightness);

  if (fabsf(value.sensor.calibrationFactor) < 0.001f) {
    value.sensor.calibrationFactor = -14850.0f;
  }
  value.sensor.occupancyOnKg = max(0.0f, value.sensor.occupancyOnKg);
  value.sensor.occupancyOffKg = min(
      value.sensor.occupancyOnKg, max(0.0f, value.sensor.occupancyOffKg));
  value.sensor.smoothing = max(0.01f, clamp01(value.sensor.smoothing));
  value.sensor.noiseFloorKg = max(0.0f, value.sensor.noiseFloorKg);
  value.sensor.movementScaleKg = max(0.001f, value.sensor.movementScaleKg);
  value.sensor.stillnessThreshold = clamp01(value.sensor.stillnessThreshold);
  value.sensor.calmBuildSeconds = max(1.0f, value.sensor.calmBuildSeconds);
  value.sensor.calmRiseSeconds = max(0.1f, value.sensor.calmRiseSeconds);
  value.sensor.calmFallSeconds = max(0.1f, value.sensor.calmFallSeconds);
  value.sensor.sampleIntervalMs = value.sensor.sampleIntervalMs < 20U
      ? 20U
      : (value.sensor.sampleIntervalMs > 1000U ? 1000U : value.sensor.sampleIntervalMs);

  value.interaction.expectedPlatformCount = value.interaction.expectedPlatformCount < 1U
      ? 1U
      : min(static_cast<uint8_t>(config::kMaxPlatforms), value.interaction.expectedPlatformCount);
  value.interaction.influenceRadiusMeters = max(0.25f, value.interaction.influenceRadiusMeters);
  value.interaction.calmThreshold = clamp01(value.interaction.calmThreshold);
  value.interaction.calmReleaseThreshold = min(
      value.interaction.calmThreshold, clamp01(value.interaction.calmReleaseThreshold));
  value.interaction.maxAgitationForChorus = clamp01(value.interaction.maxAgitationForChorus);
  value.interaction.allCalmHoldSeconds = max(0.0f, value.interaction.allCalmHoldSeconds);
  value.interaction.chorusFadeSeconds = max(0.1f, value.interaction.chorusFadeSeconds);
  value.interaction.activationWaveSpeedMetersPerSecond = max(
      0.1f, value.interaction.activationWaveSpeedMetersPerSecond);
  value.interaction.activationWaveWidthMeters = max(
      0.05f, value.interaction.activationWaveWidthMeters);
  value.interaction.activationWaveDurationSeconds = max(
      0.5f, value.interaction.activationWaveDurationSeconds);
}

}  // namespace

String SettingsStore::defaultDeviceName() const {
  const uint64_t chip = ESP.getEfuseMac();
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "jelly-%06X", static_cast<uint32_t>(chip & 0xFFFFFFU));
  String value(buffer);
  value.toLowerCase();
  return value;
}

void SettingsStore::load() {
  preferences_.begin("device", true);
  data_.deviceId = preferences_.getUChar("id", 0);
  data_.name = preferences_.getString("name", defaultDeviceName());
  preferences_.end();

  preferences_.begin("hardware", true);
  data_.hasScale = preferences_.getBool("hasScale", false);
  data_.isJelly = preferences_.getBool("isJelly", false);
  data_.isBig = preferences_.getBool("isBig", false);
  data_.powerLimitMilliAmps = preferences_.getUInt("powerMa", 12000);
  preferences_.end();

  preferences_.begin("transform", true);
  data_.position.x = preferences_.getFloat("x", 0.0f);
  data_.position.y = preferences_.getFloat("y", 0.0f);
  data_.position.z = preferences_.getFloat("z", 0.0f);
  data_.rotationYDegrees = preferences_.getFloat("rotY", 0.0f);
  preferences_.end();

  preferences_.begin("pattern", true);
  PatternId pattern = PatternId::Demo;
  parsePattern(preferences_.getString("name", "demo"), pattern);
  data_.fallbackPattern = pattern;
  data_.pattern.brightness = preferences_.getFloat("brightness", 0.72f);
  data_.pattern.speed = preferences_.getFloat("speed", 0.45f);
  data_.pattern.scale = preferences_.getFloat("scale", 1.0f);
  data_.pattern.density = preferences_.getFloat("density", 0.45f);
  data_.pattern.hue = preferences_.getFloat("hue", 0.52f);
  data_.pattern.hue2 = preferences_.getFloat("hue2", 0.72f);
  data_.pattern.contrast = preferences_.getFloat("contrast", 0.65f);
  data_.pattern.sparkle = preferences_.getFloat("sparkle", 0.10f);
  preferences_.end();

  preferences_.begin("show", true);
  ShowMode mode = ShowMode::Auto;
  parseShowMode(preferences_.getString("mode", "auto"), mode);
  data_.showMode = mode;
  data_.localShowOverride = preferences_.getBool("override", false);
  ShowMode overrideMode = ShowMode::Fallback;
  parseShowMode(preferences_.getString("overrideMode", "fallback"), overrideMode);
  data_.localOverrideMode = overrideMode;
  data_.masterBrightness = preferences_.getFloat("master", 1.0f);
  preferences_.end();

  preferences_.begin("sensor", true);
  data_.sensor.calibrationFactor = preferences_.getFloat("cal", -14850.0f);
  data_.sensor.occupancyOnKg = preferences_.getFloat("onKg", 18.0f);
  data_.sensor.occupancyOffKg = preferences_.getFloat("offKg", 10.0f);
  data_.sensor.smoothing = preferences_.getFloat("smooth", 0.20f);
  data_.sensor.noiseFloorKg = preferences_.getFloat("noise", 0.015f);
  data_.sensor.movementScaleKg = preferences_.getFloat("move", 0.20f);
  data_.sensor.stillnessThreshold = preferences_.getFloat("still", 0.12f);
  data_.sensor.calmBuildSeconds = preferences_.getFloat("build", 35.0f);
  data_.sensor.calmRiseSeconds = preferences_.getFloat("rise", 8.0f);
  data_.sensor.calmFallSeconds = preferences_.getFloat("fall", 1.5f);
  data_.sensor.sampleIntervalMs = preferences_.getUInt("sampleMs", 80);
  preferences_.end();

  preferences_.begin("interaction", true);
  data_.interaction.expectedPlatformCount = preferences_.getUChar("expected", 4);
  data_.interaction.influenceRadiusMeters = preferences_.getFloat("radius", 5.0f);
  data_.interaction.calmThreshold = preferences_.getFloat("calm", 0.86f);
  data_.interaction.calmReleaseThreshold = preferences_.getFloat("release", 0.68f);
  data_.interaction.maxAgitationForChorus = preferences_.getFloat("maxAg", 0.16f);
  data_.interaction.allCalmHoldSeconds = preferences_.getFloat("hold", 12.0f);
  data_.interaction.chorusFadeSeconds = preferences_.getFloat("fade", 8.0f);
  data_.interaction.activationWaveSpeedMetersPerSecond = preferences_.getFloat("waveSpeed", 5.5f);
  data_.interaction.activationWaveWidthMeters = preferences_.getFloat("waveWidth", 0.65f);
  data_.interaction.activationWaveDurationSeconds = preferences_.getFloat("waveDur", 5.0f);
  preferences_.end();

  preferences_.begin("schema", true);
  const uint8_t schemaVersion = preferences_.getUChar("version", 0);
  preferences_.end();

  bool migratedLegacyConfig = false;
  if (schemaVersion < 2) {
    // The uploaded firmware stored all role, pattern, and transform values in
    // one `config` namespace. Preserve them on the first refactored boot.
    preferences_.begin("config", true);
    const bool hasLegacyConfig =
        preferences_.isKey("hasScale") ||
        preferences_.isKey("isJelly") ||
        preferences_.isKey("isBig") ||
        preferences_.isKey("pattern") ||
        preferences_.isKey("posX") ||
        preferences_.isKey("posY") ||
        preferences_.isKey("posZ") ||
        preferences_.isKey("rotationY");
    if (hasLegacyConfig) {
      data_.hasScale = preferences_.getBool("hasScale", data_.hasScale);
      data_.isJelly = preferences_.getBool("isJelly", data_.isJelly);
      data_.isBig = preferences_.getBool("isBig", data_.isBig);
      PatternId legacyPattern = data_.fallbackPattern;
      if (parsePattern(preferences_.getString("pattern", patternName(legacyPattern)), legacyPattern)) {
        data_.fallbackPattern = legacyPattern;
      }
      data_.position.x = preferences_.getFloat("posX", data_.position.x);
      data_.position.y = preferences_.getFloat("posY", data_.position.y);
      data_.position.z = preferences_.getFloat("posZ", data_.position.z);
      data_.rotationYDegrees = preferences_.getFloat("rotationY", data_.rotationYDegrees);
      migratedLegacyConfig = true;
    }
    preferences_.end();
  }

  normalizeSettings(data_);

  if (schemaVersion < 2) {
    if (migratedLegacyConfig) {
      saveHardware();
      saveTransform();
      savePattern();
      Log.println("Migrated legacy `config` preferences to schema 2.");
    }
    preferences_.begin("schema", false);
    preferences_.putUChar("version", 2);
    preferences_.end();
  }

  Log.printf("Loaded device %s (id=%u, jelly=%d, scale=%d, big=%d)\n",
             data_.name.c_str(), data_.deviceId, data_.isJelly, data_.hasScale, data_.isBig);
}

void SettingsStore::saveIdentity() {
  preferences_.begin("device", false);
  preferences_.putUChar("id", data_.deviceId);
  preferences_.putString("name", data_.name);
  preferences_.end();
}

void SettingsStore::saveHardware() {
  preferences_.begin("hardware", false);
  preferences_.putBool("hasScale", data_.hasScale);
  preferences_.putBool("isJelly", data_.isJelly);
  preferences_.putBool("isBig", data_.isBig);
  preferences_.putUInt("powerMa", data_.powerLimitMilliAmps);
  preferences_.end();
}

void SettingsStore::saveTransform() {
  preferences_.begin("transform", false);
  preferences_.putFloat("x", data_.position.x);
  preferences_.putFloat("y", data_.position.y);
  preferences_.putFloat("z", data_.position.z);
  preferences_.putFloat("rotY", data_.rotationYDegrees);
  preferences_.end();
}

void SettingsStore::savePattern() {
  preferences_.begin("pattern", false);
  preferences_.putString("name", patternName(data_.fallbackPattern));
  preferences_.putFloat("brightness", data_.pattern.brightness);
  preferences_.putFloat("speed", data_.pattern.speed);
  preferences_.putFloat("scale", data_.pattern.scale);
  preferences_.putFloat("density", data_.pattern.density);
  preferences_.putFloat("hue", data_.pattern.hue);
  preferences_.putFloat("hue2", data_.pattern.hue2);
  preferences_.putFloat("contrast", data_.pattern.contrast);
  preferences_.putFloat("sparkle", data_.pattern.sparkle);
  preferences_.end();
}

void SettingsStore::saveShow() {
  preferences_.begin("show", false);
  preferences_.putString("mode", showModeName(data_.showMode));
  preferences_.putBool("override", data_.localShowOverride);
  preferences_.putString("overrideMode", showModeName(data_.localOverrideMode));
  preferences_.putFloat("master", data_.masterBrightness);
  preferences_.end();
}

void SettingsStore::saveSensor() {
  preferences_.begin("sensor", false);
  preferences_.putFloat("cal", data_.sensor.calibrationFactor);
  preferences_.putFloat("onKg", data_.sensor.occupancyOnKg);
  preferences_.putFloat("offKg", data_.sensor.occupancyOffKg);
  preferences_.putFloat("smooth", data_.sensor.smoothing);
  preferences_.putFloat("noise", data_.sensor.noiseFloorKg);
  preferences_.putFloat("move", data_.sensor.movementScaleKg);
  preferences_.putFloat("still", data_.sensor.stillnessThreshold);
  preferences_.putFloat("build", data_.sensor.calmBuildSeconds);
  preferences_.putFloat("rise", data_.sensor.calmRiseSeconds);
  preferences_.putFloat("fall", data_.sensor.calmFallSeconds);
  preferences_.putUInt("sampleMs", data_.sensor.sampleIntervalMs);
  preferences_.end();
}

void SettingsStore::saveInteraction() {
  preferences_.begin("interaction", false);
  preferences_.putUChar("expected", data_.interaction.expectedPlatformCount);
  preferences_.putFloat("radius", data_.interaction.influenceRadiusMeters);
  preferences_.putFloat("calm", data_.interaction.calmThreshold);
  preferences_.putFloat("release", data_.interaction.calmReleaseThreshold);
  preferences_.putFloat("maxAg", data_.interaction.maxAgitationForChorus);
  preferences_.putFloat("hold", data_.interaction.allCalmHoldSeconds);
  preferences_.putFloat("fade", data_.interaction.chorusFadeSeconds);
  preferences_.putFloat("waveSpeed", data_.interaction.activationWaveSpeedMetersPerSecond);
  preferences_.putFloat("waveWidth", data_.interaction.activationWaveWidthMeters);
  preferences_.putFloat("waveDur", data_.interaction.activationWaveDurationSeconds);
  preferences_.end();
}

}  // namespace jelly
