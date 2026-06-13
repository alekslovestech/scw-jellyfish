# scw-jellyfish

## Real LED hardware — color order

The physical WS2811 strips use **BRG** byte order (confirmed by hardware test on the star-test rig).

```cpp
FastLED.addLeds<WS2811, DATA_PIN, BRG>(leds, NUM_LEDS);
```

Mapping: `CRGB(255,0,0)` → Blue, `CRGB(0,255,0)` → Red, `CRGB(0,0,255)` → Green.
Every Arduino sketch targeting this hardware must use `BRG` — any other order produces wrong colors.

## Sub-projects

- `star-test/` — 5-pointed star, 50 LEDs, ESP32, GPIO 4
- `visualizer-ts/` — Three.js browser visualizer for the jellyfish installation
- `jelly-firmware/` — Arduino firmware for the jellyfish installation
