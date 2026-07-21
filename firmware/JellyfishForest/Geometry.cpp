#include "Geometry.h"
#include <math.h>
#include "MathUtils.h"

namespace jelly {

uint16_t Geometry::ledCountPerStrip() const {
  return settings_.isBig ? config::kBigLedsPerStrip : config::kSmallLedsPerStrip;
}

float Geometry::maximumDepthMeters() const {
  return settings_.isBig ? 4.9f : 1.8f;
}

float Geometry::maximumRadiusMeters() const {
  return settings_.isBig ? 2.55f : 0.60f;
}

Vec3 Geometry::findLocalPosition(uint16_t pixel, uint8_t strip) const {
  float radius = 0.0f;
  float y = 0.0f;

  if (settings_.isBig) {
    if (pixel < 50) {
      radius = 0.0f;
      y = -0.1f * pixel;
    } else if (pixel < 100) {
      radius = 0.15f;
      y = 0.1f * (static_cast<int>(pixel) - 100);
    } else if (pixel < 125) {
      radius = 0.15f + 0.1f * (pixel - 100);
      y = 0.0f;
    } else {
      radius = 2.55f;
      y = -0.1f * (pixel - 125);
    }
  } else {
    if (pixel < 17) {
      radius = 0.0f;
      y = -0.1f * pixel;
    } else if (pixel < 35) {
      radius = 0.15f;
      y = 0.1f * (static_cast<int>(pixel) - 35);
    } else if (pixel < 40) {
      radius = 0.1f * (static_cast<int>(pixel) - 33);
      y = 0.0f;
    } else {
      radius = 0.60f;
      y = 0.1f * (40 - static_cast<int>(pixel));
    }
  }

  const float angle = TWO_PI * strip / config::kStripCount;
  return {cosf(angle) * radius, y, sinf(angle) * radius};
}

void Geometry::recalculate() {
  for (uint8_t strip = 0; strip < config::kStripCount; ++strip) {
    for (uint16_t pixel = 0; pixel < config::kMaxLedsPerStrip; ++pixel) {
      positions_[strip][pixel] = findLocalPosition(pixel, strip);
    }
  }
}

const Vec3& Geometry::localPosition(uint8_t strip, uint16_t pixel) const {
  return positions_[strip][pixel];
}

Vec3 Geometry::worldPosition(uint8_t strip, uint16_t pixel) const {
  const Vec3& local = positions_[strip][pixel];
  const float angle = settings_.rotationYDegrees * DEG_TO_RAD;
  const float cosine = cosf(angle);
  const float sine = sinf(angle);
  return {
      settings_.position.x + cosine * local.x + sine * local.z,
      settings_.position.y + local.y,
      settings_.position.z - sine * local.x + cosine * local.z,
  };
}

PathCoordinate Geometry::pathCoordinate(uint16_t pixel) const {
  PathCoordinate result;
  const Vec3& local = positions_[0][pixel];
  result.depthT = clamp01(-local.y / maximumDepthMeters());
  result.radiusT = clamp01(sqrtf(local.x * local.x + local.z * local.z) / maximumRadiusMeters());

  if (settings_.isBig) {
    if (pixel < 100) {
      result.segment = JellySegment::Inner;
      result.segmentT = pixel / 99.0f;
      result.pathT = (1.0f - result.depthT) * 0.60f;
    } else if (pixel < 125) {
      result.segment = JellySegment::Bell;
      result.segmentT = (pixel - 100) / 24.0f;
      result.pathT = 0.60f + result.segmentT * 0.20f;
    } else {
      result.segment = JellySegment::Outer;
      result.segmentT = (pixel - 125) / 24.0f;
      result.pathT = 0.80f + result.segmentT * 0.20f;
    }
  } else {
    if (pixel < 35) {
      result.segment = JellySegment::Inner;
      result.segmentT = pixel / 34.0f;
      result.pathT = (1.0f - result.depthT) * 0.60f;
    } else if (pixel < 40) {
      result.segment = JellySegment::Bell;
      result.segmentT = (pixel - 35) / 4.0f;
      result.pathT = 0.60f + result.segmentT * 0.20f;
    } else {
      result.segment = JellySegment::Outer;
      result.segmentT = (pixel - 40) / 9.0f;
      result.pathT = 0.80f + result.segmentT * 0.20f;
    }
  }

  result.segmentT = clamp01(result.segmentT);
  result.pathT = clamp01(result.pathT);
  return result;
}

}  // namespace jelly
