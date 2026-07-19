#include "config.h"
#include "color_utils.h"

struct InnerSpreadSegment {
  int group;
  float t;
};

// Returns the logical segment and normalized progress within that segment.
//
// Small layout:
//   0..34  inner folded path
//   35..39 bell/radial section
//   40..49 outer hanging section
//
// Big layout:
//   0..99    inner folded path
//   100..124 bell/radial section
//   125..149 outer hanging section
static InnerSpreadSegment innerSpreadSegmentForPixel(int p) {
  InnerSpreadSegment result;

  if (isBig) {
    if (p < 100) {
      result.group = FS_SEG_INNER;
      result.t = p / 99.0f;
    } else if (p < 125) {
      result.group = FS_SEG_BELL;
      result.t = (p - 100) / 24.0f;
    } else {
      result.group = FS_SEG_OUTER;
      result.t = (p - 125) / 24.0f;
    }
  } else {
    if (p < 35) {
      result.group = FS_SEG_INNER;
      result.t = p / 34.0f;
    } else if (p < 40) {
      result.group = FS_SEG_BELL;
      result.t = (p - 35) / 4.0f;
    } else {
      result.group = FS_SEG_OUTER;
      result.t = (p - 40) / 9.0f;
    }
  }

  result.t = fsClamp01(result.t);
  return result;
}

// innerSpreadWave — ported from visualizer-ts/src/animations/innerSpreadWave.ts.
//
// A traveling sinusoidal worm runs along the inner -> bell -> outer path.
// The folded inner section uses physical height so both sides of the fold
// illuminate together rather than following raw pixel order.
void innerSpreadWave() {
  constexpr float WORM_FREQ   = 17.0f;
  constexpr float SPEED       = 1.2f;
  constexpr float JELLY_PHASE = 0.8f;

  // Small inner tentacle reaches approximately y = -1.8.
  // Big inner tentacle reaches approximately y = -4.9.
  const float maxInnerDepth = isBig ? 4.9f : 1.8f;
  const int pixelsPerStrip = activeLedCountPerStrip();

  const float time = millis() * 0.001f;
  const float jellyOffset = _jellyId * JELLY_PHASE;

  for (int s = 0; s < NUM_SHORT_STRIPS; s++) {
    for (int p = 0; p < pixelsPerStrip; p++) {
      const InnerSpreadSegment segment =
        innerSpreadSegmentForPixel(p);

      float pathPosition;

      if (segment.group == FS_SEG_INNER) {
        // Physical height:
        //   root/top -> depth 0
        //   lowest point -> depth 1
        //
        // Convert that to path position:
        //   inner tip -> 0.0
        //   inner root -> 0.6
        const float depth = fsClamp01(
          -ledPos[s][p].y / maxInnerDepth
        );

        pathPosition = (1.0f - depth) * 0.6f;
      }
      else if (segment.group == FS_SEG_BELL) {
        pathPosition = 0.6f + segment.t * 0.2f;
      }
      else {
        pathPosition = 0.8f + segment.t * 0.2f;
      }

      float wave = sinf(
        pathPosition * WORM_FREQ
        - time * SPEED
        + jellyOffset
      );

      wave = fmaxf(wave, 0.0f);

      if (segment.group == FS_SEG_INNER) {
        // Squared red intensity gives the worm a narrower, sharper crest.
        strips[s].SetPixelColor(
          p,
          rgbWithIntensity(wave, 0.0f, 0.0f, wave)
        );
      }
      else {
        // Bell and outer tentacle remain blue between passing crests.
        strips[s].SetPixelColor(
          p,
          rgbWithIntensity(wave, 0.0f, 1.0f - wave)
        );
      }
    }
  }
}