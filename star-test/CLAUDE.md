# star-test

## Hardware

- **MCU**: ESP32
- **LED strip**: WS2811, 50 LEDs
- **Data pin**: GPIO 4
- **Brightness**: 150

## FastLED color order

The physical WS2811 strip uses **BRG** byte order.

```cpp
FastLED.addLeds<WS2811, DATA_PIN, BRG>(leds, NUM_LEDS);
```

Confirmed by colorTest: CRGB(255,0,0) drove the Blue channel, CRGB(0,255,0) drove Red, CRGB(0,0,255) drove Green.
Use `BRG` in every Arduino sketch for this strip — any other order will produce wrong colors.
