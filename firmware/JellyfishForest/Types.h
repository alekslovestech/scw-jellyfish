#pragma once

#include <Arduino.h>

namespace jelly {

// Arduino-ESP32 2.x compiles sketches as GNU++11. Explicit constructors keep
// brace construction and brace assignment valid even though these types have
// default values; relying on C++14 aggregate rules breaks in the Arduino IDE.
struct Vec3 {
  float x;
  float y;
  float z;

  constexpr Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
  constexpr Vec3(float xValue, float yValue, float zValue)
      : x(xValue), y(yValue), z(zValue) {}
};

struct ColorF {
  float r;
  float g;
  float b;

  constexpr ColorF() : r(0.0f), g(0.0f), b(0.0f) {}
  constexpr ColorF(float red, float green, float blue)
      : r(red), g(green), b(blue) {}
};

enum class PatternId : uint8_t {
  Heartbeat,
  Demo,
  Ripple,
  FireSpread,
  Waterfall,
  TwoToneDiffuse,
  ColorWheel,
  BottomFill,
  SensorDemo,
  FallingRain,
  MovementSimulation,
  InnerSpreadWave,
  RainbowWave,
  Crazy,
  Sparkle,
  None,
};

enum class ShowMode : uint8_t {
  Auto,
  Fallback,
  Forest,
  Chorus,
  Blackout,
};

enum class SceneId : uint8_t {
  InteractiveForest,
  ManualForest,
  ManualChorus,
  LegacyFallback,
  Blackout,
};

struct PatternParameters {
  float brightness = 0.72f;
  float speed = 0.45f;
  float scale = 1.0f;
  float density = 0.45f;
  float hue = 0.52f;
  float hue2 = 0.72f;
  float contrast = 0.65f;
  float sparkle = 0.10f;
};

struct SensorTuning {
  float calibrationFactor = -14850.0f;
  float occupancyOnKg = 18.0f;
  float occupancyOffKg = 10.0f;
  float smoothing = 0.20f;
  float noiseFloorKg = 0.015f;
  float movementScaleKg = 0.20f;
  float stillnessThreshold = 0.12f;
  float calmBuildSeconds = 35.0f;
  float calmRiseSeconds = 8.0f;
  float calmFallSeconds = 1.5f;
  uint32_t sampleIntervalMs = 80;
};

struct InteractionTuning {
  uint8_t expectedPlatformCount = 4;
  float influenceRadiusMeters = 5.0f;
  float calmThreshold = 0.86f;
  float calmReleaseThreshold = 0.68f;
  float maxAgitationForChorus = 0.16f;
  float allCalmHoldSeconds = 12.0f;
  float chorusFadeSeconds = 8.0f;
  float activationWaveSpeedMetersPerSecond = 5.5f;
  float activationWaveWidthMeters = 0.65f;
  float activationWaveDurationSeconds = 5.0f;
};

struct DeviceSettings {
  uint8_t deviceId = 0;
  String name;
  bool hasScale = false;
  bool isJelly = false;
  bool isBig = false;
  Vec3 position;
  float rotationYDegrees = 0.0f;
  PatternId fallbackPattern = PatternId::Demo;
  ShowMode showMode = ShowMode::Auto;
  bool localShowOverride = false;
  ShowMode localOverrideMode = ShowMode::Fallback;
  PatternParameters pattern;
  SensorTuning sensor;
  InteractionTuning interaction;
  float masterBrightness = 1.0f;
  uint32_t powerLimitMilliAmps = 12000;
};

struct PlatformState {
  bool valid = false;
  uint8_t id = 0;
  uint32_t sequence = 0;
  Vec3 position;
  float weightKg = 0.0f;
  float agitation = 0.0f;
  float calmness = 0.0f;
  bool occupied = false;
  uint64_t senderShowTimeMs = 0;
  uint64_t lastSeenLocalMs = 0;
};

struct FieldState {
  float presence = 0.0f;
  float agitation = 0.0f;
  float calmness = 0.0f;
  float harmony = 0.0f;
  float synchronization = 0.0f;
  float turbulence = 0.0f;
  float brightness = 0.0f;
};

struct InstallationState {
  uint8_t seenPlatforms = 0;
  uint8_t occupiedPlatforms = 0;
  float meanCalmness = 0.0f;
  float minimumCalmness = 0.0f;
  float maximumAgitation = 0.0f;
  float chorus = 0.0f;
  bool allCalmLatched = false;
  uint64_t chorusStartedShowTimeMs = 0;
  uint32_t sequence = 0;
};

struct ActivationEvent {
  uint32_t eventId = 0;
  uint8_t platformId = 0;
  Vec3 origin;
  float weightKg = 0.0f;
  float agitation = 0.0f;
  uint64_t startShowTimeMs = 0;
};

struct AudioState {
  float level = 0.0f;
  float bass = 0.0f;
  float mid = 0.0f;
  float high = 0.0f;
  bool beat = false;
  uint64_t lastPacketLocalMs = 0;
};

const char* patternName(PatternId value);
bool parsePattern(const String& value, PatternId& out);
const char* showModeName(ShowMode value);
bool parseShowMode(const String& value, ShowMode& out);
const char* sceneName(SceneId value);

}  // namespace jelly
