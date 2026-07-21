#include "Types.h"

namespace jelly {

const char* patternName(PatternId value) {
  switch (value) {
    case PatternId::Heartbeat: return "heartbeat";
    case PatternId::Demo: return "demo";
    case PatternId::Ripple: return "ripple";
    case PatternId::FireSpread: return "fireSpread";
    case PatternId::Waterfall: return "waterfall";
    case PatternId::TwoToneDiffuse: return "twoToneDiffuse";
    case PatternId::ColorWheel: return "colorwheel";
    case PatternId::BottomFill: return "bottomfill";
    case PatternId::SensorDemo: return "sensordemo";
    case PatternId::FallingRain: return "fallingRain";
    case PatternId::MovementSimulation: return "movementSimulation";
    case PatternId::InnerSpreadWave: return "innerSpreadWave";
    case PatternId::RainbowWave: return "rainbowWave";
    case PatternId::Crazy: return "crazy";
    case PatternId::Sparkle: return "sparkle";
    case PatternId::None: return "none";
  }
  return "demo";
}

bool parsePattern(const String& value, PatternId& out) {
  for (uint8_t raw = static_cast<uint8_t>(PatternId::Heartbeat);
       raw <= static_cast<uint8_t>(PatternId::None); ++raw) {
    const PatternId candidate = static_cast<PatternId>(raw);
    if (value.equalsIgnoreCase(patternName(candidate))) {
      out = candidate;
      return true;
    }
  }
  return false;
}

const char* showModeName(ShowMode value) {
  switch (value) {
    case ShowMode::Auto: return "auto";
    case ShowMode::Fallback: return "fallback";
    case ShowMode::Forest: return "forest";
    case ShowMode::Chorus: return "chorus";
    case ShowMode::Blackout: return "blackout";
  }
  return "auto";
}

bool parseShowMode(const String& value, ShowMode& out) {
  for (uint8_t raw = static_cast<uint8_t>(ShowMode::Auto);
       raw <= static_cast<uint8_t>(ShowMode::Blackout); ++raw) {
    const ShowMode candidate = static_cast<ShowMode>(raw);
    if (value.equalsIgnoreCase(showModeName(candidate))) {
      out = candidate;
      return true;
    }
  }
  return false;
}

const char* sceneName(SceneId value) {
  switch (value) {
    case SceneId::InteractiveForest: return "interactive-forest";
    case SceneId::ManualForest: return "forest";
    case SceneId::ManualChorus: return "chorus";
    case SceneId::LegacyFallback: return "fallback";
    case SceneId::Blackout: return "blackout";
  }
  return "interactive-forest";
}

}  // namespace jelly
