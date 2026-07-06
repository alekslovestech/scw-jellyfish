#include "color_utils.h"

// fireSpread — ported from visualizer-ts/src/animations/fireSpread.ts
// Private gradient tables + fsSample/fsSampleBlend live here (no other pattern
// uses them). Segment/t comes from fsSegmentForPos() (leds.ino); FS_* consts and
// _jellyId/_fireStartTime are shared globals (jelly-firmware.ino).
struct FsGradStop { float pos, r, g, b; };

const FsGradStop FS_BELL_OUTER[4]       = {{0.00f,1,1,1},{0.20f,1,1,0},{0.40f,1,0.2f,0},{0.80f,1,0,0}};
const FsGradStop FS_GREEN_BELL_OUTER[4] = {{0.00f,1,1,1},{0.20f,0.6f,1,0},{0.40f,0.1f,0.9f,0},{0.80f,0,0.4f,0}};
const FsGradStop FS_BLUE_BELL_OUTER[4]  = {{0.00f,1,1,1},{0.20f,0.4f,0.8f,1},{0.40f,0.1f,0.3f,1},{0.80f,0.1f,0,0.6f}};
const FsGradStop FS_BLUE_INNER[4]       = {{0.00f,1,1,1},{0.20f,0.4f,0.8f,1},{0.50f,0.1f,0.3f,1},{0.90f,0.1f,0,0.6f}};
const FsGradStop FS_INNER[4]            = {{0.00f,1,1,1},{0.20f,1,1,0},{0.50f,1,0.2f,0},{0.90f,1,0,0}};
const FsGradStop FS_GREEN_INNER[4]      = {{0.00f,1,1,1},{0.20f,0.6f,1,0},{0.50f,0.1f,0.9f,0},{0.90f,0,0.4f,0}};

void fsSample(const struct FsGradStop* stops, float pos, float* r, float* g, float* b) {
  float p = fsClamp01(pos);
  if (p <= stops[0].pos) { *r = stops[0].r; *g = stops[0].g; *b = stops[0].b; return; }
  const struct FsGradStop& last = stops[3];
  if (p >= last.pos) { *r = last.r; *g = last.g; *b = last.b; return; }
  for (int i = 1; i < 4; i++) {
    if (p <= stops[i].pos) {
      const struct FsGradStop& a = stops[i - 1];
      const struct FsGradStop& c = stops[i];
      float f = (p - a.pos) / (c.pos - a.pos);
      *r = a.r + (c.r - a.r) * f; *g = a.g + (c.g - a.g) * f; *b = a.b + (c.b - a.b) * f;
      return;
    }
  }
  *r = last.r; *g = last.g; *b = last.b;
}

void fsSampleBlend(const struct FsGradStop* stopsA, const struct FsGradStop* stopsB, float pos, float mix, float* r, float* g, float* b) {
  float ar, ag, ab, br, bg, bb;
  fsSample(stopsA, pos, &ar, &ag, &ab);
  fsSample(stopsB, pos, &br, &bg, &bb);
  *r = ar + (br - ar) * mix; *g = ag + (bg - ag) * mix; *b = ab + (bb - ab) * mix;
}

