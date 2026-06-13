#include <math.h>

int _ledFrame = 0;
unsigned long _lastLedTime = 0;
float _weight = 0;
float _lastWeight = 0;
float _agitation = 1;
float _calmness = 0;
float _wavePhase = 0;

void runIdentifySequence() {
  forEachStrip([](StripBus& strip) {
    fillStrip(strip, RgbColor(255,255,255));
    strip.Show();
  });
  delay(400);
  forEachStrip([](StripBus& strip) {
    fillStrip(strip, RgbColor(0,0,0));
    strip.Show();
  });
  delay(400);
  forEachStrip([](StripBus& strip) {
    fillStrip(strip, RgbColor(255,255,255));
    strip.Show();
  });
  delay(400);
  forEachStrip([](StripBus& strip) {
    fillStrip(strip, RgbColor(0,0,0));
    strip.Show();
  });
  delay(400);
  forEachStrip([](StripBus& strip) {
    fillStrip(strip, RgbColor(255,255,255));
    strip.Show();
  });
  delay(400);
  forEachStrip([](StripBus& strip) {
    fillStrip(strip, RgbColor(0,0,0));
    strip.Show();
  });
  delay(400);
}

void drawFrame() {
  //_measurement -= _measurement/10;
  //int read = analogRead(PIN_SENSOR);
  //_measurement += read;
  //Serial.println(read); 
  //Serial.println(_measurement);
  showColorWheelAcrossEightStrips();
  _ledFrame++;
}

void demo() {
  fillStrip(strip0, RgbColor(255, 0, 0));     // red
  fillStrip(strip1, RgbColor(0, 255, 0));     // green
  fillStrip(strip2, RgbColor(0, 0, 255));     // blue
  fillStrip(strip3, RgbColor(255, 255, 0));   // yellow
  fillStrip(strip4, RgbColor(0, 255, 255));   // cyan
  fillStrip(strip5, RgbColor(255, 0, 255));   // magenta
  fillStrip(strip6, RgbColor(255, 128, 0));   // orange
  fillStrip(strip7, RgbColor(128, 0, 255));   // purple

  strip0.Show();
  strip1.Show();
  strip2.Show();
  strip3.Show();
  strip4.Show();
  strip5.Show();
  strip6.Show();
  strip7.Show();
}

void fillStrip(NeoPixelBus<NeoBrgFeature, NeoEsp32LcdX8Ws2812xMethod>& strip, RgbColor c)
{
  for (uint16_t i = 0; i < NUM_LEDS_PER_STRIP; i++) {
    strip.SetPixelColor(i, c);
  }
}

void ledsTick() {
  if (millis() - _lastLedTime > 30) {
    _lastLedTime = millis();
    drawFrame();
  }
}

void showColorWheelAcrossEightStrips()
{
  constexpr uint8_t WHEEL_STRIPS = 8;

  // Slow global drift of the whole wheel
  uint8_t globalShift = millis() / 20;   // smaller = slower, larger = faster

  for (uint8_t s = 0; s < WHEEL_STRIPS; ++s) {
    StripBus& strip = *strips[s];

    // Evenly space the strips around the color wheel
    uint8_t stripOffset = (uint8_t)((s * 256) / WHEEL_STRIPS);

    for (uint16_t i = 0; i < NUM_LEDS_PER_STRIP; ++i) {
      // Spread each strip across the full wheel
      uint8_t ledHue = (uint8_t)((i * 256) / NUM_LEDS_PER_STRIP);

      // Combine strip offset + position + slow animation
      uint8_t hue = ledHue + stripOffset + globalShift;

      strip.SetPixelColor(i, Wheel(hue));
    }

    strip.Show();
  }
}

void weightUpdate() {
    _agitation += abs(_weight - _lastWeight) + 0.01;
    _lastWeight = _weight;
    _agitation *= 0.99;
    if (_calmness > 1000/_agitation) {
        _calmness = 0.95*_calmness + 50/_agitation;
    } else {
        _calmness = 0.999*_calmness + 1/_agitation;
    }
    Serial.print("Weight: ");
    Serial.print(_weight);
    Serial.print("   Agitation: ");
    Serial.print(_agitation);
    Serial.print("   Calmness: ");
    Serial.println(_calmness);
}

// Helper: convert a hue wheel position (0-255) into RGB
RgbColor Wheel(uint8_t pos)
{
  pos = 255 - pos;

  if (pos < 85) {
    return RgbColor(255 - pos * 3, 0, pos * 3);
  }

  if (pos < 170) {
    pos -= 85;
    return RgbColor(0, pos * 3, 255 - pos * 3);
  }

  pos -= 170;
  return RgbColor(pos * 3, 255 - pos * 3, 0);
}

