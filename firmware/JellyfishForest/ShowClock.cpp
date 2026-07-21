#include "ShowClock.h"
#include <esp_timer.h>
#include <math.h>

namespace jelly {

uint64_t monotonicMillis() {
  return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
}

uint64_t ShowClock::now(uint64_t localNowMs) const {
  if (!hasSync_) return localNowMs;
  const double value = static_cast<double>(localNowMs) + offsetMs_;
  return value < 0.0 ? 0ULL : static_cast<uint64_t>(value);
}

void ShowClock::ingest(uint64_t remoteShowTimeMs, uint64_t receivedLocalMs, uint8_t priority) {
  const bool currentExpired = hasSync_ && receivedLocalMs - lastSyncLocalMs_ > 2500ULL;
  if (hasSync_ && !currentExpired && priority < priority_) return;

  const double target = static_cast<double>(remoteShowTimeMs) - static_cast<double>(receivedLocalMs);
  if (!hasSync_ || currentExpired || priority > priority_ || fabs(target - offsetMs_) > 500.0) {
    offsetMs_ = target;
  } else {
    offsetMs_ += (target - offsetMs_) * 0.18;
  }

  hasSync_ = true;
  priority_ = priority;
  lastSyncLocalMs_ = receivedLocalMs;
}

bool ShowClock::synchronized(uint64_t localNowMs) const {
  return hasSync_ && localNowMs - lastSyncLocalMs_ <= 2500ULL;
}

}  // namespace jelly
