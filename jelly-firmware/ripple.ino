#include "config.h"
#include "color_utils.h"

// Three lobes between x = -2 and x = 2.
// Returns an intensity from 0.0 to 1.0.
float ripple(float x) {
  if (x <= -2.0f || x >= 2.0f) {
    return 0.0f;
  }

  const float x2 = x * x;
  const float a = x2 - 4.0f;
  const float b = x2 - 1.0f;

  // Original maximum is 256 at x = 0.
  return (a * a * b * b) / 16.0f;
}

// Expanding wavefront radiating outward from a 3-D point.
void rippleOutFromPoint(const Pos3D& startPoint, int frame, RgbColor rgb) {
  const float wavePosition = frame * 0.0531f;

  for (int s = 0; s < 8; s++) {
    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
      const float distance = distanceFast(startPoint, ledPos[s][p]);

      const float waveX =
          wavePosition
          - distance * 10.0f
          - 2.0f;

      const float intensity = ripple(waveX);

      strips[s].SetPixelColor(
          p,
          rgbWithIntensity(rgb, intensity)
      );
    }
  }
}

// Saturating additive blend: the overlay can brighten the existing pattern,
// but channel values never wrap around past 255.
RgbColor addRgbSaturated(const RgbColor& base, const RgbColor& overlay) {
  return RgbColor(
    min(255, (int)base.R + (int)overlay.R),
    min(255, (int)base.G + (int)overlay.G),
    min(255, (int)base.B + (int)overlay.B)
  );
}

bool hasSeenGlobalRipple(uint32_t eventId) {
  for (uint8_t i = 0; i < MAX_GLOBAL_RIPPLES; i++) {
    if (globalRipples[i].eventId == eventId) return true;
  }
  return false;
}

void startGlobalRipple(uint32_t eventId,
                       const Pos3D& origin,
                       RgbColor color,
                       float speed,
                       unsigned long durationMs) {
  if (eventId == 0 || hasSeenGlobalRipple(eventId)) return;

  const unsigned long now = millis();
  int target = -1;
  unsigned long oldestAge = 0;

  for (uint8_t i = 0; i < MAX_GLOBAL_RIPPLES; i++) {
    if (!globalRipples[i].active) {
      target = i;
      break;
    }

    const unsigned long age = now - globalRipples[i].startMs;
    if (target < 0 || age > oldestAge) {
      target = i;
      oldestAge = age;
    }
  }

  GlobalRipple& wave = globalRipples[target];
  wave.active = true;
  wave.eventId = eventId;
  wave.origin = origin;
  wave.color = color;
  wave.speed = constrain(speed, 0.05f, 20.0f);
  wave.durationMs = constrain(durationMs, 1000UL, 120000UL);
  wave.startMs = now;
}

void overlayGlobalRipples() {
  const unsigned long now = millis();
  const uint16_t pixelCount = activeLedCountPerStrip();

  for (uint8_t waveIndex = 0; waveIndex < MAX_GLOBAL_RIPPLES; waveIndex++) {
    GlobalRipple& wave = globalRipples[waveIndex];
    if (!wave.active) continue;

    const unsigned long elapsedMs = now - wave.startMs;
    if (elapsedMs >= wave.durationMs) {
      wave.active = false;
      continue;
    }

    const float frame = elapsedMs / UPDATE_INTERVAL_MS;
    const float wavePosition = frame * 0.01f * wave.speed;

    for (uint8_t strip = 0; strip < activeStripCount(); strip++) {
      for (uint16_t pixel = 0; pixel < pixelCount; pixel++) {
        const Pos3D absolutePos = getAbsoluteLedPosition(strip, pixel);
        const float distance = distanceFast(wave.origin, absolutePos);
        const float waveX = wavePosition - distance * 10.0f - 2.0f;
        const float intensity = ripple(waveX);
        if (intensity <= 0.0f) continue;

        const RgbColor base = strips[strip].GetPixelColor(pixel);
        const RgbColor light = rgbWithIntensity(wave.color, intensity);
        strips[strip].SetPixelColor(pixel, addRgbSaturated(base, light));
      }
    }
  }
}
