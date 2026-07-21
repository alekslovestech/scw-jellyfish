#include "PatternEngine.h"

#include <math.h>
#include "LogBuffer.h"
#include "MathUtils.h"

namespace jelly {
namespace {

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
  const Vec3 world = geometry_.worldPosition(strip, pixel);
  const PathCoordinate path = geometry_.pathCoordinate(pixel);
  const float speed = 0.18f + settings_.pattern.speed * 0.42f;
  const float scale = 0.45f + settings_.pattern.scale * 0.70f;

  const float devicePhase =
      settings_.deviceId * 1.371f +
      settings_.position.x * 0.17f +
      settings_.position.z * 0.11f;
  const float synchronizedPhase =
      (1.0f - field.synchronization) * devicePhase;

  const float slowBreath = 0.5f + 0.5f * sinf(showSeconds * speed + synchronizedPhase);
  const float worldWave = 0.5f + 0.5f * sinf(
      world.y * 0.58f * scale +
      world.x * 0.16f +
      world.z * 0.13f -
      showSeconds * speed * 0.73f +
      synchronizedPhase);
  const float bellBreath = 0.5f + 0.5f * sinf(
      path.pathT * 5.4f - showSeconds * speed * 0.52f + synchronizedPhase * 0.6f);

  const float agitation = clamp01(max(field.agitation, simulatedAgitation));
  const float turbulence = clamp01(max(field.turbulence, simulatedAgitation));
  const float chaotic = 0.5f + 0.5f * sinf(
      world.x * (3.2f + 5.0f * agitation) +
      world.z * (2.4f + 4.0f * agitation) +
      path.pathT * 18.0f -
      showSeconds * (1.8f + settings_.pattern.speed * 5.0f) +
      strip * 1.7f);

  const float calmSmooth = lerp(worldWave * 0.55f + bellBreath * 0.45f, slowBreath, field.harmony * 0.70f);
  const float motion = lerp(calmSmooth, chaotic, turbulence);

  const float ambient = 0.13f + 0.13f * motion;
  const float occupiedLight = field.brightness * (0.43f + 0.10f * calmSmooth);
  const float energeticLight = agitation * field.presence * (0.18f + 0.17f * chaotic);
  float intensity = ambient + occupiedLight + energeticLight;
  intensity *= 0.86f + settings_.pattern.contrast * (0.20f + 0.10f * motion);
  intensity = min(1.15f, intensity);

  const float baseHue = settings_.pattern.hue;
  const float secondHue = settings_.pattern.hue2;
  const float hueDrift = 0.025f * sinf(showSeconds * 0.11f + world.y * 0.22f);
  ColorF cool = hsv(lerp(baseHue, secondHue, 0.25f + 0.45f * worldWave) + hueDrift, 0.78f, intensity);

  const ColorF agitated = hsv(
      lerp(0.96f, 0.08f, chaotic),
      0.86f,
      intensity * (0.95f + 0.20f * chaotic));
  ColorF calmPearl = lerpColor(
      hsv(0.49f + 0.05f * worldWave, 0.48f, intensity * 1.08f),
      hsv(0.12f, 0.32f, intensity * 1.05f),
      0.25f + 0.25f * bellBreath);

  ColorF result = lerpColor(cool, agitated, turbulence * 0.82f);
  result = lerpColor(result, calmPearl, field.harmony * 0.78f + chorusAmount * 0.15f);

  const float sparkleThreshold = 1.0f - settings_.pattern.sparkle * (0.05f + field.harmony * 0.18f);
  const uint32_t sparkleKey =
      static_cast<uint32_t>(floorf(showSeconds * 2.0f)) * 4099U +
      settings_.deviceId * 100003U + strip * 257U + pixel;
  if (hash01(sparkleKey) > sparkleThreshold) {
    const float sparklePhase = fmodf(showSeconds * 2.0f, 1.0f);
    result = addColor(result, white((1.0f - sparklePhase) * 0.20f));
  }
  return result;
}

ColorF PatternEngine::chorusPixel(uint8_t strip, uint16_t pixel, float showSeconds) const {
  const Vec3 world = geometry_.worldPosition(strip, pixel);
  const PathCoordinate path = geometry_.pathCoordinate(pixel);
  const float speed = 0.34f + settings_.pattern.speed * 0.36f;

  const float standingA = 0.5f + 0.5f * sinf(
      world.y * 0.72f + world.x * 0.24f - world.z * 0.18f - showSeconds * speed);
  const float standingB = 0.5f + 0.5f * sinf(
      (world.x + world.z) * 0.41f + path.pathT * 3.2f + showSeconds * speed * 0.618f);
  const float pulse = 0.5f + 0.5f * sinf(showSeconds * 0.47f);
  const float intensity = 0.62f + 0.22f * standingA + 0.16f * standingB + 0.08f * pulse;

  const ColorF cyan = hsv(0.49f + 0.045f * standingA, 0.42f, intensity);
  const ColorF pearl = hsv(0.10f + 0.03f * standingB, 0.24f, intensity * 1.06f);
  return lerpColor(cyan, pearl, 0.30f + 0.35f * standingB);
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
      FieldState simulated;
      simulated.presence = 0.85f;
      simulated.agitation = 0.5f + 0.5f * sinf(showSeconds * speed);
      simulated.turbulence = simulated.agitation;
      simulated.brightness = 0.75f;
      return forestPixel(strip, pixel, showSeconds, simulated, 0.0f, simulated.agitation);
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

void PatternEngine::convertAndShow(uint64_t localNowMs) {
  const uint16_t activeCount = geometry_.ledCountPerStrip();
  const float brightness = clamp01(settings_.pattern.brightness) *
      clamp01(settings_.masterBrightness);
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
