# 2.0.3 - Wi-Fi/lwIP startup fix

- Defers `WebServer.begin()` until the ESP32 station is actually connected.
- Stops the HTTP listener before Wi-Fi/lwIP is torn down for reconnects.
- Prevents the ESP32-S3 `tcpip_send_msg_wait_sem ... Invalid mbox` assertion when credentials are absent or invalid.
- Includes an editable `Secrets.h` and `WIFI_SETUP.txt`; `Secrets.example.h` remains only an example.
- Retains static SCW addressing (`192.168.0.(100 + device ID)`).

# Release notes

## 2.0.2 - SCW static Wi-Fi restoration

- Restored the installation's no-DHCP static addressing by default.
- Device ID `N` now uses `192.168.0.(100 + N)`; unassigned devices use `.199`.
- Reapplies station mode and static IP before every connection attempt.
- Restored hostname setup and disabled ESP32 Wi-Fi power saving after association.
- Added useful static-IP and Wi-Fi status logging.

## 2.0.1 - Arduino compatibility

## Arduino/ESP32 build compatibility

- Added explicit GNU++11-compatible constructors for `Vec3` and `ColorF`, fixing all brace-construction and brace-assignment errors reported by Arduino-ESP32 2.0.17.
- Replaced the ESP32-S3-only `NeoEsp32LcdX8Ws2812xMethod` with NeoPixelBus' architecture-selected `X8Ws2812xMethod` alias. Classic ESP32 targets now use the supported eight-channel backend automatically.
- Removed incorrect `const` qualifiers from HTTP argument helpers because Arduino-ESP32's `WebServer::hasArg()` and `WebServer::arg()` are non-const.
- Pinned PlatformIO Espressif32 6.8.0 (Arduino core 2.0.17) and NeoPixelBus 2.8.2, and stopped forcing C++17, so PlatformIO checks the same GNU++11 compatibility required by Arduino IDE builds.
- Added a complete GNU++11 host syntax smoke test to the release validation performed for this patch.

---

# Release notes: 2.0.0-refactor

## Installation-level behaviour

- Added a world-space ambient forest scene.
- Added spatial influence from every recent occupied platform.
- Added occupancy-edge activation events with synchronized spherical waves.
- Added agitation-driven local energy and calmness-driven harmony/phase locking.
- Added hold/release hysteresis and a fading collective chorus.
- Added dashboard and device-conductor shared-clock hierarchy.

## Firmware

- Replaced shared `.ino` globals with focused C++ modules.
- Added persisted sensor, interaction, show, pattern, role, identity, and transform settings.
- Added one-time migration of the previous `config` NVS namespace.
- Added protocol-v2 UDP platform, activation, clock, installation, show, and audio packets.
- Added version filtering, peer expiry, canonical-state priority, scene crossfades, current limiting, and non-blocking identify.
- Preserved every previous dropdown pattern as a parameterized fallback.
- Added a persisted per-controller override so a single fallback pattern can be tested without leaving the global show.
- Preserved prior HTTP/mDNS/OTA operations where practical.

## Dashboard

- Replaced the fleet-table-first UI with a spatial show-conductor interface.
- Added installation map, calm/chorus state, global tuning, spatial test waves, sensor calibration, fallback controls, logs, and OTA.
- Split fleet registry, device model, interaction model, conductor, API, HTML, CSS, and JavaScript.
- Added persistence of global settings to each online controller.
- Added optional OSC routing with four equal-power speaker gains.
- Added end-to-end activation event deduplication so UDP loopback cannot trigger a wave or spatial sound twice.

## Audio and test tooling

- Added an optional four-channel generative reference soundscape.
- Added a no-hardware platform/activation simulator.
- Added 15 automated tests, browser syntax validation, Python compilation checks, and a PlatformIO-aware check script.

## Validation boundary

The source passes the included 15-test suite, Python bytecode compilation, Node JavaScript syntax checking, and a host-side C++17 syntax/warning sweep. A real PlatformIO ESP32 build was not available in the preparation environment. Hardware timing, GPIO assignments, HX711 calibration/polarity, LED current/power distribution, Wi-Fi broadcast behaviour, and multichannel audio routing remain commissioning tasks.
