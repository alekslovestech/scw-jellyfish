#include "config.h"

// Heartbeat pattern — renders the externally driven `heartbeatLevel` as a red
// pulse radiating from the bell centre. Level is set by incoming UDP packets
// (see wifi.ino); with no packets it fades toward zero.
void heartbeat() {
  // Fade out if no heartbeat packets have arrived recently
  if (millis() - lastHeartbeatPacketMs > 1000) {
    heartbeatLevel *= 0.96;
  }

  float brightness = (heartbeatLevel * 255);

  Pos3D startPoint = {0.0f, -0.3f, 0.0f};
  float distanceSQ = 0.0f;
  float distance = 0.0f;

  for (int s = 0; s < 8; s++) {
    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
      distance = distanceFast(startPoint, ledPos[s][p]);
      strips[s].SetPixelColor(p, RgbColor(max(0.0f,brightness-500*max(distance-0.1f,0.0f)),0,0));
    }
  }
}
