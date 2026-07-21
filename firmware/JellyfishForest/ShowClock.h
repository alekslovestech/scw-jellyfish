#pragma once

#include <Arduino.h>

namespace jelly {

uint64_t monotonicMillis();

class ShowClock {
 public:
  uint64_t now(uint64_t localNowMs) const;
  void ingest(uint64_t remoteShowTimeMs, uint64_t receivedLocalMs, uint8_t priority);
  bool synchronized(uint64_t localNowMs) const;
  uint8_t priority() const { return priority_; }
  int64_t offsetMs() const { return static_cast<int64_t>(offsetMs_); }

 private:
  double offsetMs_ = 0.0;
  bool hasSync_ = false;
  uint8_t priority_ = 0;
  uint64_t lastSyncLocalMs_ = 0;
};

}  // namespace jelly
