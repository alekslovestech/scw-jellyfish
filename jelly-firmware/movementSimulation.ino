// movementSimulation — adapted from visualizer-ts/src/animations/movementSimulation.ts.
//
// A green "extend / retract" pulse that simulates a jellyfish moving. Driven by
// PHYSICAL HEIGHT (ledPos[s][p].y) so the folded inner tentacle behaves correctly.
// Two organic touches vs the straight port:
//   * each arm (strip) gets a small phase offset so they don't pulse in unison;
//   * the inner tentacle retracts less far than the outer (INNER_KEEP_DEPTH).
void movementSimulation() {
  const float SWEEP_DURATION   = 3.0f;   // seconds to retract
  const float RETURN_DURATION  = 3.0f;   // seconds to extend back
  const float PAUSE_FULL       = 0.06f;  // hold at full extension
  const float PAUSE_EMPTY      = 0.06f;  // hold at full retraction
  const float INNER_KEEP_DEPTH = 0.65f;  // inner retracts only to 65% (~half of before)
  const float OUTER_KEEP_DEPTH = 0.30f;  // outer/bell retract to 30% (unchanged)
  const float EDGE             = 0.06f;  // soft transition width at the sweep edge
  const float MAX_DEPTH        = 1.8f;   // top (y=0) to lowest inner tip (~ -1.8)
  const float STRIP_VARIANCE   = 0.5f;   // seconds of per-arm phase spread (small = subtle)
  const float GOLDEN           = 0.618033f;
  const float CYCLE = PAUSE_FULL + SWEEP_DURATION + PAUSE_EMPTY + RETURN_DURATION;

  float time = millis() / 1000.0f;

  for (int s = 0; s < 8; s++) {
    // Golden-ratio phase offset so the 8 arms drift out of lockstep.
    float stripOffset = fmodf(s * GOLDEN, 1.0f) * STRIP_VARIANCE;
    float cycleTime = fmodf(time + stripOffset, CYCLE);

    // sweepProgress: 0 = fully extended, 1 = fully retracted.
    float sweepProgress;
    if (cycleTime < PAUSE_FULL) {
      sweepProgress = 0.0f;
    } else if (cycleTime < PAUSE_FULL + SWEEP_DURATION) {
      sweepProgress = (cycleTime - PAUSE_FULL) / SWEEP_DURATION;
    } else if (cycleTime < PAUSE_FULL + SWEEP_DURATION + PAUSE_EMPTY) {
      sweepProgress = 1.0f;
    } else {
      float returnTime = cycleTime - (PAUSE_FULL + SWEEP_DURATION + PAUSE_EMPTY);
      sweepProgress = 1.0f - (returnTime / RETURN_DURATION);
    }

    // Per-segment lit depth: inner pulls back less than outer/bell.
    float innerLit = INNER_KEEP_DEPTH + (1.0f - INNER_KEEP_DEPTH) * (1.0f - sweepProgress);
    float outerLit = OUTER_KEEP_DEPTH + (1.0f - OUTER_KEEP_DEPTH) * (1.0f - sweepProgress);

    for (int p = 0; p < NUM_LEDS_PER_STRIP; p++) {
      int group; float t;
      fsSegmentForPos(p, &group, &t);
      float litDepth = (group == FS_SEG_INNER) ? innerLit : outerLit;

      float depth = fsClamp01(-ledPos[s][p].y / MAX_DEPTH);
      float on = fsClamp01((litDepth - depth) / EDGE); // 1 inside, 0 outside, soft edge
      strips[s].SetPixelColor(p, RgbColor(0, fsToByte(on * 255.0f), 0));
    }
  }
}
