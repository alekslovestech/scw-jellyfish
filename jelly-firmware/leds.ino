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
  _ledFrame++;
}

void fillStrip(NeoPixelBus<NeoGrbFeature, NeoEsp32LcdX8Ws2812xMethod>& strip, RgbColor c)
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

