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

void overlayRandomPixelGlows(float newStartsPerSecond,
                             float risePerSecond,
                             float fadePerSecond,
                             float peakBrightness = 255.0f);

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
  // First render the normal selected pattern.
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

  // Then add all active installation-wide ripples on top of that frame.
  if (isJelly) {
    overlayGlobalRipples();
  }

/*  overlayRandomPixelGlows(
      2.0f,    // average new starts per second
      8.0f,   // rise brightness per second
      3.0f,   // fade brightness per second
      180.0f   // peak brightness
  );*/


  showLEDs();
  if (hasScale && scale.is_ready()) {
    _weight = scale.get_units(1);  
    weightUpdate();
  }
  _ledFrame++;
}

// Flashing identify routine, called directly from firmware on request
void runIdentifySequence() {
  const uint8_t count = activeStripCount();

  for (int times = 0; times < 3; times++) {
    for (uint8_t s = 0; s < count; s++) {
      const uint16_t pixelCount = activeLedCountPerStrip();
      fillStrip(strips[s], pixelCount,
                RgbColor(MAX_BRIGHTNESS, MAX_BRIGHTNESS, MAX_BRIGHTNESS));
      strips[s].Show();
    }
    delay(400);

    for (uint8_t s = 0; s < count; s++) {
      const uint16_t pixelCount = activeLedCountPerStrip();
      fillStrip(strips[s], pixelCount, RgbColor(0, 0, 0));
      strips[s].Show();
    }
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

const Pos3D& getLedPosition(uint8_t stripIndex, uint16_t pixelIndex) {
  return ledPos[stripIndex][pixelIndex];
}

// Return this LED's position in the shared/global coordinate system.
// Local jelly coordinates are first rotated around Y, then translated by devicePos.
Pos3D getAbsoluteLedPosition(uint8_t stripIndex, uint16_t pixelIndex)
{
    const Pos3D& local = getLedPosition(stripIndex, pixelIndex);
    const float angle = deviceRotationY * DEG_TO_RAD;
    const float c = cosf(angle);
    const float s = sinf(angle);

    Pos3D absolute;
    absolute.x = devicePos.x + c * local.x + s * local.z;
    absolute.y = devicePos.y + local.y;
    absolute.z = devicePos.z - s * local.x + c * local.z;
    return absolute;
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

Pos2D find2Dpos(uint16_t p) {
  return isBig ? bigJellyFind2Dpos(p) : smallJellyFind2Dpos(p);
}

// Position in the vertical plane along one of the eight radial strips.
Pos2D smallJellyFind2Dpos(uint16_t p) {
  Pos2D pos;

  if (p < 17) {
    pos.y = -0.1f * p;
    pos.x = 0.0f;
  } else if (p < 35) {
    pos.y = 0.1f * (p - 35);
    pos.x = 0.15f;
  } else if (p < 40) {
    pos.y = 0.0f;
    pos.x = 0.1f * (p - 33);
  } else {
    pos.y = 0.1f * (40 - p);
    pos.x = 0.6f;
  }

  return pos;
}

Pos2D bigJellyFind2Dpos(uint16_t p) {
  Pos2D pos;

  if (p < 50) {
    // First 50 LEDs: straight down the centre.
    pos.x = 0.0f;
    pos.y = -0.1f * p;
  } else if (p < 100) {
    // Next 50: back up, slightly farther from the centre.
    pos.x = 0.15f;
    pos.y = 0.1f * (p - 100);
  } else if (p < 125) {
    // Next 25: radially outward across the bell.
    pos.x = 0.15f + 0.1f * (p - 100);
    pos.y = 0.0f;
  } else {
    // Final 25: down from the outer edge.
    pos.x = 2.55f;
    pos.y = -0.1f * (p - 125);
  }

  return pos;
}

// Rotate one of the first eight strips around the jellyfish's Y axis.
Pos3D find3Dpos(uint16_t p, uint8_t stripIndex) {
  const Pos2D pos2D = find2Dpos(p);
  const float angle = TWO_PI * stripIndex / NUM_SHORT_STRIPS;

  Pos3D pos;
  pos.x = cosf(angle) * pos2D.x;
  pos.y = pos2D.y;
  pos.z = sinf(angle) * pos2D.x;
  return pos;
}

void calculateLedPositions() {
  const uint16_t pixelCount = activeLedCountPerStrip();

  for (uint8_t strip = 0; strip < NUM_SHORT_STRIPS; strip++) {
    for (uint16_t pixel = 0; pixel < pixelCount; pixel++) {
      ledPos[strip][pixel] = find3Dpos(pixel, strip);
    }
  }
}

// Fill an explicitly sized strip.
void fillStrip(StripBus& strip, uint16_t pixelCount, RgbColor c) {
  for (uint16_t i = 0; i < pixelCount; i++) {
    strip.SetPixelColor(i, c);
  }
}

// Convenience overload for the currently configured jelly size.
void fillStrip(StripBus& strip, RgbColor c) {
  fillStrip(strip, activeLedCountPerStrip(), c);
}