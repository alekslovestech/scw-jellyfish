#include <math.h>
#include "config.h"

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

// ── Shared helpers for the ported visualizer animations ─────────────────────
// (fireSpread / waterfall / twoToneDiffuse). FS_* segment consts live in config.h;
// _jellyId / _fireStartTime live in jelly-firmware.ino so they precede fireSpread.ino
// in the sketch's alphabetical tab concatenation.

float fsClamp01(float x) {
  return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

void fsSegmentForPos(int p, int* seg, float* t) {
  if (p < FS_INNER_LEDS) {
    *seg = FS_SEG_INNER; *t = p / (float)(FS_INNER_LEDS - 1);
  } else if (p < FS_INNER_LEDS + FS_BELL_LEDS) {
    *seg = FS_SEG_BELL; *t = (p - FS_INNER_LEDS) / (float)(FS_BELL_LEDS - 1);
  } else {
    *seg = FS_SEG_OUTER; *t = (p - FS_INNER_LEDS - FS_BELL_LEDS) / (float)(FS_OUTER_LEDS - 1);
  }
}
// Smooth 0..1 inner->outer blend, eased across the inner/bell boundary so a
// slightly-off physical tentacle length never shows a hard-cut seam.
float fsInnerOuterMix(int p) {
  const int TRANSITION_HALF = 3;
  float edge0 = FS_INNER_LEDS - TRANSITION_HALF;
  float edge1 = FS_INNER_LEDS + TRANSITION_HALF;
  float x = fsClamp01((p - edge0) / (edge1 - edge0));
  return x * x * (3.0f - 2.0f * x); // smoothstep
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
      if (_ledFrame%5000==0) {
        point.x = random(-200,200)/100.0f;
        point.y = random(-200,50)/100.0f;
        point.z = random(-200,200)/100.0f;
      }
      rippleOutFromPoint(point, _ledFrame%5000, RgbColor(0,0,255));
    } else if (currentPattern == "colorwheel") {
      showColorWheelAcrossEightStrips();
    } else if (currentPattern == "bottomfill") {
      bottomFill();
    } else if (currentPattern == "sensordemo") {
      sensorDemo();
    } else if (currentPattern == "fireSpread") {
      fireSpread();
    } else if (currentPattern == "waterfall") {
      waterfall();
    } else if (currentPattern == "twoToneDiffuse") {
      twoToneDiffuse();
    } else if (currentPattern == "fallingRain") {
      fallingRain();
    } else if (currentPattern == "movementSimulation") {
      movementSimulation();
    } else if (currentPattern == "sparkle") {
      sparkle();
    } else if (currentPattern == "crazy") {
      crazy();
    } else if (currentPattern == "innerSpreadWave") {
      innerSpreadWave();
    } else if (currentPattern == "rainbowWave") {
      rainbowWave();
    } else {                
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

// Rotating rainbow
void rotate()
{
  for (int s = 0; s<8; s++) {
    fillStrip(strips[s], wheel(16*s+_ledFrame));
    strips[s].Show();
  }
}

// Helpers

void showLEDs()
{
    const uint8_t count = activeStripCount();

    for (uint8_t s = 0; s < count; s++) {
        strips[s].Show();
    }
}

const Pos3D& getLedPosition(uint8_t stripIndex, uint16_t pixelIndex)
{
    if (stripIndex < NUM_SHORT_STRIPS) {
        return ledPos[stripIndex][pixelIndex];
    }

    return ledPosLong[stripIndex - NUM_SHORT_STRIPS][pixelIndex];
}

// For load cells
void weightUpdate() {
  _agitation += abs(_weight - _lastWeight) + 0.01; //Agitation rises whenever weight rises or falls
  _lastWeight = _weight;
  _agitation *= 0.99;
  if (_calmness > 1000/_agitation) {                 //Calmness drifts towards a level determined by agitation, but it rises very slowly but falls quite quickly
      _calmness = 0.95*_calmness + 50/_agitation;
  } else {
      _calmness = 0.999*_calmness + 1/_agitation;
  }
  /*
  logPrint("Weight: ");
  logPrint(_weight);
  logPrint("   Agitation: ");
  logPrint(_agitation);
  logPrint("   Calmness: ");
  logPrintln(_calmness); */
}

void updateLocalScaleState() {
  if (!hasScale) return;

  unsigned long now = millis();
  if (now - lastScaleSampleMs < SCALE_SAMPLE_INTERVAL_MS) return;
  lastScaleSampleMs = now;

  if (!scale.is_ready()) return;

  float rawWeight = scale.get_units(1);

  if (localWeightSmooth == 0.0f) {
    localWeightSmooth = rawWeight;
  }

  float previousSmooth = localWeightSmooth;

  localWeightSmooth =
    previousSmooth + (rawWeight - previousSmooth) * SCALE_SMOOTHING;

  localWeight = localWeightSmooth;

  float movement = fabs(localWeightSmooth - previousSmooth);

  // Tune this divisor based on your expected kg/gram noise and movement.
  // Smaller number = more sensitive agitation.
  float instantAgitation = constrain(movement / 0.08f, 0.0f, 1.0f);

  localAgitation =
    localAgitation + (instantAgitation - localAgitation) * AGITATION_SMOOTHING;

  localCalmness = constrain(1.0f - localAgitation, 0.0f, 1.0f);
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

