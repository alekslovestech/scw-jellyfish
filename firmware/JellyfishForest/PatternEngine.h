#pragma once

#include <NeoPixelBus.h>
#include "AppConfig.h"
#include "Geometry.h"
#include "InteractionEngine.h"
#include "Types.h"

namespace jelly {

// Use NeoPixelBus' architecture-selected eight-channel parallel method.
// On a classic ESP32 this resolves to the I2S parallel backend; on an ESP32-S3
// it resolves to the LCD parallel backend.  Naming the LCD method directly
// makes the sketch fail to compile for the classic ESP32 used by Arduino's
// "ESP32 Dev Module" target.
using StripBus = NeoPixelBus<NeoBrgFeature, X8Ws2812xMethod>;

class PatternEngine {
 public:
  PatternEngine(
      DeviceSettings& settings,
      const Geometry& geometry,
      const InteractionEngine& interaction)
      : settings_(settings), geometry_(geometry), interaction_(interaction) {}

  void begin();
  void tick(
      uint64_t localNowMs,
      uint64_t showNowMs,
      const InstallationState& installation,
      const AudioState& audio);
  void startActivation(const ActivationEvent& event, uint64_t showNowMs);
  void identify(uint64_t localNowMs);

  SceneId scene() const { return scene_; }
  const char* sceneLabel() const { return sceneName(scene_); }
  uint64_t renderedFrames() const { return renderedFrames_; }

 private:
  struct RippleSlot {
    bool active = false;
    ActivationEvent event;
  };

  SceneId effectiveScene() const;
  void beginSceneTransition(SceneId next, uint64_t localNowMs);
  void captureDisplayedFrame();
  void clearFrame();
  void renderScene(
      SceneId scene,
      float showSeconds,
      uint64_t localNowMs,
      const InstallationState& installation,
      const AudioState& audio);
  void renderInteractiveForest(float showSeconds, const InstallationState& installation);
  void renderManualForest(float showSeconds);
  void renderChorus(float showSeconds, float strength);
  void renderLegacy(float showSeconds, const AudioState& audio);
  void renderBlackout();

  ColorF forestPixel(
      uint8_t strip,
      uint16_t pixel,
      float showSeconds,
      const FieldState& field,
      float chorusAmount,
      float simulatedAgitation = 0.0f) const;
  ColorF chorusPixel(uint8_t strip, uint16_t pixel, float showSeconds) const;
  ColorF legacyPixel(
      PatternId pattern,
      uint8_t strip,
      uint16_t pixel,
      float showSeconds,
      const AudioState& audio);

  void overlayActivationWaves(uint64_t showNowMs);
  void applyIdentifyOverlay(uint64_t localNowMs);
  void convertAndShow(uint64_t localNowMs);
  uint8_t gammaByte(float value) const;

  DeviceSettings& settings_;
  const Geometry& geometry_;
  const InteractionEngine& interaction_;

  ColorF frame_[config::kStripCount][config::kMaxLedsPerStrip] = {};
  RgbColor output_[config::kStripCount][config::kMaxLedsPerStrip] = {};
  RgbColor transitionFrom_[config::kStripCount][config::kMaxLedsPerStrip] = {};
  RippleSlot ripples_[config::kMaxActivationWaves] = {};

  SceneId scene_ = SceneId::InteractiveForest;
  uint64_t lastFrameLocalMs_ = 0;
  uint64_t transitionStartedLocalMs_ = 0;
  uint64_t identifyUntilLocalMs_ = 0;
  uint64_t renderedFrames_ = 0;
  bool legacyPatternInitialized_ = false;
  PatternId activeLegacyPattern_ = PatternId::None;
  float legacyPatternStartedShowSeconds_ = 0.0f;
  uint32_t rippleCycle_ = UINT32_MAX;
  Vec3 rippleCenter_;
};

}  // namespace jelly
