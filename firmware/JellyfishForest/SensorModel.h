#pragma once

#include <HX711.h>
#include "Types.h"

namespace jelly {

class SensorModel {
 public:
  explicit SensorModel(HX711& scale) : scale_(scale) {}

  bool begin(const DeviceSettings& settings);
  void tick(uint64_t localNowMs, const DeviceSettings& settings);
  bool tare();
  void applyTuning(const SensorTuning& tuning);
  bool ready() const { return ready_; }
  const String& status() const { return status_; }
  const PlatformState& state() const { return state_; }
  bool consumeActivation();

 private:
  HX711& scale_;
  PlatformState state_;
  bool ready_ = false;
  bool activationPending_ = false;
  float smoothedWeightKg_ = 0.0f;
  float agitation_ = 0.0f;
  float calmness_ = 0.0f;
  float stableSeconds_ = 0.0f;
  uint64_t lastSampleLocalMs_ = 0;
  uint32_t sequence_ = 0;
  String status_ = "disabled";
};

}  // namespace jelly
