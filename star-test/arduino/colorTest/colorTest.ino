#include <FastLED.h>

#define NUM_LEDS   50
#define DATA_PIN   4
#define BRIGHTNESS 150

CRGB leds[NUM_LEDS];

CRGB COLORS[] = {
  CRGB(255,   0,   0),   // 1 — should be RED
  CRGB(  0, 255,   0),   // 2 — should be GREEN
  CRGB(  0,   0, 255),   // 3 — should be BLUE
  CRGB(255, 255,   0),   // 4 — should be YELLOW
  CRGB(  0, 255, 255),   // 5 — should be CYAN
  CRGB(255,   0, 255),   // 6 — should be MAGENTA
  CRGB(255, 128,   0),   // 7 — should be ORANGE
  CRGB(255, 255, 255),   // 8 — should be WHITE
};
const int NUM_COLORS = 8;
const int DURATION_MS = 8000;

void setup() {
  FastLED.addLeds<WS2811, DATA_PIN, BRG>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
}

void loop() {
  int colorIdx = (millis() / DURATION_MS) % NUM_COLORS;
  fill_solid(leds, NUM_LEDS, COLORS[colorIdx]);
  FastLED.show();
  delay(16);
}
