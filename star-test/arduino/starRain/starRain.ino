#include <FastLED.h>

#define NUM_LEDS   50
#define DATA_PIN   4
#define BRIGHTNESS 150

CRGB leds[NUM_LEDS];

// ── Tuning ────────────────────────────────────────────────────
const float SPEED        = 0.3f;  // how fast drops travel
const float SPEED_VAR    = 0.6f;  // speed variance between drops
const int   NUM_DROPS    = 4;     // simultaneous drops on the strip
const float DROP_SPACING = 0.25f; // initial spread between drops
const float TRAIL_LENGTH = 0.10f; // length of each drop's tail (0→1)
// ──────────────────────────────────────────────────────────────

void setup() {
  FastLED.addLeds<WS2811, DATA_PIN, BRG>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
}

void loop() {
  float time = millis() / 1000.0f;

  for (int i = 0; i < NUM_LEDS; i++) {
    float tPerim    = i / (float)NUM_LEDS;  // 0→1 around the star perimeter
    float intensity = 0.0f;

    for (int d = 0; d < NUM_DROPS; d++) {
      float dropSeed   = fmod(d * 0.618033f, 1.0f);
      float dropSpeed  = SPEED * (1.0f - SPEED_VAR / 2.0f + dropSeed * SPEED_VAR);
      float dropOffset = fmod(dropSeed + d * DROP_SPACING, 1.0f);
      float dropFront  = fmod(dropOffset + time * dropSpeed, 1.0f);

      float diff = dropFront - tPerim;
      if (diff > 0.0f && diff < TRAIL_LENGTH) {
        float frac = 1.0f - diff / TRAIL_LENGTH;  // 1 at head, 0 at tail
        if (frac > intensity) intensity = frac;
      }
    }

    // Small high-freq flicker per LED
    float flicker       = 0.85f + 0.15f * sin(time * 60.0f + i * 7.3f);
    float finalIntensity = intensity * flicker;

    // Blue: R=0, G=dim, B=full
    leds[i] = CRGB(
      0,
      (uint8_t)(38  * finalIntensity),
      (uint8_t)(255 * finalIntensity)
    );
  }

  FastLED.show();
  delay(16);  // ~60 fps
}
