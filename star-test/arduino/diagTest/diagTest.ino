#include <FastLED.h>

#define NUM_LEDS      50
#define DATA_PIN      4
#define BRIGHTNESS    150
#define LEDS_PER_EDGE 5

CRGB leds[NUM_LEDS];

// ── Stage timing (matches diagTest.ts) ────────────────────────
// Stage 1: 6 colors × 1.5s = 9s
// Stage 2: 5s  — heat ramp
// Stage 3: 5s  — tipness geometry
const float S1 = 9.0f;
const float S2 = 5.0f;
const float S3 = 5.0f;
const float TOTAL = S1 + S2 + S3;  // 19s, then loops
// ──────────────────────────────────────────────────────────────

// Stage 1 colors: red, green, blue, yellow, white, orange
const CRGB STAGE1_COLORS[] = {
  CRGB(255,   0,   0),
  CRGB(  0, 255,   0),
  CRGB(  0,   0, 255),
  CRGB(255, 255,   0),
  CRGB(255, 255, 255),
  CRGB(255, 115,   0),
};

// Matches fireColor() in starFire.ts / diagTest.ts exactly
CRGB fireColor(float heat) {
  if (heat < 0.3f) {
    float f = heat / 0.3f;
    return CRGB((uint8_t)(f * 0.9f * 255), 0, 0);
  } else if (heat < 0.6f) {
    float f = (heat - 0.3f) / 0.3f;
    return CRGB((uint8_t)((0.9f + f * 0.1f) * 255), (uint8_t)(f * 0.45f * 255), 0);
  } else if (heat < 0.85f) {
    float f = (heat - 0.6f) / 0.25f;
    return CRGB(255, (uint8_t)((0.45f + f * 0.45f) * 255), (uint8_t)(f * 0.1f * 255));
  } else {
    float f = (heat - 0.85f) / 0.15f;
    return CRGB(255, (uint8_t)((0.9f + f * 0.1f) * 255), (uint8_t)((0.1f + f * 0.5f) * 255));
  }
}

// Matches getLEDDescriptor tipness in diagTest.ts:
//   even edges start at outer tip  → tipness = 1 - t
//   odd  edges start at inner corner → tipness = t
float tipness(int i) {
  int   edge = i / LEDS_PER_EDGE;
  float t    = (i % LEDS_PER_EDGE) / (float)LEDS_PER_EDGE;
  return (edge % 2 == 0) ? 1.0f - t : t;
}

void setup() {
  FastLED.addLeds<WS2811, DATA_PIN, BRG>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
}

void loop() {
  float time = fmod(millis() / 1000.0f, TOTAL);

  if (time < S1) {
    // ── Stage 1: solid color sweep ──────────────────────────
    int colorIdx = (int)(time / 1.5f) % 6;
    fill_solid(leds, NUM_LEDS, STAGE1_COLORS[colorIdx]);

  } else if (time < S1 + S2) {
    // ── Stage 2: heat ramp LED 0→49 maps to heat 0→1 ────────
    for (int i = 0; i < NUM_LEDS; i++) {
      float heat = i / (float)(NUM_LEDS - 1);
      leds[i] = fireColor(heat);
    }

  } else {
    // ── Stage 3: tipness geometry (bright=tip, dark=corner) ──
    for (int i = 0; i < NUM_LEDS; i++) {
      float t = tipness(i);
      leds[i] = CRGB((uint8_t)(255 * t), (uint8_t)(255 * t), (uint8_t)(255 * t));
    }
  }

  FastLED.show();
  delay(16);
}
