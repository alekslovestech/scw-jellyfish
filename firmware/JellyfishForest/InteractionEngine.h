#pragma once

#include "AppConfig.h"
#include "Types.h"

namespace jelly {

class InteractionEngine {
 public:
  void setLocalPlatform(const PlatformState& state, uint64_t localNowMs);
  void ingestPlatform(const PlatformState& state, uint64_t localNowMs);
  void tick(uint64_t localNowMs, uint64_t showNowMs, const InteractionTuning& tuning);

  FieldState sampleField(const Vec3& worldPosition, const InteractionTuning& tuning) const;
  const PlatformState* platformById(uint8_t id) const;
  const PlatformState& localPlatform() const { return localPlatform_; }
  const InstallationState& localInstallation() const { return localInstallation_; }
  const InstallationState& installation(uint64_t localNowMs) const;

  void applyCanonicalInstallation(
      const InstallationState& state,
      uint64_t localNowMs,
      uint8_t priority);
  bool canonicalActive(uint64_t localNowMs) const;
  uint8_t canonicalPriority() const { return canonicalPriority_; }

 private:
  PlatformState* slotFor(uint8_t id);
  void expire(uint64_t localNowMs);
  void updateInstallation(
      uint64_t localNowMs,
      uint64_t showNowMs,
      const InteractionTuning& tuning);

  PlatformState platforms_[config::kMaxPlatforms] = {};
  PlatformState localPlatform_;
  InstallationState localInstallation_;
  InstallationState canonicalInstallation_;
  uint64_t lastTickLocalMs_ = 0;
  uint64_t calmCandidateSinceLocalMs_ = 0;
  uint64_t canonicalReceivedLocalMs_ = 0;
  uint8_t canonicalPriority_ = 0;
  uint32_t installationSequence_ = 0;
};

}  // namespace jelly
