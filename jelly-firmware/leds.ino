#include <math.h>

int _ledFrame = 0;
unsigned long _lastLedTime = 0;
float _weight = 0;
float _lastWeight = 0;
float _agitation = 1;
float _calmness = 0;
float _wavePhase = 0;

struct Pos3D point;

float distanceSquared(const Pos3D& pos1, const Pos3D& pos2) {
  float dx = pos1.x - pos2.x;
  float dy = pos1.y - pos2.y;
  float dz = pos1.z - pos2.z; 
  return dx*dx + dy*dy + dz*dz;
}

float distanceFast(const Pos3D& pos1, const Pos3D& pos2) {
  float distanceSQ = distanceSquared(pos1, pos2);
  return sqrtf(distanceSQ);
}

//Fundamental fuctions - entry point:
void ledsTick() {
  if (millis() - _lastLedTime > UPDATE_INTERVAL_MS) {
    _lastLedTime = millis();
    drawFrame();
  }
}

void drawFrame() {
  if (isJelly) {
    if (currentPattern == "demo") {
      demo();
    } else if (currentPattern == "ripple") {
      if (_ledFrame%900==0) {
        point.x = random(-200,200)/100.0f;
        point.y = random(-200,50)/100.0f;
        point.z = random(-200,200)/100.0f;
      }
      rippleOutFromPoint(point, _ledFrame%900, RgbColor(255,255,0));
    } else {                 // "heartbeat" (default)
      //heartbeat();
      if (millis() - lastHeartbeatPacketMs < 3000) {
        heartbeat();
      } 
    }
  } else {
    showColorWheelAcrossEightStrips();
  }
  showLEDs();
  if (hasScale && scale.is_ready()) {
    _weight = scale.get_units(1);  
    weightUpdate();
  }
  _ledFrame++;
}

// Flashing identify routine, called directly from firmware on request
void runIdentifySequence() {
  for (int times=0; times<3; times++) {
    for (int i=0; i< NUM_STRIPS; i++) {
      fillStrip(strips[i], RgbColor(MAX_BRIGHTNESS,MAX_BRIGHTNESS,MAX_BRIGHTNESS));
      strips[i].Show();
    };
    delay(400); 
    for (int i=0; i< NUM_STRIPS; i++) {
      fillStrip(strips[i], RgbColor(0,0,0));
      strips[i].Show();
    };
    delay(400); 
  }
}

// High level patters:
void sensorDemo() {
  for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
    for (int s = 0; s<8 ; s++) {
      if (p < _agitation) strips[s].SetPixelColor(p, wheel(_ledFrame%256));
      else strips[s].SetPixelColor(p, RgbColor(0,0,0));
    }
  }
  for (int s = 0; s < NUM_STRIPS; s++) {
    strips[s].Show();
  };
}

// Fill from bottom
void bottomFill() {
  constexpr uint8_t WHEEL_STRIPS = 8;
  for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
    struct Pos2D pos;
    pos = smallJellyFind2Dpos(p); 
    for (int s = 0; s < WHEEL_STRIPS; s++) {
      if (-pos.y*100 > _ledFrame%200) strips[s].SetPixelColor(p, RgbColor(255,0,0));
      else strips[s].SetPixelColor(p, RgbColor(0,0,0));
    };
  };
  for (int s = 0; s < NUM_STRIPS; s++) {
    strips[s].Show();
  };
}

// Rotating rainbow
void rotate()
{
  for (int s = 0; s<8; s++) {
    fillStrip(strips[s], wheel(16*s+_ledFrame));
    strips[s].Show();
  }
}

void showColorWheelAcrossEightStrips()
{
  constexpr uint8_t WHEEL_STRIPS = 8;

  // Slow global drift of the whole wheel
  uint8_t globalShift = millis() / 20;   // smaller = slower, larger = faster

  for (uint8_t s = 0; s < WHEEL_STRIPS; ++s) {
    StripBus& strip = strips[s];

    // Evenly space the strips around the color wheel
    uint8_t stripOffset = (uint8_t)((s * 256) / WHEEL_STRIPS);

    for (uint16_t i = 0; i < NUM_LEDS_PER_STRIP; ++i) {
      uint8_t ledHue = (uint8_t)((i * 256) / NUM_LEDS_PER_STRIP);

      // Combine strip offset + position + slow animation
      uint8_t hue = ledHue + stripOffset + globalShift;

      strip.SetPixelColor(i, wheel(hue));
    }

    strip.Show();
  }
}

// Helpers

void showLEDs() {
  for (int s = 0; s < 8; s++) {
    strips[s].Show();
  }
}

// For load cells
void weightUpdate() {
  _agitation += abs(_weight - _lastWeight) + 0.01;
  _lastWeight = _weight;
  _agitation *= 0.99;
  if (_calmness > 1000/_agitation) {
      _calmness = 0.95*_calmness + 50/_agitation;
  } else {
      _calmness = 0.999*_calmness + 1/_agitation;
  }
  logPrint("Weight: ");
  logPrint(_weight);
  logPrint("   Agitation: ");
  logPrint(_agitation);
  logPrint("   Calmness: ");
  logPrintln(_calmness);
}

// Helper: convert a hue wheel position (0-255) into RGB
RgbColor wheel(uint8_t pos)
{
  pos = pos%256;
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

//Get position in the plane along one strip of a small jellyfish
struct Pos2D smallJellyFind2Dpos(int p) {
  struct Pos2D pos;
  if (p<17) {
    pos.y = -0.1*p;
    pos.x = 0;
  } else if (p<35) {
    pos.y = (p-35)*0.1;
    pos.x = 0.15;
  } else if (p<40) {
    pos.y = 0;
    pos.x = (p-33)*0.1;
  } else {
    pos.y = (40-p)*0.1;
    pos.x = 0.6;
  }
  return pos;
}

//Get full 3D positions along all 8 stips
struct Pos3D smallJellyFind3Dpos(int p, int s) {
  struct Pos3D pos;
  struct Pos2D pos2D = smallJellyFind2Dpos(p);

  pos.y = pos2D.y;
  pos.x = cos(PI*s/4.0)*pos2D.x;
  pos.z = sin(PI*s/4.0)*pos2D.x;

  return pos;
}

// Full color on one strip
void fillStrip(StripBus& strip, RgbColor c) {
  for (uint16_t i = 0; i < NUM_LEDS_PER_STRIP; i++) {
    strip.SetPixelColor(i, c);
  }
}

