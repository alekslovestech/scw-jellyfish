#include <FastLED.h>

#define NUM_LEDS   50
#define DATA_PIN   4
#define BRIGHTNESS 150

CRGB leds[NUM_LEDS];

// 5-pointed star: 10 segments of 5 LEDs, inner-corner↔tip alternating
// tipness = 0 at inner corners, 1 at outer tips
float tipness(int i) {
  float t   = fmod(i / 5.0f, 1.0f);   // position within segment 0→1
  int   seg = (i * 10) / NUM_LEDS;    // segment index 0..9
  return (seg % 2 == 0) ? t : 1.0f - t;
}

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

void setup() {
  FastLED.addLeds<WS2811, DATA_PIN, BRG>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
}

void loop() {
  float time  = millis() / 1000.0f;
  float cycle = 0.5f + 0.5f * sin(time * 1.4f);  // slow breath

  for (int i = 0; i < NUM_LEDS; i++) {
    float tip = tipness(i);

    // Three layered flicker waves (same weights as TS source)
    float flicker =
      0.50f * sin(time * 8.1f  + i * 2.3f) +
      0.30f * sin(time * 15.7f + i * 5.1f) +
      0.20f * sin(time * 27.3f + i * 9.7f);
    float flickerNorm = 0.5f + 0.5f * flicker;  // 0→1

    float heat = tip * 0.55f + flickerNorm * 0.25f + cycle * 0.20f;
    heat = max(0.0f, min(1.0f, heat));

    leds[i] = fireColor(heat);
  }

  FastLED.show();
  delay(16);  // ~60 fps
}
