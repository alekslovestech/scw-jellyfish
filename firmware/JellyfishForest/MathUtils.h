#pragma once

#include <Arduino.h>
#include <math.h>
#include "Types.h"

namespace jelly {

inline float clamp01(float value) {
  return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

inline float smoothstep(float edge0, float edge1, float value) {
  if (edge0 == edge1) return value < edge0 ? 0.0f : 1.0f;
  const float t = clamp01((value - edge0) / (edge1 - edge0));
  return t * t * (3.0f - 2.0f * t);
}

inline float lerp(float a, float b, float amount) {
  return a + (b - a) * amount;
}

inline ColorF lerpColor(const ColorF& a, const ColorF& b, float amount) {
  return {lerp(a.r, b.r, amount), lerp(a.g, b.g, amount), lerp(a.b, b.b, amount)};
}

inline ColorF addColor(const ColorF& a, const ColorF& b) {
  return {a.r + b.r, a.g + b.g, a.b + b.b};
}

inline ColorF scaleColor(const ColorF& color, float scale) {
  return {color.r * scale, color.g * scale, color.b * scale};
}

inline ColorF clampColor(const ColorF& color) {
  return {clamp01(color.r), clamp01(color.g), clamp01(color.b)};
}

inline float distanceSquared(const Vec3& a, const Vec3& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return dx * dx + dy * dy + dz * dz;
}

inline float distance(const Vec3& a, const Vec3& b) {
  return sqrtf(distanceSquared(a, b));
}

inline float exponentialAlpha(float dtSeconds, float timeConstantSeconds) {
  if (timeConstantSeconds <= 0.0001f) return 1.0f;
  return 1.0f - expf(-dtSeconds / timeConstantSeconds);
}

inline uint32_t hash32(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  value ^= value >> 16;
  return value;
}

inline float hash01(uint32_t value) {
  return (hash32(value) & 0x00FFFFFFU) / 16777215.0f;
}

inline ColorF hsv(float hue, float saturation, float value) {
  hue -= floorf(hue);
  saturation = clamp01(saturation);
  value = max(0.0f, value);

  const float h = hue * 6.0f;
  const int sector = static_cast<int>(floorf(h));
  const float fraction = h - sector;
  const float p = value * (1.0f - saturation);
  const float q = value * (1.0f - saturation * fraction);
  const float t = value * (1.0f - saturation * (1.0f - fraction));

  switch (sector % 6) {
    case 0: return {value, t, p};
    case 1: return {q, value, p};
    case 2: return {p, value, t};
    case 3: return {p, q, value};
    case 4: return {t, p, value};
    default: return {value, p, q};
  }
}

}  // namespace jelly
