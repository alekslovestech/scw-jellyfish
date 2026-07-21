# Architecture

## Goals

The refactor separates hardware access, sensing, installation state, rendering, networking, persistence, and operator control. The key change is conceptual: a controller no longer chooses only a private strip effect. It samples a shared spatial field whose inputs are platform state and whose phase is derived from a shared show clock.

```text
                    dashboard / high-priority conductor
                   clock + show + canonical installation
                                  |
                                  v
HX711 -> SensorModel -> platform UDP -> InteractionEngine -> PatternEngine -> LEDs
             |               |               ^                    |
             +-> activation -+---------------+                    |
                                  |                                |
                                  +-> OSC -> four-channel audio <--+

                   device ID 1 supplies fallback clock/state
                   when the dashboard conductor is unavailable
```

## Firmware modules

### `SettingsStore`

Loads, normalizes, and persists identity, role, transform, pattern controls, show mode, sensor tuning, and interaction tuning in separate NVS namespaces. Schema version 2 performs a one-time migration from the old `config` namespace when those keys are present.

### `Geometry`

Builds per-pixel local coordinates for small or large jellyfish and transforms them into installation/world coordinates using each controller's position and Y rotation. World coordinates make one wave or chorus visually continuous across physically separate controllers.

### `SensorModel`

Owns HX711 reads and converts load into three independent signals:

1. **Occupancy** uses `occupancyOnKg` and `occupancyOffKg` hysteresis.
2. **Agitation** measures frame-to-frame filtered-weight movement above a noise floor, then applies fast attack and slower release.
3. **Calmness** accumulates only while occupied and below the stillness threshold. Movement subtracts accumulated stable time and calmness falls using a shorter configurable time constant.

An empty-to-occupied edge creates exactly one activation event. Taring clears the filtered and derived state.

### `ShowClock`

Maps the controller's monotonic timer to shared show time. Sources have priority and expire: the dashboard broadcasts priority 100; the fallback device conductor broadcasts priority 10. Render functions use shared time, not frame count or packet arrival time.

### `InteractionEngine`

Maintains the local platform plus recent peer platforms. Stale peers expire. At any world-space sample point, each platform contributes through a Gaussian-like distance weight based on `influenceRadiusMeters`.

The resulting field contains:

- presence
- agitation
- calmness
- harmony
- synchronization
- turbulence
- brightness

Presence establishes a local brightness lift. Agitation adds further brightness and movement. Calmness reallocates the visual energy toward smooth pearl tones and phase coherence rather than reducing brightness.

The engine also derives installation-wide state. A chorus candidate requires all of the following:

- at least `expectedPlatformCount` recent platforms are visible;
- every visible platform is occupied;
- minimum occupied-platform calmness is at or above `calmThreshold`;
- maximum agitation is at or below `maxAgitationForChorus`;
- the condition remains true for `allCalmHoldSeconds`.

Release uses lower calmness and higher agitation thresholds plus occupancy loss, preventing flicker near the boundary. Chorus intensity fades rather than switching instantly.

### `PatternEngine`

Owns LED buffers, scene transitions, current scaling, activation overlays, and the parameterized legacy registry.

`auto` renders the interactive forest and blends toward the global chorus as the installation chorus value rises. The ambient forest combines slow world-space waves, body-coordinate breathing, audio-reactive detail, and local field modulation. The chorus uses a common world-space phase so nearby and distant jellyfish participate in one coherent structure.

Each controller normally follows the global mode. A separately persisted local override can force that controller into auto, forest, chorus, blackout, or its selected fallback pattern for commissioning. UDP show packets continue updating the underlying global mode without cancelling the override; disabling the override rejoins the controller without a special reboot.

Activation events render as expanding spherical shells:

```text
radius = elapsed_seconds * wave_speed
intensity = envelope * gaussian(distance_to_origin - radius, wave_width)
```

A short high-energy shell is layered over the active scene, so arrival feedback works in forest, fallback, and chorus modes.

Legacy patterns are selected through an enum and one renderer dispatch. The same normalized controls—brightness, speed, scale, density, two hues, contrast, and sparkle—shape every fallback rather than exposing unrelated magic numbers.

### `NetworkManager`

Handles Wi-Fi retry, mDNS advertisement, UDP receive/broadcast, optional static-address compatibility, and audio-feature packets. It accepts only protocol-v2 packets when a packet declares a protocol version. Platform and canonical-installation state have timeouts, so a stopped sender cannot leave the installation permanently occupied or calm.

### `WebApi`

Provides status, configuration, calibration, pattern, show, log, test-wave, and OTA endpoints. The API retains the names used by the previous dashboard where practical. `/status` contains both legacy flat fields and nested sensor/interaction/installation telemetry.

## Dashboard modules

### `FleetManager`

Discovers `_esp32art._tcp.local` services, keeps a lock-protected device registry, polls status concurrently, and proxies HTTP/OTA requests. The controller IP is used for requests because `.local` resolution is not equally reliable on every operating system.

### `ShowConductor`

Binds UDP port `42120`, ingests platform and activation packets, and broadcasts at 4 Hz:

- high-priority shared clock;
- canonical installation state;
- current global show settings.

It mirrors the firmware's all-calm logic so every controller receives the same latch and fade state even when packets arrive in different orders. Device status polling provides a fallback telemetry path when UDP reception is blocked.

### Browser UI

The dependency-free UI is split into HTML, CSS, and JavaScript. It presents the installation map and collective state first, with fleet maintenance in per-controller dialogs. Auto-refresh pauses while a form or firmware file input is active so edits are not destroyed.

## Audio architecture

The dashboard converts a platform's X/Z position into equal-power bilinear gains for front-left, front-right, rear-left, and rear-right. OSC carries platform, activation, installation, and show state to the optional engine.

The reference engine generates:

- a continuous low underwater drone/noise bed;
- spatial local layers for occupied platforms;
- a positioned activation transient;
- increasingly consonant and synchronized layers as calmness and chorus rise.

The audio engine can also publish low-rate spectral/beat features back to the ESP broadcast channel. Lighting does not depend on those packets and smoothly releases the features if they stop.

## Failure behaviour

| Failure | Expected result |
|---|---|
| Dashboard stops | Controllers continue; ID 1 becomes lower-priority clock/state source after timeout |
| Audio engine stops | Lighting and dashboard continue; OSC errors are ignored |
| One platform disappears | Peer expires; collective chorus releases |
| One controller loses Wi-Fi | It continues local rendering/sensing from persisted settings; global state eventually becomes stale |
| HX711 unavailable | Controller reports sensor unavailable and can still act as a jellyfish |
| Bad/mismatched UDP packet | Packet is ignored |
| OTA interrupted | Existing firmware remains subject to ESP32 update-partition behaviour; the dashboard reports failure |

## Deliberate boundaries

The software does not determine structural safety of nets/platforms, certify load cells, size power conductors, implement emergency lighting, or provide a safety-rated shutdown. Those require independent engineering controls. The `blackout` mode is a show control, not an emergency-stop system.
