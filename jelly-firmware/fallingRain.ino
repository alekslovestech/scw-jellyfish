// fallingRain — ported from visualizer-ts/src/animations/fallingRain.ts.
//
// Each strip (s) is one independent rain column with NUM_DROPS drops falling at
// varied speeds. Vertical position uses PHYSICAL HEIGHT (ledPos[s][p].y) so the
// folded inner tentacle reads as straight-down rain (the TS t-based getVertPos
// would fold it). Drops are short bright-headed streaks that fade to a tail.
void fallingRain() {
  const float SPEED          = 0.1f;   // vertical units / second
  const float SPEED_VARIANCE = 0.2f;  // spread of per-drop speeds
  const int   NUM_DROPS      = 7;      // simultaneous drops per column
  const float DROP_SPACING   = 0.13f;  // phase spacing between drops
  const float TRAIL_LENGTH   = 0.08f;  // short sharp drops
  const float HEAD_G         = 0.15f;  // green tint at the head (blue is full)
  const float MAX_DEPTH      = 1.8f;   // top (y=0) to lowest inner tip (~ -1.8)
  const float GOLDEN         = 0.618033f;

  float time = millis() / 1000.0f;

  for (int s = 0; s < 8; s++) {
    // Golden-ratio spread so no two columns share a phase.
    float stripSeed = fmodf(s * GOLDEN, 1.0f);

    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
      float vPos = fsClamp01(-ledPos[s][p].y / MAX_DEPTH); // 0 top -> 1 bottom

      float intensity = 0.0f;

      for (int d = 0; d < NUM_DROPS; d++) {
        float dropSeed   = fmodf(stripSeed + d * GOLDEN, 1.0f);
        float dropSpeed  = SPEED * (1.0f - SPEED_VARIANCE / 2.0f + dropSeed * SPEED_VARIANCE);
        float dropOffset = fmodf(dropSeed + d * DROP_SPACING, 1.0f);
        float dropFront  = fmodf(dropOffset + time * dropSpeed, 1.0f); // descends 0->1, loops

        float diff = dropFront - vPos;      // >0: this LED is in the trail above the front
        if (diff > 0.0f && diff < TRAIL_LENGTH) {
          float frac = 1.0f - diff / TRAIL_LENGTH;   // 1 at head, 0 at tail
          if (frac > intensity) intensity = frac;
        }
      }

      // Head green tint baked as HEAD_G*intensity^2, blue as intensity (matches TS).
      strips[s].SetPixelColor(p, RgbColor(
        0,
        fsToByte(HEAD_G * intensity * intensity * 255.0f),
        fsToByte(intensity * 255.0f)
      ));
    }
  }
}