void fireSpread() {
  const float BELL_FRACTION = 0.5f;
  const float WOBBLE_AMP = 0.10f, WOBBLE_FREQ = 5.0f, WAVE_SPEED = 10.5f;
  const float FLICKER_AMP = 0.9f, FLICKER_FREQ = 3.0f, FLICKER_PHASE = 0.53f;
  const float BASE_SCALE = 0.40f, MAX_SCALE = 1.00f, GROW_DURATION = 15.0f, FADE_WIDTH = 0.20f;
  const float BREATH_AMP = 0.12f, BREATH_FREQ = 0.75f, STRIP_AMP = 0.09f, STRIP_FREQ = 1.5f;
  const float PHI = 2.3999f, SHRINK_MAX = 0.30f;
  const float SPREAD_START = 8.0f, SPREAD_STAGGER = 2.0f;
  const float PRE_IGNITION_WINDOW = 4.0f, PRE_IGNITION_INTENSITY = 0.08f, KINDLE_DURATION = 2.0f;
  const float ENERGY_START = KINDLE_DURATION + GROW_DURATION;
  const float ENERGY_DURATION = 20.0f, MAX_ENERGY_STRIP = 0.38f, MAX_ENERGY_SHRINK = 0.62f;
  const float GREEN_START = 30.0f, GREEN_DURATION = 20.0f, GREEN_JELLY_STAGGER = 1.5f;
  const float BLUE_START = 55.0f, BLUE_DURATION = 20.0f, BLUE_JELLY_STAGGER = 1.5f;

  float time = millis() / 1000.0f;
  if (_fireStartTime < 0.0f) _fireStartTime = time;
  float elapsed = time - _fireStartTime;

  float iTime = (_jellyId == 0) ? 0.0f : SPREAD_START + (_jellyId - 1) * SPREAD_STAGGER;
  float jellyElapsed = elapsed - iTime;

  float greenProgress = fsClamp01((elapsed - GREEN_START - _jellyId * GREEN_JELLY_STAGGER) / GREEN_DURATION);
  float blueProgress  = fsClamp01((elapsed - BLUE_START  - _jellyId * BLUE_JELLY_STAGGER)  / BLUE_DURATION);

  float globalBreath = BREATH_AMP * (0.65f * sinf(time * BREATH_FREQ) + 0.35f * sinf(time * BREATH_FREQ * 2.3f));

  for (int s = 0; s < 8; s++) {
    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
      int group; float t;
      fsSegmentForPos(p, &group, &t);
      int ledId = s * NUM_LEDS_PER_STRIP + p;

      float gradPos, fromRoot;
      const struct FsGradStop* gradient;
      const struct FsGradStop* greenGradient;
      const struct FsGradStop* blueGradient;

      if (group == FS_SEG_BELL) {
        gradPos = t * BELL_FRACTION;
        gradient = FS_BELL_OUTER; greenGradient = FS_GREEN_BELL_OUTER; blueGradient = FS_BLUE_BELL_OUTER;
        fromRoot = t * 0.5f;
      } else if (group == FS_SEG_OUTER) {
        gradPos = BELL_FRACTION + t * (1.0f - BELL_FRACTION);
        gradient = FS_BELL_OUTER; greenGradient = FS_GREEN_BELL_OUTER; blueGradient = FS_BLUE_BELL_OUTER;
        fromRoot = 0.5f + t * 0.5f;
      } else {
        gradPos = 1.0f - t;
        gradient = FS_INNER; greenGradient = FS_GREEN_INNER; blueGradient = FS_BLUE_INNER;
        fromRoot = 1.0f - t;
      }

      float r = 0, g = 0, b = 0, intensity = 0;

      if (jellyElapsed <= 0.0f) {
        float preProgress = fsClamp01((elapsed - (iTime - PRE_IGNITION_WINDOW)) / PRE_IGNITION_WINDOW);
        if (preProgress > 0.0f) {
          float preScale = BASE_SCALE * preProgress * preProgress;
          float segmentAlpha = fsClamp01((preScale - fromRoot) / FADE_WIDTH);
          if (segmentAlpha > 0.0f) {
            float baseR, baseG, baseB;
            fsSampleBlend(gradient, greenGradient, gradPos, greenProgress, &baseR, &baseG, &baseB);
            if (blueProgress > 0.0f) fsSampleBlend(greenGradient, blueGradient, gradPos, blueProgress, &r, &g, &b);
            else { r = baseR; g = baseG; b = baseB; }
            intensity = preProgress * PRE_IGNITION_INTENSITY * segmentAlpha;
          }
        }
        strips[s].SetPixelColor(p, rgbWithIntensity(r, g, b, intensity));
        continue;
      }

      float growProgress = fminf(1.0f, fmaxf(0.0f, jellyElapsed - KINDLE_DURATION) / GROW_DURATION);
      float currentScale = BASE_SCALE + (MAX_SCALE - BASE_SCALE) * growProgress;

      float energyProgress = fsClamp01((jellyElapsed - ENERGY_START) / ENERGY_DURATION);
      float jellyStripAmp = STRIP_AMP + (MAX_ENERGY_STRIP - STRIP_AMP) * energyProgress;
      float jellyShrinkMax = SHRINK_MAX + (MAX_ENERGY_SHRINK - SHRINK_MAX) * energyProgress;

      float stripPhase = s * PHI;
      float stripPulse = jellyStripAmp * (
        0.5f * sinf(time * STRIP_FREQ + stripPhase) +
        0.3f * sinf(time * STRIP_FREQ * 2.1f + stripPhase * 1.6f) +
        0.2f * sinf(time * STRIP_FREQ * 3.7f + stripPhase * 0.9f));

      float floorScale = fmaxf(BASE_SCALE, currentScale - jellyShrinkMax);
      float effectiveScale = fminf(MAX_SCALE, fmaxf(floorScale, currentScale + globalBreath + stripPulse));
      float segmentAlpha = fsClamp01((effectiveScale - fromRoot) / FADE_WIDTH);

      if (segmentAlpha == 0.0f) { strips[s].SetPixelColor(p, RgbColor(0, 0, 0)); continue; }

      float wave = WOBBLE_AMP * sinf(gradPos * WOBBLE_FREQ - time * WAVE_SPEED);
      float wavePos = gradPos + wave;
      if (blueProgress > 0.0f) fsSampleBlend(greenGradient, blueGradient, wavePos, blueProgress, &r, &g, &b);
      else fsSampleBlend(gradient, greenGradient, wavePos, greenProgress, &r, &g, &b);

      float kindleProgress = fminf(1.0f, jellyElapsed / KINDLE_DURATION);
      float flickerAmp = FLICKER_AMP * kindleProgress;
      float flicker = 1.0f - flickerAmp * (0.5f + 0.5f * sinf(time * FLICKER_FREQ + ledId * FLICKER_PHASE));
      float baseIntensity = PRE_IGNITION_INTENSITY + (1.0f - PRE_IGNITION_INTENSITY) * kindleProgress;
      intensity = baseIntensity * flicker * segmentAlpha;

      strips[s].SetPixelColor(p, rgbWithIntensity(r, g, b, intensity));
    }
  }
}
