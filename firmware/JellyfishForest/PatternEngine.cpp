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

ColorF white(float value) {
  return {value, value, value};
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

  const uint16_t count = geometry_.ledCountPerStrip();
  for (uint8_t strip = 0; strip < config::kStripCount; ++strip) {
    for (uint16_t pixel = 0; pixel < count; ++pixel) {
      frame_[strip][pixel] = legacyPixel(selected, strip, pixel, showSeconds, audio);
    }
  }
}

ColorF PatternEngine::legacyPixel(
    PatternId pattern,
    uint8_t strip,
    uint16_t pixel,
    float showSeconds,
    const AudioState& audio) const {
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
      const float radial = sqrtf(local.x * local.x + local.y * local.y + local.z * local.z);
      const float wave = fmodf(showSeconds * speed * 1.7f, 4.0f + scale * 2.0f);
      const float distanceToWave = fabsf(radial - wave);
      const float intensity = expf(-distanceToWave * distanceToWave / 0.05f);
      return hsv(p.hue, 0.82f, intensity);
    }
    case PatternId::FireSpread: {
      const float deviceDelay = settings_.deviceId * (0.25f + (1.0f - p.density) * 1.2f);
      const float elapsed = max(0.0f, showSeconds - deviceDelay);
      const float reach = clamp01(elapsed * speed * 0.08f);
      const float front = smoothstep(path.pathT, min(1.0f, path.pathT + 0.14f), reach);
      const float flicker = 0.52f + 0.48f * sinf(showSeconds * (4.0f + speed * 3.0f) + ledKey * 0.27f);
      const float hue = lerp(0.01f, p.hue2, clamp01(elapsed * 0.008f));
      return hsv(hue, 0.94f, front * (0.42f + 0.58f * flicker));
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
      const float wave = max(0.0f, sinf(path.pathT * (8.0f + scale * 5.0f) - showSeconds * speed));
      const float hue = path.segment == JellySegment::Inner ? p.hue2 : p.hue;
      return hsv(hue, 0.86f, powf(wave, 1.6f));
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
