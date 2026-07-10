import { Effect } from "../types";
import { hash01 } from "../hash";

// --- OLD-SCHOOL TV STATIC ("snow") ---
// An overlay effect: on top of whatever the base animation drew, sprinkle random
// grey speckles that reshuffle every flicker frame — the shimmering "snow" of an
// untuned analog TV. Like fallingRain it's pure per-LED noise, but with no motion:
// each pixel independently decides, each frame, whether it flashes and how bright.
//
// Determinism note: the speckle pattern comes from hash01(ledId, frame) — integer
// math, so it's reproducible and portable to the Arduino firmware.

// Live-tunable from the panel (see controls below).
const params = {
  amount: 0.7, // how strongly the static overrides the base (0 = none, 1 = full)
  density: 0.5, // fraction of LEDs flashing at any instant (0–1)
  speed: 24, // flicker rate in Hz — how often the snow reshuffles
};

export const tvStatic: Effect = {
  name: "tvStatic",
  enabled: false, // off by default — toggle it on in the panel

  params,
  controls: [
    { key: "amount", label: "amount", min: 0, max: 1, step: 0.05 },
    { key: "density", label: "density", min: 0, max: 1, step: 0.05 },
    { key: "speed", label: "flicker (Hz)", min: 1, max: 60, step: 1 },
  ],

  apply(leds, time) {
    // Quantise time into flicker frames so the noise holds for 1/speed seconds
    // then jumps — that hard jump is what makes it read as "static", not a fade.
    const frame = Math.floor(time * params.speed);

    for (const led of leds) {
      // Does this pixel flash this frame?
      if (hash01(led.id, frame, 11) >= params.density) continue;

      // A grey speckle, brightness randomised per pixel/frame.
      const grey = 0.3 + hash01(led.id, frame, 12) * 0.7;

      // Blend the base toward the grey speckle by `amount`.
      led.color.setRGB(
        led.color.r + (grey - led.color.r) * params.amount,
        led.color.g + (grey - led.color.g) * params.amount,
        led.color.b + (grey - led.color.b) * params.amount,
      );
      led.intensity = Math.max(led.intensity, params.amount * grey);
    }
  },
};
