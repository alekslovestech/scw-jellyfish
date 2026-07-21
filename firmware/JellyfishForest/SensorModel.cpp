#include "SensorModel.h"
#include <math.h>
#include "AppConfig.h"
#include "LogBuffer.h"
#include "MathUtils.h"
#include "ShowClock.h"

namespace jelly {

bool SensorModel::begin(const DeviceSettings& settings) {
  state_ = PlatformState{};
  state_.id = settings.deviceId;
  state_.position = settings.position;

  if (!settings.hasScale) {
    status_ = "disabled";
    ready_ = false;
    return false;
  }

  scale_.begin(config::kHx711DoutPin, config::kHx711SckPin);
  const uint64_t started = monotonicMillis();
  while (!scale_.is_ready() && monotonicMillis() - started < config::kHx711ReadyTimeoutMs) {
    delay(5);
  }

  if (!scale_.is_ready()) {
    status_ = "HX711 not ready";
    ready_ = false;
    Log.println("HX711 did not become ready; continuing without scale input.");
    return false;
  }

  scale_.set_scale(settings.sensor.calibrationFactor);
  scale_.tare(5);
  smoothedWeightKg_ = 0.0f;
  agitation_ = 0.0f;
  calmness_ = 0.0f;
  stableSeconds_ = 0.0f;
  ready_ = true;
  status_ = "ready";
  Log.println("HX711 initialized and tared.");
  return true;
}

void SensorModel::tick(uint64_t localNowMs, const DeviceSettings& settings) {
  state_.id = settings.deviceId;
  state_.position = settings.position;
  state_.lastSeenLocalMs = localNowMs;
  state_.valid = settings.hasScale && ready_;
  if (!state_.valid) return;

  const uint32_t sampleInterval = settings.sensor.sampleIntervalMs < 20U
      ? 20U
      : settings.sensor.sampleIntervalMs;
  if (lastSampleLocalMs_ != 0 && localNowMs - lastSampleLocalMs_ < sampleInterval) return;

  const float dtSeconds = lastSampleLocalMs_ == 0
      ? sampleInterval / 1000.0f
      : min(0.5f, (localNowMs - lastSampleLocalMs_) / 1000.0f);
  lastSampleLocalMs_ = localNowMs;

  if (!scale_.is_ready()) {
    status_ = "HX711 temporarily unavailable";
    return;
  }

  status_ = "ready";
  const float rawWeightKg = fabsf(scale_.get_units(1));
  const float previousWeightKg = smoothedWeightKg_;
  const float smoothing = clamp01(settings.sensor.smoothing);
  smoothedWeightKg_ = previousWeightKg + (rawWeightKg - previousWeightKg) * smoothing;

  const float deltaKg = fabsf(smoothedWeightKg_ - previousWeightKg);
  const float movementAboveNoise = max(0.0f, deltaKg - settings.sensor.noiseFloorKg);
  const float instantAgitation = clamp01(
      movementAboveNoise / max(0.001f, settings.sensor.movementScaleKg));
  const float agitationTime = instantAgitation > agitation_ ? 0.20f : 1.25f;
  agitation_ += (instantAgitation - agitation_) * exponentialAlpha(dtSeconds, agitationTime);

  const bool wasOccupied = state_.occupied;
  if (state_.occupied) {
    state_.occupied = smoothedWeightKg_ >= settings.sensor.occupancyOffKg;
  } else {
    state_.occupied = smoothedWeightKg_ >= settings.sensor.occupancyOnKg;
  }

  if (!wasOccupied && state_.occupied) {
    activationPending_ = true;
    stableSeconds_ = 0.0f;
    calmness_ = 0.0f;
  }

  if (!state_.occupied) {
    stableSeconds_ = 0.0f;
    calmness_ += (0.0f - calmness_) * exponentialAlpha(dtSeconds, 0.8f);
  } else {
    if (agitation_ <= settings.sensor.stillnessThreshold) {
      stableSeconds_ += dtSeconds;
    } else {
      stableSeconds_ = max(0.0f, stableSeconds_ - dtSeconds * (1.5f + agitation_ * 4.0f));
    }

    const float calmTarget = smoothstep(
        0.0f,
        max(1.0f, settings.sensor.calmBuildSeconds),
        stableSeconds_);
    const float timeConstant = calmTarget >= calmness_
        ? settings.sensor.calmRiseSeconds
        : settings.sensor.calmFallSeconds;
    calmness_ += (calmTarget - calmness_) * exponentialAlpha(dtSeconds, timeConstant);
  }

  state_.weightKg = smoothedWeightKg_;
  state_.agitation = clamp01(agitation_);
  state_.calmness = state_.occupied ? clamp01(calmness_) : 0.0f;
  state_.sequence = ++sequence_;
}

void SensorModel::applyTuning(const SensorTuning& tuning) {
  if (ready_) scale_.set_scale(tuning.calibrationFactor);
}

bool SensorModel::tare() {
  if (!ready_ || !scale_.is_ready()) return false;
  scale_.tare(8);
  smoothedWeightKg_ = 0.0f;
  agitation_ = 0.0f;
  calmness_ = 0.0f;
  stableSeconds_ = 0.0f;
  state_.occupied = false;
  state_.weightKg = 0.0f;
  state_.agitation = 0.0f;
  state_.calmness = 0.0f;
  return true;
}

bool SensorModel::consumeActivation() {
  const bool pending = activationPending_;
  activationPending_ = false;
  return pending;
}

}  // namespace jelly
