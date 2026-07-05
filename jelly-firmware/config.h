#pragma once
#include <Arduino.h>

// Compile-time constants shared across the sketch. Only pure, immutable values
// live here — mutable state (deviceName, isJelly, calibration_factor, strips[],
// pin table, …) stays with the code that owns it.

// ── LED layout ──────────────────────────────────────────────────
constexpr uint16_t NUM_LEDS_PER_STRIP = 50;
constexpr uint16_t NUM_STRIPS         = 12;
const uint8_t      MAX_BRIGHTNESS      = 128;   // 0-255, global LED brightness cap
const float        UPDATE_INTERVAL_MS  = 10;    // ms between LED frames

// ── Ported-animation segment layout (fireSpread / waterfall / twoToneDiffuse) ──
// Along one strip, confirmed on hardware: inner 0-35, bell 36-39, outer 40-49.
const int FS_SEG_INNER = 0;
const int FS_SEG_BELL  = 1;
const int FS_SEG_OUTER = 2;
const int FS_INNER_LEDS = 36;
const int FS_BELL_LEDS  = 4;
const int FS_OUTER_LEDS = 10;

// ── Load cell (HX711) ───────────────────────────────────────────
const int HX711_DOUT = 14;
const int HX711_SCK  = 13;
constexpr unsigned long HX711_READY_TIMEOUT_MS = 1500;

// ── Wi-Fi ───────────────────────────────────────────────────────
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 5000;
const unsigned long WIFI_RETRY_INTERVAL_MS  = 1000000;

// ── UDP ports ───────────────────────────────────────────────────
const int MUSIC_UDP_PORT = 4210;
const int SCALE_UDP_PORT = 4211;

// ── Logging ─────────────────────────────────────────────────────
const size_t MAX_LOG_SIZE = 4096;