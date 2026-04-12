int _ledFrame = 0;
unsigned long _lastLedTime = 0;

void runIdentifySequence() {
  for (int k = 0; k < 5; k++) {
    ws2812b.fill(ws2812b.Color(180, 180, 180));
    ws2812b.show();
    delay(180);
    ws2812b.clear();
    ws2812b.show();
    delay(180);
  }
}

void drawFrame() {
  for (int p = 0; p < NUM_PIXELS; p++) {
    float val   = float(max(double(0), sin(p * 0.832 - _ledFrame * 0.03) * 10));
    int   value = int(val * val);
    ws2812b.setPixelColor(p, ws2812b.Color(0, value, value));
  }
  ws2812b.show();
  _ledFrame++;
}

void ledsTick() {
  if (millis() - _lastLedTime > 30) {
    _lastLedTime = millis();
    drawFrame();
  }
}
