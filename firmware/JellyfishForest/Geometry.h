#pragma once

#include "AppConfig.h"
#include "Types.h"

namespace jelly {

enum class JellySegment : uint8_t {
  Inner,
  Bell,
  Outer,
};

struct PathCoordinate {
  JellySegment segment = JellySegment::Inner;
  float segmentT = 0.0f;
  float pathT = 0.0f;
  float depthT = 0.0f;
  float radiusT = 0.0f;
};

class Geometry {
 public:
  explicit Geometry(const DeviceSettings& settings) : settings_(settings) {}

  void recalculate();
  uint8_t stripCount() const { return config::kStripCount; }
  uint16_t ledCountPerStrip() const;
  float maximumDepthMeters() const;
  float maximumRadiusMeters() const;
  const Vec3& localPosition(uint8_t strip, uint16_t pixel) const;
  Vec3 worldPosition(uint8_t strip, uint16_t pixel) const;
  PathCoordinate pathCoordinate(uint16_t pixel) const;

 private:
  Vec3 findLocalPosition(uint16_t pixel, uint8_t strip) const;
  const DeviceSettings& settings_;
  Vec3 positions_[config::kStripCount][config::kMaxLedsPerStrip] = {};
};

}  // namespace jelly
