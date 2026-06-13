#include <FastLED.h>

#define NUM_LEDS      50
#define DATA_PIN      4
#define BRIGHTNESS    150
#define LEDS_PER_EDGE 5
#define NUM_POINTS    5

CRGB leds[NUM_LEDS];

// One colour per star point — matches starPyramid.ts
CRGB POINT_COLORS[NUM_POINTS] = {
  CRGB(255,   0,   0),  // point 0 — red
  CRGB(  0, 255,   0),  // point 1 — green
  CRGB( 51, 128, 255),  // point 2 — blue
  CRGB(255, 255,   0),  // point 3 — yellow
  CRGB(255,   0, 255),  // point 4 — magenta
};

const float FILL_SPEED = 0.4f;  // matches TS

// Matches getLEDDescriptor: starPoint = floor((edge+1)/2) % NUM_POINTS
int getStarPoint(int edge) {
  return ((edge + 1) / 2) % NUM_POINTS;
}

// Distance from outer tip along the arm (0 = closest to tip)
int distFromTip(int edge, int posInEdge) {
  return (edge % 2 == 0) ? posInEdge : (LEDS_PER_EDGE - 1 - posInEdge);
}

void setup() {
  FastLED.addLeds<WS2811, DATA_PIN, BRG>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
}

void loop() {
  float time  = millis() / 1000.0f;
  float maxLevel = LEDS_PER_EDGE - 1;  // 4

  // Triangle wave 0 → maxLevel → 0
  float cycle = fmod(time * FILL_SPEED, 2.0f);
  float fillLevel = (cycle < 1.0f)
    ? cycle * maxLevel
    : (2.0f - cycle) * maxLevel;

  for (int i = 0; i < NUM_LEDS; i++) {
    int edge      = i / LEDS_PER_EDGE;
    int posInEdge = i % LEDS_PER_EDGE;
    int point     = getStarPoint(edge);
    int dist      = distFromTip(edge, posInEdge);

    // Smooth leading edge (matches TS)
    float intensity = fillLevel - dist + 0.5f;
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;

    leds[i] = CRGB(
      (uint8_t)(POINT_COLORS[point].r * intensity),
      (uint8_t)(POINT_COLORS[point].g * intensity),
      (uint8_t)(POINT_COLORS[point].b * intensity)
    );
  }

  FastLED.show();
  delay(16);
}
