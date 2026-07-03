#include <math.h>

int _ledFrame = 0;
unsigned long _lastLedTime = 0;
float _weight = 0;
float _lastWeight = 0;
float _agitation = 1;
float _calmness = 0;
float _wavePhase = 0;

struct Pos3D point;
//struct Pos3D point2;
//struct Pos3D point3;

const uint8_t MAX_BRIGHTNESS = 128; //0-255

//Fundamental fuctions - entry point:
void ledsTick() {
  if (millis() - _lastLedTime > 10) {
    _lastLedTime = millis();
    drawFrame();
  }
  //logPrintln("Ticking, time elapsed: ");
  //logPrint(millis()- _lastLedTime);
}

void drawFrame() {
  //bottomFill();
  /*struct Pos3D start;
  start.x = -1.0f;
  start.y = 0.0f;
  start.z = 0.2f;
  */


  if (isJelly) {
    if (millis() - lastHeartbeatPacketMs < 3000) {
      heartbeat();
    } 
    else {
      if (_ledFrame%900==0) {
        point.x = random(-200,200)/100.0f;
        point.y = random(-200,50)/100.0f;
        point.z = random(-200,200)/100.0f;
      }
      rippleOutFromPoint(point, _ledFrame%900);
    }
  } else {
    showColorWheelAcrossEightStrips();
  }
  /*if (_ledFrame%900==300) {
    point2.x = random(-200,200)/100.0f;
    point2.y = random(-200,50)/100.0f;
    point2.z = random(-200,200)/100.0f;
  }
  if (_ledFrame%900==600) {
    point3.x = random(-200,200)/100.0f;
    point3.y = random(-200,50)/100.0f;
    point3.z = random(-200,200)/100.0f;
  }*/

  //rippleOutFromPoint(point2, (_ledFrame+300)%900);
  //rippleOutFromPoint(point3, (_ledFrame+600)%900);
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

void heartbeat() {
  // Fade out if no heartbeat packets have arrived recently
  if (millis() - lastHeartbeatPacketMs > 1000) {
    heartbeatLevel *= 0.96;
  }

  float brightness = (heartbeatLevel * 255);

  struct Pos3D startPoint;
  startPoint.x = 0.0f;
  startPoint.y = -0.3f;
  startPoint.z = 0.0f;
  float distanceSQ = 0.0f;
  float distance = 0.0f;
  
  for (int s = 0; s < 8; s++) {
    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
      distanceSQ = (startPoint.x-ledPos[s][p].x)*(startPoint.x-ledPos[s][p].x)+(startPoint.y-ledPos[s][p].y)*(startPoint.y-ledPos[s][p].y)+(startPoint.z-ledPos[s][p].z)*(startPoint.z-ledPos[s][p].z);
      distance = sqrtf(distanceSQ);
      strips[s].SetPixelColor(p, RgbColor(max(0.0f,brightness-500*max(distance-0.1f,0.0f)),0,0));
    }
  }

}

void rippleOutFromPoint(struct Pos3D startPoint, int frame) {
  float distanceSQ;
  float distance;
  //int frameSQ = frame*frame;
  for (int s = 0; s < 8; s++) {
    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
      distanceSQ = (startPoint.x-ledPos[s][p].x)*(startPoint.x-ledPos[s][p].x)+(startPoint.y-ledPos[s][p].y)*(startPoint.y-ledPos[s][p].y)+(startPoint.z-ledPos[s][p].z)*(startPoint.z-ledPos[s][p].z);
      distance = sqrtf(distanceSQ);
      strips[s].SetPixelColor(p, RgbColor(0,0,ripple(frame/20.0f-distance*10.0f-2.0f)));
    }
  }
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

// Different colors for different strips
void demo() {
  fillStrip(strips[0], RgbColor(MAX_BRIGHTNESS, 0, 0));     // red
  fillStrip(strips[1], RgbColor(0, MAX_BRIGHTNESS, 0));     // green
  fillStrip(strips[2], RgbColor(0, 0, MAX_BRIGHTNESS));     // blue
  fillStrip(strips[3], RgbColor(MAX_BRIGHTNESS, MAX_BRIGHTNESS, 0));   // yellow
  fillStrip(strips[4], RgbColor(0, MAX_BRIGHTNESS, MAX_BRIGHTNESS));   // cyan
  fillStrip(strips[5], RgbColor(MAX_BRIGHTNESS, 0, MAX_BRIGHTNESS));   // magenta
  fillStrip(strips[6], RgbColor(MAX_BRIGHTNESS, MAX_BRIGHTNESS/2, 0));   // orange
  fillStrip(strips[7], RgbColor(MAX_BRIGHTNESS/2, 0, MAX_BRIGHTNESS));   // purple
  for (int i=0; i<8; i++) {
    strips[i].Show();
  }  
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
      // Spread each strip across the full wheel
      //if (i % 5 != 0)
      //  continue;
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

// Three ripples between x = -2 and x = 2. Center wave the biggest. Value range 0..256 
float ripple(float x) {
    if (x <= -2.0f || x >= 2.0f) return 0.0f;
    float x2 = x * x;
    return 16.0f * (x2 - 4.0f) * (x2 - 4.0f) * (x2 - 1.0f) * (x2 - 1.0f);
}


// Full color on one strip
void fillStrip(StripBus& strip, RgbColor c) {
  for (uint16_t i = 0; i < NUM_LEDS_PER_STRIP; i++) {
    strip.SetPixelColor(i, c);
  }
}

