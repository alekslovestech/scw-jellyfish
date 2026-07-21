#include "InteractionEngine.h"
#include <math.h>
#include "MathUtils.h"

namespace jelly {

PlatformState* InteractionEngine::slotFor(uint8_t id) {
  if (id == 0) return nullptr;
  PlatformState* empty = nullptr;
  PlatformState* oldest = &platforms_[0];
  for (auto& platform : platforms_) {
    if (platform.valid && platform.id == id) return &platform;
    if (!platform.valid && empty == nullptr) empty = &platform;
    if (platform.lastSeenLocalMs < oldest->lastSeenLocalMs) oldest = &platform;
  }
  return empty != nullptr ? empty : oldest;
}

void InteractionEngine::setLocalPlatform(const PlatformState& state, uint64_t localNowMs) {
  localPlatform_ = state;
  localPlatform_.lastSeenLocalMs = localNowMs;
  if (state.valid && state.id != 0) {
    PlatformState* slot = slotFor(state.id);
    if (slot != nullptr) *slot = localPlatform_;
  }
}

void InteractionEngine::ingestPlatform(const PlatformState& state, uint64_t localNowMs) {
  if (!state.valid || state.id == 0) return;
  PlatformState* slot = slotFor(state.id);
  if (slot == nullptr) return;

  // A reboot may reset the sequence, so only reject an older packet while the
  // current peer record is fresh.
  if (slot->valid && slot->id == state.id &&
      localNowMs - slot->lastSeenLocalMs < 1000ULL &&
      state.sequence < slot->sequence) {
    return;
  }

  *slot = state;
  slot->lastSeenLocalMs = localNowMs;
  slot->valid = true;
}

void InteractionEngine::expire(uint64_t localNowMs) {
  for (auto& platform : platforms_) {
    if (platform.valid && localNowMs - platform.lastSeenLocalMs > config::kPeerTimeoutMs) {
      platform.valid = false;
    }
  }
}

void InteractionEngine::tick(
    uint64_t localNowMs,
    uint64_t showNowMs,
    const InteractionTuning& tuning) {
  expire(localNowMs);
  updateInstallation(localNowMs, showNowMs, tuning);
  lastTickLocalMs_ = localNowMs;
}

void InteractionEngine::updateInstallation(
    uint64_t localNowMs,
    uint64_t showNowMs,
    const InteractionTuning& tuning) {
  InstallationState next = localInstallation_;
  next.seenPlatforms = 0;
  next.occupiedPlatforms = 0;
  next.meanCalmness = 0.0f;
  next.minimumCalmness = 1.0f;
  next.maximumAgitation = 0.0f;

  for (const auto& platform : platforms_) {
    if (!platform.valid) continue;
    ++next.seenPlatforms;
    if (!platform.occupied) continue;
    ++next.occupiedPlatforms;
    next.meanCalmness += platform.calmness;
    next.minimumCalmness = min(next.minimumCalmness, platform.calmness);
    next.maximumAgitation = max(next.maximumAgitation, platform.agitation);
  }

  if (next.occupiedPlatforms > 0) {
    next.meanCalmness /= next.occupiedPlatforms;
  } else {
    next.minimumCalmness = 0.0f;
  }

  const uint8_t expected = tuning.expectedPlatformCount < 1U
      ? 1U
      : tuning.expectedPlatformCount;
  const bool everyVisiblePlatformOccupied =
      next.seenPlatforms > 0 &&
      next.occupiedPlatforms == next.seenPlatforms;
  const bool candidate =
      next.seenPlatforms >= expected &&
      everyVisiblePlatformOccupied &&
      next.minimumCalmness >= tuning.calmThreshold &&
      next.maximumAgitation <= tuning.maxAgitationForChorus;

  if (candidate) {
    if (calmCandidateSinceLocalMs_ == 0) calmCandidateSinceLocalMs_ = localNowMs;
    if (!next.allCalmLatched &&
        localNowMs - calmCandidateSinceLocalMs_ >=
            static_cast<uint64_t>(tuning.allCalmHoldSeconds * 1000.0f)) {
      next.allCalmLatched = true;
      next.chorusStartedShowTimeMs = showNowMs;
    }
  } else {
    calmCandidateSinceLocalMs_ = 0;
    const bool release =
        next.occupiedPlatforms < expected ||
        next.occupiedPlatforms < next.seenPlatforms ||
        next.minimumCalmness < tuning.calmReleaseThreshold ||
        next.maximumAgitation > min(1.0f, tuning.maxAgitationForChorus * 1.8f);
    if (release) next.allCalmLatched = false;
  }

  const float dtSeconds = lastTickLocalMs_ == 0
      ? 0.02f
      : min(0.5f, (localNowMs - lastTickLocalMs_) / 1000.0f);
  const float chorusTarget = next.allCalmLatched ? 1.0f : 0.0f;
  const float fadeSeconds = next.allCalmLatched
      ? max(0.1f, tuning.chorusFadeSeconds)
      : max(0.1f, tuning.chorusFadeSeconds * 0.55f);
  next.chorus += (chorusTarget - next.chorus) * exponentialAlpha(dtSeconds, fadeSeconds);
  next.chorus = clamp01(next.chorus);
  next.sequence = ++installationSequence_;
  localInstallation_ = next;
}

FieldState InteractionEngine::sampleField(
    const Vec3& worldPosition,
    const InteractionTuning& tuning) const {
  FieldState field;
  const float radius = max(0.25f, tuning.influenceRadiusMeters);
  const float denominator = 2.0f * radius * radius;
  float influenceSum = 0.0f;
  float remainingDarkness = 1.0f;

  for (const auto& platform : platforms_) {
    if (!platform.valid || !platform.occupied) continue;
    const float influence = expf(-distanceSquared(worldPosition, platform.position) / denominator);
    if (influence < 0.002f) continue;
    influenceSum += influence;
    field.agitation += influence * platform.agitation;
    field.calmness += influence * platform.calmness;
    remainingDarkness *= (1.0f - clamp01(influence * 0.86f));
  }

  field.presence = clamp01(1.0f - remainingDarkness);
  if (influenceSum > 0.0001f) {
    field.agitation = clamp01(field.agitation / influenceSum);
    field.calmness = clamp01(field.calmness / influenceSum);
  }

  field.harmony = field.presence * field.calmness;
  field.synchronization = field.presence * powf(field.calmness, 1.55f);
  field.turbulence = field.presence * field.agitation * (1.0f - 0.55f * field.calmness);

  // Occupancy itself establishes a bright local field. Agitation changes the
  // motion and adds energy; calmness changes harmony and coherence without
  // taking the occupied brightness away.
  field.brightness = clamp01(
      field.presence * (0.54f + 0.12f * field.calmness) +
      field.agitation * field.presence * 0.30f);
  return field;
}

const PlatformState* InteractionEngine::platformById(uint8_t id) const {
  for (const auto& platform : platforms_) {
    if (platform.valid && platform.id == id) return &platform;
  }
  return nullptr;
}

bool InteractionEngine::canonicalActive(uint64_t localNowMs) const {
  return canonicalReceivedLocalMs_ != 0 &&
      localNowMs - canonicalReceivedLocalMs_ <= config::kCanonicalTimeoutMs;
}

const InstallationState& InteractionEngine::installation(uint64_t localNowMs) const {
  return canonicalActive(localNowMs) ? canonicalInstallation_ : localInstallation_;
}

void InteractionEngine::applyCanonicalInstallation(
    const InstallationState& state,
    uint64_t localNowMs,
    uint8_t priority) {
  const bool active = canonicalActive(localNowMs);
  if (active && priority < canonicalPriority_) return;
  canonicalInstallation_ = state;
  canonicalReceivedLocalMs_ = localNowMs;
  canonicalPriority_ = priority;
}

}  // namespace jelly
