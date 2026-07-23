#include "PatternEngine.h"

#include <math.h>
#include "LogBuffer.h"
#include "MathUtils.h"

namespace jelly {
namespace {

// Music-volume modulation is deliberately a final global multiplier. It does
// not alter scene structure, colours, platform calmness, or power-limit logic.
constexpr float kMusicMinimumBrightness = 0.15f;
constexpr float kMusicMaximumBrightness = 1.00f;
constexpr uint64_t kMusicPacketTimeoutMs = 1000ULL;
constexpr float kMusicAttackSeconds = 0.055f;
constexpr float kMusicReleaseSeconds = 0.220f;

StripBus gStrips[config::kStripCount] = {
    {config::kMaxLedsPerStrip, config::kLedPins[0]},
    {config::kMaxLedsPerStrip, config::kLedPins[1]},
    {config::kMaxLedsPerStrip, config::kLedPins[2]},
    {config::kMaxLedsPerStrip, config::kLedPins[3]},
    {config::kMaxLedsPerStrip, config::kLedPins[4]},
    {config::kMaxLedsPerStrip, config::kLedPins[5]},
    {config::kMaxLedsPerStrip, config::kLedPins[6]},
    {config::kMaxLedsPerStrip, config::kLedPins[7]},
};

constexpr float kSceneTransitionSeconds = 1.8f;

struct FireGradientStop {
  float position;
  float r;
  float g;
  float b;
};

const FireGradientStop kFireBellOuter[4] = {
    {0.00f, 1.0f, 1.0f, 1.0f},
    {0.20f, 1.0f, 1.0f, 0.0f},
    {0.40f, 1.0f, 0.2f, 0.0f},
    {0.80f, 1.0f, 0.0f, 0.0f},
};
const FireGradientStop kFireGreenBellOuter[4] = {
    {0.00f, 1.0f, 1.0f, 1.0f},
    {0.20f, 0.6f, 1.0f, 0.0f},
    {0.40f, 0.1f, 0.9f, 0.0f},
    {0.80f, 0.0f, 0.4f, 0.0f},
};
const FireGradientStop kFireBlueBellOuter[4] = {
    {0.00f, 1.0f, 1.0f, 1.0f},
    {0.20f, 0.4f, 0.8f, 1.0f},
    {0.40f, 0.1f, 0.3f, 1.0f},
    {0.80f, 0.1f, 0.0f, 0.6f},
};
const FireGradientStop kFireInner[4] = {
    {0.00f, 1.0f, 1.0f, 1.0f},
    {0.20f, 1.0f, 1.0f, 0.0f},
    {0.50f, 1.0f, 0.2f, 0.0f},
    {0.90f, 1.0f, 0.0f, 0.0f},
};
const FireGradientStop kFireGreenInner[4] = {
    {0.00f, 1.0f, 1.0f, 1.0f},
    {0.20f, 0.6f, 1.0f, 0.0f},
    {0.50f, 0.1f, 0.9f, 0.0f},
    {0.90f, 0.0f, 0.4f, 0.0f},
};
const FireGradientStop kFireBlueInner[4] = {
    {0.00f, 1.0f, 1.0f, 1.0f},
    {0.20f, 0.4f, 0.8f, 1.0f},
    {0.50f, 0.1f, 0.3f, 1.0f},
    {0.90f, 0.1f, 0.0f, 0.6f},
};

ColorF white(float value) {
  return {value, value, value};
}

float legacyRippleProfile(float x) {
  if (x <= -2.0f || x >= 2.0f) return 0.0f;
  const float x2 = x * x;
  const float a = x2 - 4.0f;
  const float b = x2 - 1.0f;
  return clamp01((a * a * b * b) / 16.0f);
}

// A sine crest with a true zero interval. `darkness` raises the cutoff and
// therefore widens the black gap between consecutive crests; `power` sharpens
// the surviving light. Unlike a conventional 0.5 + 0.5 * sin() wave, this can
// never create an all-pixel illumination floor.
float deepCrest(float phase, float darkness, float power) {
  const float threshold = lerp(-0.22f, 0.58f, clamp01(darkness));
  const float wave = sinf(phase);
  if (wave <= threshold) return 0.0f;
  const float normalized = clamp01((wave - threshold) / (1.0f - threshold));
  return powf(normalized, max(0.25f, power));
}

// Matches the root-to-branch coordinate used by the proven FireSpread
// fallback. Zero is the bell centre/root; one is the end of the active branch.
float fireRootCoordinate(const PathCoordinate& path) {
  if (path.segment == JellySegment::Bell) {
    return path.segmentT * 0.5f;
  }
  if (path.segment == JellySegment::Outer) {
    return 0.5f + path.segmentT * 0.5f;
  }
  return 1.0f - path.segmentT;
}

ColorF sampleFireGradient(const FireGradientStop* stops, float position) {
  const float p = clamp01(position);
  if (p <= stops[0].position) {
    return {stops[0].r, stops[0].g, stops[0].b};
  }
  if (p >= stops[3].position) {
    return {stops[3].r, stops[3].g, stops[3].b};
  }
  for (uint8_t index = 1; index < 4; ++index) {
    if (p <= stops[index].position) {
      const FireGradientStop& a = stops[index - 1];
      const FireGradientStop& b = stops[index];
      const float amount = (p - a.position) / (b.position - a.position);
      return {
          lerp(a.r, b.r, amount),
          lerp(a.g, b.g, amount),
          lerp(a.b, b.b, amount),
      };
    }
  }
  return {stops[3].r, stops[3].g, stops[3].b};
}

ColorF blendFireGradients(
    const FireGradientStop* from,
    const FireGradientStop* to,
    float position,
    float amount) {
  return lerpColor(
      sampleFireGradient(from, position),
      sampleFireGradient(to, position),
      clamp01(amount));
}

ColorF cyclingFireColor(
    const FireGradientStop* redGradient,
    const FireGradientStop* greenGradient,
    const FireGradientStop* blueGradient,
    float position,
    float timeSeconds,
    uint16_t deviceId) {
  // Preserve the original red -> green -> blue progression, then smoothly
  // return through magenta/orange to the red/yellow fire palette and repeat.
  // The transition remains staggered between jellyfish, just like the ported
  // implementation, but no longer becomes permanently blue.
  constexpr float kRedHoldEnd = 30.0f;
  constexpr float kGreenTransitionEnd = 50.0f;
  constexpr float kGreenHoldEnd = 55.0f;
  constexpr float kBlueTransitionEnd = 75.0f;
  constexpr float kBlueHoldEnd = 85.0f;
  constexpr float kCycleEnd = 105.0f;
  constexpr float kJellyStaggerSeconds = 1.5f;

  const float staggeredTime = max(
      0.0f,
      timeSeconds - static_cast<float>(deviceId) * kJellyStaggerSeconds);
  const float cycleTime = fmodf(staggeredTime, kCycleEnd);

  if (cycleTime < kRedHoldEnd) {
    return sampleFireGradient(redGradient, position);
  }
  if (cycleTime < kGreenTransitionEnd) {
    const float amount = (cycleTime - kRedHoldEnd) /
        (kGreenTransitionEnd - kRedHoldEnd);
    return blendFireGradients(
        redGradient, greenGradient, position, amount);
  }
  if (cycleTime < kGreenHoldEnd) {
    return sampleFireGradient(greenGradient, position);
  }
  if (cycleTime < kBlueTransitionEnd) {
    const float amount = (cycleTime - kGreenHoldEnd) /
        (kBlueTransitionEnd - kGreenHoldEnd);
    return blendFireGradients(
        greenGradient, blueGradient, position, amount);
  }
  if (cycleTime < kBlueHoldEnd) {
    return sampleFireGradient(blueGradient, position);
  }

  const float amount = (cycleTime - kBlueHoldEnd) /
      (kCycleEnd - kBlueHoldEnd);
  return blendFireGradients(
      blueGradient, redGradient, position, amount);
}

}  // namespace

void PatternEngine::begin() {
  for (auto& strip : gStrips) {
    strip.Begin();
    strip.ClearTo(RgbColor(0));
    strip.Show();
  }
  clearFrame();
  captureDisplayedFrame();
  Log.println("LED renderer initialized.");
}

SceneId PatternEngine::effectiveScene() const {
  if (!settings_.isJelly) return SceneId::Blackout;
  const ShowMode mode = settings_.localShowOverride
      ? settings_.localOverrideMode
      : settings_.showMode;
  switch (mode) {
    case ShowMode::Auto: return SceneId::InteractiveForest;
    case ShowMode::Fallback: return SceneId::LegacyFallback;
    case ShowMode::Forest: return SceneId::ManualForest;
    case ShowMode::Chorus: return SceneId::ManualChorus;
    case ShowMode::Blackout: return SceneId::Blackout;
  }
  return SceneId::InteractiveForest;
}

void PatternEngine::tick(
    uint64_t localNowMs,
    uint64_t showNowMs,
    const InstallationState& installation,
    const AudioState& audio) {
  if (lastFrameLocalMs_ != 0 &&
      localNowMs - lastFrameLocalMs_ < config::kRenderIntervalMs) {
    return;
  }
  if (lastFrameLocalMs_ == 0) {
    lastFrameLocalMs_ = localNowMs;
  } else {
    lastFrameLocalMs_ += config::kRenderIntervalMs;
    if (localNowMs - lastFrameLocalMs_ > 200ULL) lastFrameLocalMs_ = localNowMs;
  }

  const SceneId target = effectiveScene();
  if (target != scene_) beginSceneTransition(target, localNowMs);

  clearFrame();
  renderScene(scene_, showNowMs / 1000.0f, localNowMs, installation, audio);
  overlayActivationWaves(showNowMs);
  applyIdentifyOverlay(localNowMs);
  updateMusicBrightness(localNowMs, audio);
  convertAndShow(localNowMs);
  ++renderedFrames_;
}

void PatternEngine::beginSceneTransition(SceneId next, uint64_t localNowMs) {
  captureDisplayedFrame();
  scene_ = next;
  transitionStartedLocalMs_ = localNowMs;
  if (next == SceneId::LegacyFallback) {
    legacyPatternInitialized_ = false;
  }
  Log.printf("Scene changed to %s\n", sceneName(scene_));
}

void PatternEngine::captureDisplayedFrame() {
  for (uint8_t strip = 0; strip < config::kStripCount; ++strip) {
    for (uint16_t pixel = 0; pixel < config::kMaxLedsPerStrip; ++pixel) {
      transitionFrom_[strip][pixel] = gStrips[strip].GetPixelColor(pixel);
    }
  }
}

void PatternEngine::clearFrame() {
  for (uint8_t strip = 0; strip < config::kStripCount; ++strip) {
    for (uint16_t pixel = 0; pixel < config::kMaxLedsPerStrip; ++pixel) {
      frame_[strip][pixel] = {};
    }
  }
}

void PatternEngine::renderScene(
    SceneId scene,
    float showSeconds,
    uint64_t localNowMs,
    const InstallationState& installation,
    const AudioState& audio) {
  (void)localNowMs;
  switch (scene) {
    case SceneId::InteractiveForest:
      renderInteractiveForest(showSeconds, installation);
      break;
    case SceneId::ManualForest:
      renderManualForest(showSeconds);
      break;
    case SceneId::ManualChorus:
      renderChorus(showSeconds, 1.0f);
      break;
    case SceneId::LegacyFallback:
      renderLegacy(showSeconds, audio);
      break;
    case SceneId::Blackout:
      renderBlackout();
      break;
  }
}

void PatternEngine::renderInteractiveForest(
    float showSeconds,
    const InstallationState& installation) {
  const float chorusAmount = clamp01(installation.chorus);
  const uint16_t count = geometry_.ledCountPerStrip();
  for (uint8_t strip = 0; strip < config::kStripCount; ++strip) {
    for (uint16_t pixel = 0; pixel < count; ++pixel) {
      const Vec3 world = geometry_.worldPosition(strip, pixel);
      const FieldState field = interaction_.sampleField(world, settings_.interaction);
      const ColorF forest = forestPixel(strip, pixel, showSeconds, field, chorusAmount);
      const ColorF chorus = chorusPixel(strip, pixel, showSeconds);
      frame_[strip][pixel] = lerpColor(forest, chorus, chorusAmount);
    }
  }
}

void PatternEngine::renderManualForest(float showSeconds) {
  const uint16_t count = geometry_.ledCountPerStrip();
  const FieldState ambient;
  for (uint8_t strip = 0; strip < config::kStripCount; ++strip) {
    for (uint16_t pixel = 0; pixel < count; ++pixel) {
      frame_[strip][pixel] = forestPixel(strip, pixel, showSeconds, ambient, 0.0f);
    }
  }
}

void PatternEngine::renderChorus(float showSeconds, float strength) {
  const uint16_t count = geometry_.ledCountPerStrip();
  for (uint8_t strip = 0; strip < config::kStripCount; ++strip) {
    for (uint16_t pixel = 0; pixel < count; ++pixel) {
      frame_[strip][pixel] = scaleColor(chorusPixel(strip, pixel, showSeconds), strength);
    }
  }
}

ColorF PatternEngine::forestPixel(
    uint8_t strip,
    uint16_t pixel,
    float showSeconds,
    const FieldState& field,
    float chorusAmount,
    float simulatedAgitation) const {
  const Vec3 local = geometry_.localPosition(strip, pixel);
  const PathCoordinate path = geometry_.pathCoordinate(pixel);
  const uint32_t ledKey =
      settings_.deviceId * 100003U + strip * 4099U + pixel * 97U;

  // -----------------------------------------------------------------------
  // Ambient forest
  // -----------------------------------------------------------------------
  // Darkness is the default. Individual pixels receive long, independently
  // phased bioluminescent blooms, and each jellyfish occasionally gets one
  // much faster spherical shimmer. There is deliberately no permanent base
  // level: outside these sparse events an unoccupied jellyfish pixel is off.

  const float sparkleControl = clamp01(settings_.pattern.sparkle);
  const float sparklePeriod =
      lerp(55.0f, 110.0f, hash01(ledKey ^ 0xA341316CU)) /
      lerp(0.80f, 1.25f, sparkleControl);
  const float sparkleDuration =
      lerp(7.0f, 16.0f, hash01(ledKey ^ 0xC8013EA4U)) /
      lerp(0.85f, 1.15f, settings_.pattern.speed);
  const float sparkleShiftedTime =
      showSeconds + hash01(ledKey ^ 0xAD90777DU) * sparklePeriod;
  const uint32_t sparkleCycle = static_cast<uint32_t>(
      floorf(sparkleShiftedTime / sparklePeriod));
  const float sparklePhase = sparkleShiftedTime - sparkleCycle * sparklePeriod;
  // Square-root response keeps the useful low end broad: the persisted
  // default of 0.10 produces a sparse but visible field, while 0 disables
  // these blooms completely.
  const float sparkleChance = 0.78f * sqrtf(sparkleControl);
  const bool sparkleChosen = hash01(
      ledKey ^ (sparkleCycle * 0x9E3779B9U)) < sparkleChance;

  float sparkleEnvelope = 0.0f;
  if (sparkleChosen && sparklePhase < sparkleDuration) {
    const float progress = sparklePhase / sparkleDuration;
    const float sine = sinf(PI * progress);
    // Squaring the sine gives a long, gentle fade with exact darkness at
    // either end. smoothstep softens its middle without creating a plateau.
    sparkleEnvelope = smoothstep(0.0f, 1.0f, sine * sine);
  }

  const float sparklePeak =
      lerp(0.38f, 0.85f, hash01(ledKey ^ 0x7E95761EU)) *
      lerp(0.85f, 1.0f, sparkleControl);
  const float sparkleHue = lerp(
      settings_.pattern.hue,
      settings_.pattern.hue2,
      0.18f + 0.72f * hash01(ledKey ^ 0xB7E15162U));
  ColorF ambient = hsv(
      sparkleHue,
      lerp(0.58f, 0.88f, hash01(ledKey ^ 0x243F6A88U)),
      sparkleEnvelope * sparklePeak);

  // A fixed-length per-device schedule ensures that neighbouring
  // jellyfish do not shimmer together. A cycle-specific gate skips many
  // candidate events, so the ripple can be relatively fast and bright while
  // remaining rare across the forest.
  const uint32_t jellyKey =
      settings_.deviceId * 0x045D9F3BU + 0x27D4EB2DU;
  const float ripplePeriod =
      lerp(115.0f, 65.0f, settings_.pattern.density) *
      lerp(0.82f, 1.18f, hash01(jellyKey ^ 0xB5297A4DU));
  const float rippleDuration =
      lerp(5.2f, 3.2f, settings_.pattern.speed);
  const float rippleShiftedTime =
      showSeconds + hash01(jellyKey ^ 0x68E31DA4U) * ripplePeriod;
  const uint32_t rippleCycle = static_cast<uint32_t>(
      floorf(rippleShiftedTime / ripplePeriod));
  const float rippleAge = rippleShiftedTime - rippleCycle * ripplePeriod;
  const float rippleDensity = clamp01(settings_.pattern.density);
  // As with sparkle, zero is a real off switch. The default density accepts
  // roughly half the candidate cycles, or one shimmer per jellyfish every
  // two to three minutes before device-specific timing variation.
  const float rippleChance = 0.82f * sqrtf(rippleDensity);
  const uint32_t rippleEventKey =
      jellyKey ^ (rippleCycle * 0x9E3779B9U);
  const bool rippleChosen =
      hash01(rippleEventKey ^ 0xD1B54A35U) < rippleChance;

  if (rippleChosen && rippleAge < rippleDuration) {
    const float progress = rippleAge / rippleDuration;
    const float maximumRadius = max(0.1f, geometry_.maximumRadiusMeters());
    const float maximumDepth = max(0.1f, geometry_.maximumDepthMeters());
    const Vec3 normalizedLocal(
        local.x / maximumRadius,
        local.y / maximumDepth,
        local.z / maximumRadius);
    const Vec3 rippleCenter(
        lerp(-0.62f, 0.62f, hash01(rippleEventKey ^ 0x94D049BBU)),
        lerp(-0.90f, 0.08f, hash01(rippleEventKey ^ 0x369DEA0FU)),
        lerp(-0.62f, 0.62f, hash01(rippleEventKey ^ 0xDB4F0B91U)));
    const float radius = lerp(-0.08f, 2.05f, progress);
    const float width = lerp(
        0.075f,
        0.145f,
        clamp01(
            0.225f * settings_.pattern.scale +
            0.55f * hash01(rippleEventKey ^ 0xBB67AE85U)));
    const float shellDistance = fabsf(
        distance(normalizedLocal, rippleCenter) - radius);
    const float crest = expf(
        -(shellDistance * shellDistance) / (2.0f * width * width));
    const float haloWidth = width * 2.8f;
    const float halo = expf(
        -(shellDistance * shellDistance) /
        (2.0f * haloWidth * haloWidth));
    const float eventEnvelope =
        smoothstep(0.0f, 0.13f, progress) *
        smoothstep(1.0f, 0.70f, progress);
    const float stripShimmer = 0.82f + 0.18f * sinf(
        strip * 2.3999f + progress * TWO_PI * 2.0f);
    const float rippleIntensity =
        (crest * 0.90f + halo * 0.16f) *
        eventEnvelope * stripShimmer;
    const float rippleHue = lerp(
        settings_.pattern.hue,
        settings_.pattern.hue2,
        0.38f + 0.50f * hash01(rippleEventKey ^ 0x3C6EF372U));
    ambient = addColor(
        ambient,
        hsv(rippleHue, 0.82f, rippleIntensity));
  }

  const float presence = clamp01(field.presence);
  const float agitation = clamp01(max(field.agitation, simulatedAgitation));
  const float calmness = clamp01(field.calmness);

  // For an empty field, the sparse ambient events above are the entire
  // scene. Avoiding the interactive calculations also reduces work on the
  // ESP while the installation is idle.
  if (presence < 0.002f &&
      agitation < 0.002f &&
      chorusAmount < 0.002f) {
    return ambient;
  }

  // -----------------------------------------------------------------------
  // Interactive field: coordinated deep waves
  // -----------------------------------------------------------------------
  // There is no occupied-pixel floor. Every interactive photon is carried by
  // a thresholded crest, so each pixel can return to exact black between
  // waves. Calmness is deliberately rewarding in three simultaneous ways:
  //   1. crests become much brighter,
  //   2. strip/device phases converge on the shared installation clock, and
  //   3. the FireSpread-like root wave joins the InnerSpread-like path wave.
  // Agitation applies an immediate amplitude penalty and restores private,
  // rapidly moving phases before the slower calmness value has time to fall.

  const float calmReward = powf(calmness, 1.35f);
  const float agitationPenalty = lerp(1.0f, 0.28f, agitation);
  const float coherence = clamp01(
      calmReward * (1.0f - 0.94f * agitation));

  // Device position creates a continuous phase field through the forest. At
  // high calmness nearby jellyfish therefore move together without every
  // jellyfish in the room being forced to the exact same point in the wave.
  const float installationPhase =
      settings_.position.x * 0.31f +
      settings_.position.z * 0.27f +
      settings_.position.y * 0.035f;
  const float privatePhase =
      settings_.deviceId * 1.371f + strip * 2.3999f;
  const float phaseOffset = lerp(
      privatePhase, installationPhase, coherence);

  const float baseSpeed = 0.62f + settings_.pattern.speed * 1.18f;
  const float waveSpeed = baseSpeed *
      (1.0f + agitation * 2.4f - calmReward * 0.12f);
  const float frequency = 10.5f + settings_.pattern.scale * 6.0f;

  // Agitation makes the wave visibly lose its shape. The phase damage is
  // strip- and pixel-dependent but remains deterministic, avoiding random
  // discontinuities in the renderer itself.
  const float fragmentPhase = agitation * (
      strip * 0.92f +
      1.65f * sinf(
          showSeconds * (2.4f + agitation * 3.6f) +
          path.pathT * 22.0f +
          hash01(ledKey ^ 0xA4093822U) * TWO_PI));

  // The existing Contrast control is now the useful "wave depth" control:
  // higher values raise the cutoff, producing longer passages of black.
  const float waveDarkness = clamp01(
      0.47f + settings_.pattern.contrast * 0.42f +
      agitation * 0.16f - calmReward * 0.16f);
  const float wavePower =
      1.15f + settings_.pattern.contrast * 2.10f +
      agitation * 0.90f - calmReward * 0.35f;

  const float pathPhase =
      path.pathT * frequency -
      showSeconds * waveSpeed +
      phaseOffset + fragmentPhase;
  const float root = fireRootCoordinate(path);
  const float spreadPhase =
      root * (7.5f + settings_.pattern.scale * 5.0f) +
      showSeconds * waveSpeed * 0.72f +
      phaseOffset * 0.62f -
      installationPhase * 0.35f +
      fragmentPhase * 0.55f + PI * 0.35f;

  const float pathCrest = deepCrest(
      pathPhase, waveDarkness, wavePower);
  const float spreadCrest = deepCrest(
      spreadPhase,
      clamp01(waveDarkness + 0.045f),
      wavePower + 0.25f);

  float structure = pathCrest;
  if (path.segment == JellySegment::Inner) {
    // InnerSpreadWave language: a narrow travelling worm on the folded core.
    structure = max(pathCrest, spreadCrest * calmReward * 0.52f);
  } else {
    // FireSpread language: as calmness grows, a coherent root-to-branch wave
    // becomes as important as the path worm on the bell and outer branches.
    structure = max(
        pathCrest * 0.84f,
        spreadCrest * (0.38f + calmReward * 0.62f));
  }

  // Low-calm or agitated motion breaks into independent strip pulses. Those
  // pulses converge into one slow, shared modulation as coherence builds.
  const float brokenModulation =
      0.20f + 0.80f * deepCrest(
          showSeconds * (2.1f + agitation * 5.0f) +
          strip * 2.3999f +
          path.pathT * 6.8f,
          0.38f + agitation * 0.25f,
          1.35f + agitation);
  const float sharedModulation =
      0.80f + 0.20f * (
          0.5f + 0.5f * sinf(showSeconds * 0.47f + installationPhase));
  const float modulation = lerp(
      brokenModulation, sharedModulation, coherence);

  // Calmness is the principal source of brightness. Agitation is still very
  // visible because it accelerates and warms the fragments, but it is never a
  // visual reward: it immediately cuts the available amplitude by up to 72%.
  const float amplitude =
      presence *
      lerp(0.22f, 1.02f, calmReward) *
      agitationPenalty;
  const float segmentBoost = path.segment == JellySegment::Inner
      ? 1.08f
      : (path.segment == JellySegment::Bell ? 1.00f : 0.93f);
  float intensity = amplitude * structure * modulation * segmentBoost;

  // Aligned path and spread crests receive a small pearl highlight. This
  // creates the beautiful high-energy intersections without filling valleys.
  const float intersection = min(pathCrest, spreadCrest);
  intensity *= 1.0f + intersection * calmReward * 0.24f;
  intensity = min(1.18f, intensity);

  const float hueDrift =
      0.018f * sinf(showSeconds * 0.13f + path.pathT * 4.0f);
  const ColorF primary = hsv(
      settings_.pattern.hue + hueDrift,
      0.88f,
      1.0f);
  const ColorF secondary = hsv(
      settings_.pattern.hue2 - hueDrift * 0.6f,
      0.82f,
      1.0f);

  ColorF calmColor;
  if (path.segment == JellySegment::Inner) {
    calmColor = secondary;
  } else {
    // Outer branches mix the two colours as in InnerSpreadWave, but the
    // mixture is multiplied by the deep-wave envelope rather than left on.
    const float colorMix = clamp01(
        0.12f + pathCrest * 0.56f + spreadCrest * 0.34f);
    calmColor = lerpColor(primary, secondary, colorMix);
  }

  const ColorF pearl = hsv(
      0.12f + 0.02f * sinf(showSeconds * 0.19f),
      0.20f,
      1.0f);
  calmColor = lerpColor(
      calmColor,
      pearl,
      calmReward * intersection * 0.48f);

  const float angryMix =
      0.5f + 0.5f * sinf(
          showSeconds * (3.4f + agitation * 4.0f) +
          strip * 1.7f + path.pathT * 17.0f);
  const ColorF agitatedColor = lerpColor(
      hsv(0.01f, 0.98f, 1.0f),
      hsv(0.115f, 0.96f, 1.0f),
      angryMix);

  const ColorF waveColor = lerpColor(
      calmColor,
      agitatedColor,
      agitation * 0.90f);
  return addColor(ambient, scaleColor(waveColor, intensity));
}

ColorF PatternEngine::chorusPixel(
    uint8_t strip,
    uint16_t pixel,
    float showSeconds) const {
  (void)strip;
  const PathCoordinate path = geometry_.pathCoordinate(pixel);
  const float root = fireRootCoordinate(path);

  // The collective state uses the same two proven visual grammars as the
  // interactive field, now completely phase-locked by show time and physical
  // jellyfish position. It is the brightest state, but still has true black
  // valleys: no constant term is added to `structure` or `intensity`.
  const float installationPhase =
      settings_.position.x * 0.31f +
      settings_.position.z * 0.27f +
      settings_.position.y * 0.035f;
  const float speed = 0.68f + settings_.pattern.speed * 1.00f;
  const float waveDarkness = clamp01(
      0.45f + settings_.pattern.contrast * 0.35f);
  const float wavePower =
      1.10f + settings_.pattern.contrast * 1.75f;

  const float pathCrest = deepCrest(
      path.pathT * (11.0f + settings_.pattern.scale * 5.5f) -
      showSeconds * speed +
      installationPhase,
      waveDarkness,
      wavePower);
  const float spreadCrest = deepCrest(
      root * (8.0f + settings_.pattern.scale * 4.5f) +
      showSeconds * speed * 0.78f -
      installationPhase * 0.72f + PI * 0.42f,
      clamp01(waveDarkness + 0.035f),
      wavePower + 0.20f);

  const float structure = max(pathCrest, spreadCrest * 0.94f);
  if (structure <= 0.0f) return {};

  const float intersection = min(pathCrest, spreadCrest);
  const float globalBreath =
      0.88f + 0.18f * (
          0.5f + 0.5f * sinf(showSeconds * 0.39f - installationPhase));
  const float intensity = min(
      1.24f,
      structure * globalBreath + intersection * 0.34f);

  const float hueDrift =
      0.014f * sinf(showSeconds * 0.16f + path.pathT * 3.0f);
  const ColorF primary = hsv(
      settings_.pattern.hue + hueDrift,
      0.78f,
      1.0f);
  const ColorF secondary = hsv(
      settings_.pattern.hue2 - hueDrift,
      0.70f,
      1.0f);

  ColorF color;
  if (path.segment == JellySegment::Inner) {
    color = secondary;
  } else {
    color = lerpColor(
        primary,
        secondary,
        clamp01(0.18f + pathCrest * 0.52f + spreadCrest * 0.30f));
  }

  const ColorF pearl = hsv(
      0.115f + 0.025f * spreadCrest,
      0.16f,
      1.0f);
  color = lerpColor(
      color,
      pearl,
      clamp01(intersection * 0.62f + powf(structure, 4.0f) * 0.14f));
  return scaleColor(color, intensity);
}

void PatternEngine::renderLegacy(float showSeconds, const AudioState& audio) {
  PatternId selected = settings_.fallbackPattern;
  if (selected == PatternId::Demo) {
    static const PatternId demoPatterns[] = {
        PatternId::Waterfall,
        PatternId::InnerSpreadWave,
        PatternId::Sparkle,
        PatternId::Ripple,
        PatternId::TwoToneDiffuse,
    };
    const uint8_t index = static_cast<uint8_t>(floorf(showSeconds / 12.0f)) %
        (sizeof(demoPatterns) / sizeof(demoPatterns[0]));
    selected = demoPatterns[index];
  }

  if (!legacyPatternInitialized_ || selected != activeLegacyPattern_) {
    activeLegacyPattern_ = selected;
    legacyPatternStartedShowSeconds_ = showSeconds;
    legacyPatternInitialized_ = true;
    rippleCycle_ = UINT32_MAX;
  }
  const float patternSeconds = max(0.0f, showSeconds - legacyPatternStartedShowSeconds_);

  const uint16_t count = geometry_.ledCountPerStrip();
  for (uint8_t strip = 0; strip < config::kStripCount; ++strip) {
    for (uint16_t pixel = 0; pixel < count; ++pixel) {
      frame_[strip][pixel] = legacyPixel(selected, strip, pixel, patternSeconds, audio);
    }
  }
}

ColorF PatternEngine::legacyPixel(
    PatternId pattern,
    uint8_t strip,
    uint16_t pixel,
    float showSeconds,
    const AudioState& audio) {
  const PatternParameters& p = settings_.pattern;
  const Vec3 local = geometry_.localPosition(strip, pixel);
  const PathCoordinate path = geometry_.pathCoordinate(pixel);
  const float speed = 0.08f + p.speed * 2.8f;
  const float scale = 0.25f + p.scale * 2.5f;
  const uint32_t ledKey = settings_.deviceId * 65537U + strip * 4099U + pixel;
  const uint32_t columnKey = settings_.deviceId * 65537U + strip * 4099U;

  switch (pattern) {
    case PatternId::Heartbeat: {
      const bool audioFresh = audio.lastPacketLocalMs != 0;
      const float external = audioFresh ? clamp01(audio.level) : 0.0f;
      const float phase = fmodf(showSeconds * (0.45f + p.speed * 1.1f), 1.0f);
      const float pulseA = expf(-powf((phase - 0.08f) / 0.055f, 2.0f));
      const float pulseB = 0.65f * expf(-powf((phase - 0.22f) / 0.075f, 2.0f));
      const float level = max(external, pulseA + pulseB);
      const float radial = expf(-sqrtf(local.x * local.x + local.z * local.z) * 1.2f);
      return hsv(p.hue, 0.90f, level * radial);
    }
    case PatternId::Ripple: {
      // The original local ripple chose a new arbitrary 3-D origin for each
      // long cycle. Preserve that stateful behaviour instead of measuring all
      // waves from the jellyfish origin.
      const float cycleSeconds = lerp(80.0f, 15.0f, p.density);
      const uint32_t cycle = static_cast<uint32_t>(floorf(showSeconds / cycleSeconds));
      const float cycleTime = showSeconds - cycle * cycleSeconds;
      const float centerExtent = 1.0f + p.scale;  // default: original +/-2 m
      if (cycle != rippleCycle_) {
        rippleCycle_ = cycle;
        rippleCenter_ = Vec3(
            lerp(-centerExtent, centerExtent,
                 (esp_random() & 0x00FFFFFFU) / 16777215.0f),
            lerp(-geometry_.maximumDepthMeters(), 0.5f,
                 (esp_random() & 0x00FFFFFFU) / 16777215.0f),
            lerp(-centerExtent, centerExtent,
                 (esp_random() & 0x00FFFFFFU) / 16777215.0f));
        Log.printf(
            "Ripple centre: %.2f, %.2f, %.2f\n",
            rippleCenter_.x, rippleCenter_.y, rippleCenter_.z);
      }
      const float waveVelocity = lerp(2.2f, 9.1f, p.speed);
      const float wavePosition = cycleTime * waveVelocity;
      const float waveX = wavePosition - distance(rippleCenter_, local) * 10.0f - 2.0f;
      float intensity = legacyRippleProfile(waveX);
      // At the default contrast this is effectively the original profile.
      intensity = powf(intensity, 0.45f + p.contrast * 0.85f);
      return scaleColor(hsv(p.hue, 0.92f, 1.0f), intensity);
    }
    case PatternId::FireSpread: {
      // Restores the full ported fireSpread animation: pre-ignition,
      // per-jelly staggering, radial growth, breathing, independent strip
      // pulses, flicker, and the red -> green -> blue gradient evolution.
      const float bellFraction = 0.50f;
      const float tempo = 0.55f + p.speed;  // default speed 0.45 -> 1.0x
      const float time = showSeconds * tempo;
      const float spreadStagger = 2.0f * max(0.35f, 1.45f - p.density);
      const float ignitionTime = settings_.deviceId == 0
          ? 0.0f
          : 8.0f + (settings_.deviceId - 1) * spreadStagger;
      const float jellyElapsed = time - ignitionTime;

      const float globalBreath = 0.12f * (
          0.65f * sinf(time * 0.75f) +
          0.35f * sinf(time * 0.75f * 2.3f));

      float gradientPosition = 0.0f;
      float fromRoot = 0.0f;
      const FireGradientStop* redGradient = kFireBellOuter;
      const FireGradientStop* greenGradient = kFireGreenBellOuter;
      const FireGradientStop* blueGradient = kFireBlueBellOuter;

      if (path.segment == JellySegment::Bell) {
        gradientPosition = path.segmentT * bellFraction;
        fromRoot = path.segmentT * 0.5f;
      } else if (path.segment == JellySegment::Outer) {
        gradientPosition = bellFraction + path.segmentT * (1.0f - bellFraction);
        fromRoot = 0.5f + path.segmentT * 0.5f;
      } else {
        gradientPosition = 1.0f - path.segmentT;
        fromRoot = 1.0f - path.segmentT;
        redGradient = kFireInner;
        greenGradient = kFireGreenInner;
        blueGradient = kFireBlueInner;
      }

      const float fadeWidth = max(0.08f, 0.20f * (1.5f - 0.5f * p.scale));
      ColorF color;
      float intensity = 0.0f;

      if (jellyElapsed <= 0.0f) {
        const float preProgress = clamp01(
            (time - (ignitionTime - 4.0f)) / 4.0f);
        if (preProgress <= 0.0f) return {};
        const float preScale = 0.40f * preProgress * preProgress;
        const float segmentAlpha = clamp01((preScale - fromRoot) / fadeWidth);
        if (segmentAlpha <= 0.0f) return {};
        color = cyclingFireColor(
            redGradient,
            greenGradient,
            blueGradient,
            gradientPosition,
            time,
            settings_.deviceId);
        intensity = preProgress * 0.08f * segmentAlpha;
        return scaleColor(color, intensity);
      }

      const float growProgress = clamp01((jellyElapsed - 2.0f) / 15.0f);
      const float currentScale = 0.40f + 0.60f * growProgress;
      const float energyProgress = clamp01((jellyElapsed - 17.0f) / 20.0f);
      const float stripAmplitude = lerp(0.09f, 0.38f, energyProgress);
      const float shrinkMaximum = lerp(0.30f, 0.62f, energyProgress);
      const float stripPhase = strip * 2.3999f;
      const float stripPulse = stripAmplitude * (
          0.5f * sinf(time * 1.5f + stripPhase) +
          0.3f * sinf(time * 1.5f * 2.1f + stripPhase * 1.6f) +
          0.2f * sinf(time * 1.5f * 3.7f + stripPhase * 0.9f));
      const float floorScale = max(0.40f, currentScale - shrinkMaximum);
      const float effectiveScale = min(
          1.0f, max(floorScale, currentScale + globalBreath + stripPulse));
      const float segmentAlpha = clamp01((effectiveScale - fromRoot) / fadeWidth);
      if (segmentAlpha <= 0.0f) return {};

      const float wobbleAmplitude = 0.10f * (0.5f + 0.75f * p.contrast);
      const float wavePosition = gradientPosition + wobbleAmplitude *
          sinf(gradientPosition * 5.0f - time * 10.5f);
      color = cyclingFireColor(
          redGradient,
          greenGradient,
          blueGradient,
          wavePosition,
          time,
          settings_.deviceId);

      const float kindleProgress = min(1.0f, jellyElapsed / 2.0f);
      const float flickerAmplitude = min(1.0f,
          0.90f * (0.5f + 0.77f * p.contrast)) * kindleProgress;
      const float flicker = 1.0f - flickerAmplitude *
          (0.5f + 0.5f * sinf(time * 3.0f + ledKey * 0.53f));
      const float baseIntensity = 0.08f + 0.92f * kindleProgress;
      intensity = baseIntensity * flicker * segmentAlpha;
      return scaleColor(color, intensity);
    }
    case PatternId::Waterfall: {
      const float cycles = 1.0f + scale * 1.6f;
      float phase = fmodf(showSeconds * speed * 0.22f - path.depthT * cycles, 1.0f);
      if (phase < 0.0f) phase += 1.0f;
      const float width = lerp(0.18f, 0.62f, p.density);
      float bump = 0.0f;
      if (phase < width) bump = 0.5f * (1.0f - cosf(TWO_PI * phase / width));
      return hsv(lerp(p.hue, p.hue2, bump), 0.78f, 0.16f + 0.84f * bump);
    }
    case PatternId::TwoToneDiffuse: {
      const float blend = 0.5f + 0.5f * sinf(
          local.y * scale + strip * 0.55f - showSeconds * speed * 0.45f);
      return hsv(lerp(p.hue, p.hue2, blend), 0.72f, 0.28f + 0.58f * blend);
    }
    case PatternId::ColorWheel: {
      const float hue = strip / 8.0f + path.pathT * 0.35f + showSeconds * speed * 0.08f;
      return hsv(hue, 1.0f, 0.78f);
    }
    case PatternId::BottomFill: {
      const float level = 0.5f + 0.5f * sinf(showSeconds * speed * 0.35f);
      const float on = smoothstep(level - 0.08f, level + 0.02f, path.depthT);
      return hsv(p.hue, 0.78f, on * 0.88f);
    }
    case PatternId::SensorDemo: {
      const FieldState field = interaction_.sampleField(
          geometry_.worldPosition(strip, pixel), settings_.interaction);
      const float hue = lerp(0.55f, 0.98f, field.agitation);
      return hsv(hue, 0.86f, 0.10f + field.brightness * 0.90f);
    }
    case PatternId::FallingRain: {
      const int drops = 2 + static_cast<int>(p.density * 9.0f);
      const float trail = 0.03f + 0.14f * p.scale;
      float intensity = 0.0f;
      for (int drop = 0; drop < drops; ++drop) {
        const float seed = hash01(columnKey + drop * 7919U);
        const float front = fmodf(seed + showSeconds * speed * (0.07f + seed * 0.05f), 1.0f);
        const float diff = front - path.depthT;
        if (diff >= 0.0f && diff < trail) intensity = max(intensity, 1.0f - diff / trail);
      }
      return hsv(p.hue, 0.72f, intensity);
    }
    case PatternId::MovementSimulation: {
      // A complete no-hardware demonstration of the global interaction arc:
      // arrival/low calm -> jostling -> long calm build -> high calm -> a
      // sudden disappointing disturbance. This is intentionally a test mode,
      // not a separate artistic fallback pattern.
      const float cycle = fmodf(showSeconds, 52.0f);
      FieldState simulated;
      simulated.presence = 0.86f;

      if (cycle < 8.0f) {
        simulated.calmness = 0.05f;
      } else if (cycle < 16.0f) {
        simulated.agitation =
            0.72f + 0.28f * (0.5f + 0.5f * sinf(cycle * 3.7f));
        simulated.calmness = 0.04f;
      } else if (cycle < 40.0f) {
        const float settling = smoothstep(16.0f, 40.0f, cycle);
        simulated.calmness = settling;
        simulated.agitation = 0.07f * (1.0f - settling);
      } else if (cycle < 44.0f) {
        simulated.calmness = 1.0f;
      } else if (cycle < 46.0f) {
        // Keep accumulated calmness high for the first instant so the direct
        // agitation penalty can be seen independently of calmness decay.
        simulated.calmness = 1.0f;
        simulated.agitation = smoothstep(44.0f, 44.45f, cycle);
      } else {
        const float release = smoothstep(46.0f, 52.0f, cycle);
        simulated.calmness = 1.0f - release;
        simulated.agitation = 1.0f - release * 0.78f;
      }

      simulated.harmony =
          simulated.presence * simulated.calmness;
      simulated.synchronization =
          simulated.presence * powf(simulated.calmness, 1.55f);
      simulated.turbulence =
          simulated.presence * simulated.agitation *
          (1.0f - 0.55f * simulated.calmness);
      return forestPixel(strip, pixel, showSeconds, simulated, 0.0f);
    }
    case PatternId::InnerSpreadWave: {
      // The folded inner path carries a narrow secondary-colour crest. The
      // bell and outer branches stay fully lit in the primary colour between
      // crests, then crossfade through a genuine colour mixture into the
      // secondary colour as the wave passes. This matches the original
      // red-on-blue construction; the previous shoulder-only term peaked at
      // just 25%, making the primary colour nearly disappear.
      const float wormFrequency = 11.0f + p.scale * 6.0f;  // default: 17
      const float waveSpeed = 0.45f + p.speed * 1.67f;     // default: 1.2
      const float jellyPhase = 0.25f + p.density * 1.22f; // default: 0.8
      const float wave = max(0.0f, sinf(
          path.pathT * wormFrequency -
          showSeconds * waveSpeed +
          settings_.deviceId * jellyPhase));

      const ColorF secondary = hsv(p.hue2, 0.96f, 1.0f);
      if (path.segment == JellySegment::Inner) {
        const float crestPower = 1.35f + p.contrast;  // default: 2.0
        return scaleColor(secondary, powf(wave, crestPower));
      }

      const ColorF primary = hsv(p.hue, 0.94f, 1.0f);
      // Default contrast gives an almost exactly linear crossfade, matching
      // the original `primary * (1-wave) + secondary * wave` behaviour.
      const float mixPower = 0.65f + p.contrast * 0.54f;
      const float colorMix = powf(wave, mixPower);
      return lerpColor(primary, secondary, colorMix);
    }
    case PatternId::RainbowWave: {
      const float hue = path.pathT * scale + strip / 8.0f + showSeconds * speed * 0.08f;
      const float wave = 0.45f + 0.55f * sinf(path.pathT * 9.0f - showSeconds * speed);
      return hsv(hue, 1.0f, max(0.08f, wave));
    }
    case PatternId::Crazy: {
      const float noise = 0.5f + 0.5f * sinf(
          local.x * 8.0f + local.y * 5.0f + strip * 2.2f + showSeconds * (3.0f + speed * 3.0f));
      return hsv(noise + showSeconds * 0.11f, 1.0f, 0.35f + 0.65f * noise);
    }
    case PatternId::Sparkle: {
      const float rate = 1.0f + speed * 4.0f;
      const uint32_t bucket = static_cast<uint32_t>(floorf(showSeconds * rate));
      const float phase = fmodf(showSeconds * rate, 1.0f);
      float intensity = 0.0f;
      for (uint8_t lookback = 0; lookback < 3; ++lookback) {
        const uint32_t eventBucket = bucket >= lookback ? bucket - lookback : 0;
        const float chance = hash01(ledKey ^ (eventBucket * 2654435761U));
        if (chance > 1.0f - p.density * 0.12f) {
          const float age = phase + lookback;
          intensity = max(intensity, expf(-age * (2.0f + 5.0f * p.contrast)));
        }
      }
      return white(intensity);
    }
    case PatternId::None:
      return {};
    case PatternId::Demo:
      return {};
  }
  return {};
}

void PatternEngine::renderBlackout() {
  // clearFrame() already supplied the frame.
}

void PatternEngine::startActivation(const ActivationEvent& event, uint64_t showNowMs) {
  if (event.eventId == 0) return;
  for (const auto& slot : ripples_) {
    if (slot.event.eventId == event.eventId) return;
  }

  RippleSlot* target = nullptr;
  uint64_t oldest = UINT64_MAX;
  for (auto& slot : ripples_) {
    if (!slot.active) {
      target = &slot;
      break;
    }
    if (slot.event.startShowTimeMs < oldest) {
      oldest = slot.event.startShowTimeMs;
      target = &slot;
    }
  }

  target->active = true;
  target->event = event;
  if (target->event.startShowTimeMs == 0) target->event.startShowTimeMs = showNowMs;
}

void PatternEngine::overlayActivationWaves(uint64_t showNowMs) {
  const uint16_t count = geometry_.ledCountPerStrip();
  const float duration = max(0.5f, settings_.interaction.activationWaveDurationSeconds);
  const float speed = max(0.1f, settings_.interaction.activationWaveSpeedMetersPerSecond);
  const float width = max(0.08f, settings_.interaction.activationWaveWidthMeters);

  for (auto& slot : ripples_) {
    if (!slot.active) continue;
    const int64_t elapsedMs = static_cast<int64_t>(showNowMs) -
        static_cast<int64_t>(slot.event.startShowTimeMs);
    if (elapsedMs < -250) continue;
    const float elapsed = max(0.0f, elapsedMs / 1000.0f);
    if (elapsed > duration) {
      slot.active = false;
      continue;
    }

    const float radius = elapsed * speed;
    const float fade = smoothstep(duration, duration * 0.45f, elapsed);
    const ColorF waveColor = lerpColor(
        hsv(settings_.pattern.hue, 0.35f, 1.0f),
        white(1.0f),
        0.55f);

    for (uint8_t strip = 0; strip < config::kStripCount; ++strip) {
      for (uint16_t pixel = 0; pixel < count; ++pixel) {
        const float shellDistance = fabsf(
            distance(geometry_.worldPosition(strip, pixel), slot.event.origin) - radius);
        const float intensity = expf(-(shellDistance * shellDistance) / (2.0f * width * width)) * fade;
        if (intensity < 0.002f) continue;
        frame_[strip][pixel] = addColor(
            frame_[strip][pixel],
            scaleColor(waveColor, intensity * 0.95f));
      }
    }
  }
}

void PatternEngine::identify(uint64_t localNowMs) {
  identifyUntilLocalMs_ = localNowMs + 3200ULL;
}

void PatternEngine::applyIdentifyOverlay(uint64_t localNowMs) {
  if (localNowMs >= identifyUntilLocalMs_) return;
  const bool on = ((identifyUntilLocalMs_ - localNowMs) / 260ULL) % 2ULL == 0ULL;
  const uint16_t count = geometry_.ledCountPerStrip();
  for (uint8_t strip = 0; strip < config::kStripCount; ++strip) {
    for (uint16_t pixel = 0; pixel < count; ++pixel) {
      frame_[strip][pixel] = on ? white(1.0f) : ColorF{};
    }
  }
}

uint8_t PatternEngine::gammaByte(float value) const {
  value = clamp01(value);
  return static_cast<uint8_t>(roundf(powf(value, 2.2f) * 255.0f));
}


void PatternEngine::updateMusicBrightness(
    uint64_t localNowMs,
    const AudioState& audio) {
  float target = 1.0f;

  const bool musicActive =
      audio.lastPacketLocalMs != 0 &&
      localNowMs >= audio.lastPacketLocalMs &&
      localNowMs - audio.lastPacketLocalMs <= kMusicPacketTimeoutMs;

  if (musicActive) {
    target = lerp(
        kMusicMinimumBrightness,
        kMusicMaximumBrightness,
        clamp01(audio.level));
  }

  float deltaSeconds = config::kRenderIntervalMs / 1000.0f;
  if (lastMusicBrightnessUpdateLocalMs_ != 0 &&
      localNowMs >= lastMusicBrightnessUpdateLocalMs_) {
    deltaSeconds = min(
        0.25f,
        (localNowMs - lastMusicBrightnessUpdateLocalMs_) / 1000.0f);
  }
  lastMusicBrightnessUpdateLocalMs_ = localNowMs;

  const float responseSeconds =
      target > musicBrightness_
          ? kMusicAttackSeconds
          : kMusicReleaseSeconds;
  const float response = responseSeconds <= 0.0f
      ? 1.0f
      : 1.0f - expf(-deltaSeconds / responseSeconds);

  musicBrightness_ += (target - musicBrightness_) * response;
  musicBrightness_ = clamp01(musicBrightness_);
}

void PatternEngine::convertAndShow(uint64_t localNowMs) {
  const uint16_t activeCount = geometry_.ledCountPerStrip();
  const float brightness = clamp01(settings_.pattern.brightness) *
      clamp01(settings_.masterBrightness) *
      musicBrightness_;
  float estimatedMilliAmps = 0.0f;
  float estimatedPreviousMilliAmps = 0.0f;

  for (uint8_t strip = 0; strip < config::kStripCount; ++strip) {
    for (uint16_t pixel = 0; pixel < config::kMaxLedsPerStrip; ++pixel) {
      const ColorF source = pixel < activeCount
          ? clampColor(scaleColor(frame_[strip][pixel], brightness))
          : ColorF{};
      output_[strip][pixel] = RgbColor(
          gammaByte(source.r), gammaByte(source.g), gammaByte(source.b));
      estimatedMilliAmps +=
          (output_[strip][pixel].R + output_[strip][pixel].G + output_[strip][pixel].B) /
          255.0f * 20.0f;
      const RgbColor previous = transitionFrom_[strip][pixel];
      estimatedPreviousMilliAmps +=
          (previous.R + previous.G + previous.B) / 255.0f * 20.0f;
    }
  }

  float transition = 1.0f;
  if (transitionStartedLocalMs_ != 0) {
    transition = smoothstep(
        0.0f,
        kSceneTransitionSeconds,
        (localNowMs - transitionStartedLocalMs_) / 1000.0f);
    if (transition >= 1.0f) transitionStartedLocalMs_ = 0;
  }

  // All channels use the same scene-transition fraction, so total current is
  // the same linear blend of the previous and target estimates. Limiting that
  // blended estimate keeps the transition safe without an unnecessary dip at
  // its first frame.
  const float transitionEstimate = lerp(
      estimatedPreviousMilliAmps, estimatedMilliAmps, transition);
  const float currentScale = transitionEstimate > settings_.powerLimitMilliAmps
      ? settings_.powerLimitMilliAmps / max(1.0f, transitionEstimate)
      : 1.0f;

  for (uint8_t strip = 0; strip < config::kStripCount; ++strip) {
    for (uint16_t pixel = 0; pixel < config::kMaxLedsPerStrip; ++pixel) {
      const RgbColor target = output_[strip][pixel];
      const RgbColor from = transitionFrom_[strip][pixel];
      const uint8_t red = static_cast<uint8_t>(
          lerp(from.R, target.R, transition) * currentScale);
      const uint8_t green = static_cast<uint8_t>(
          lerp(from.G, target.G, transition) * currentScale);
      const uint8_t blue = static_cast<uint8_t>(
          lerp(from.B, target.B, transition) * currentScale);
      gStrips[strip].SetPixelColor(pixel, RgbColor(red, green, blue));
    }
    gStrips[strip].Show();
  }
}

}  // namespace jelly
