# Jellyfish Forest

A refactored control system for a spatial installation of suspended illuminated jellyfish, meditation platforms with load cells, and a four-channel ambient soundscape.

The project now treats the installation as one distributed instrument. Individual patterns remain available for testing and fallback, while the normal `auto` mode creates a continuous world-space light field driven by occupancy, agitation, calmness, and collective stillness.

## Installation behaviour

- **Ambient forest:** every jellyfish glows calmly even when the platforms are empty.
- **Arrival:** an empty-to-occupied transition triggers a spatial sound and a fast spherical light wave from the platform's measured position.
- **Movement:** jostling increases local brightness, turbulence, animation speed, and warmer accents.
- **Stillness:** sustained stable weight builds calmness. Nearby jellyfish remain bright but become smoother, more harmonious, and progressively phase-locked.
- **Collective calm:** after every expected, visible platform is occupied and calm for the configured hold time, the installation fades into the brightest synchronized chorus.
- **Graceful fallback:** the dashboard is the preferred high-priority conductor, but device ID 1 can provide a lower-priority clock and canonical state if the dashboard disappears. Each controller also retains local settings and its legacy fallback pattern.

## Repository layout

```text
firmware/                  ESP32/Arduino firmware and PlatformIO project
  JellyfishForest/         Modular controller source

dashboard/                 FastAPI fleet dashboard and show conductor
  static/                  Dependency-free browser UI
  tests/                   Interaction, settings, protocol, and route tests

audio/                     Optional four-output generative soundscape engine
  tests/                   DSP/state tests that do not require audio hardware

tools/simulate_platforms.py  Hardware-free platform/activation simulator
scripts/check.sh            Repeatable software checks; builds firmware when pio exists
docs/                       Architecture, protocol, migration, and commissioning guides
```

## Quick start: dashboard

Python 3.11 or newer is recommended.

```bash
cd jellyfish-refactor
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r dashboard/requirements.txt
python3 run_dashboard.py
```

Open the host running the dashboard on port `8080`. Controllers are discovered through mDNS service `_esp32art._tcp.local`; the dashboard polls `/status`, broadcasts protocol-v2 show state on UDP port `42120`, and provides OTA, calibration, positioning, role, and fallback-pattern controls.

The dashboard saves global settings to `dashboard/show_settings.json` and also posts them to online controllers so autonomous fallback uses the same values after a reboot.

## Quick start: firmware

1. Copy the credential template and set at least one network:

   ```bash
   cp firmware/JellyfishForest/Secrets.example.h firmware/JellyfishForest/Secrets.h
   ```

2. Verify the eight LED pins, HX711 pins, strip lengths, power budget, and network assumptions in `firmware/JellyfishForest/AppConfig.h`.
3. Build and flash with PlatformIO. The project pins Espressif32 platform 6.8.0, which carries Arduino core 2.0.17:

   ```bash
   pio run -d firmware
   pio run -d firmware -t upload
   ```

The provided pin values are compile-safe examples inherited without the missing installation-specific configuration. They are **not verified wiring assignments**. Start with one controller on a current-limited bench supply before a fleet rollout.

### Arduino IDE / Arduino-ESP32 2.0.17

Open `firmware/JellyfishForest/JellyfishForest.ino` and install these libraries through Library Manager before compiling:

- NeoPixelBus **2.8.2**
- HX711 by bogde
- ArduinoJson **6.21.5 or another compatible 6.x release**

Select the actual board model. For a classic ESP32/WROOM controller, `ESP32 Dev Module` is the appropriate generic target; do not select an ESP32-S3 target unless the hardware is an S3. Version 2.0.2 uses NeoPixelBus' architecture-selected eight-channel method and is compatible with the GNU++11 mode used by Arduino-ESP32 2.x.

## Hardware-free interaction test

Run the dashboard, then in another terminal:

```bash
python3 tools/simulate_platforms.py --count 4 --scenario cycle
```

The 80-second loop demonstrates empty platforms, sequential arrivals with activation waves, agitation, settling, collective calm, and release. `--scenario calm`, `agitated`, and `empty` hold fixed states. Use `--target <host-or-broadcast-address>` when the operating system does not route limited broadcast packets as expected.

## Optional four-channel audio engine

The audio engine receives the dashboard's OSC messages on port `9000`, renders a continuous underwater bed plus platform-local layers and activation transients, and outputs four channels in this order:

1. front-left
2. front-right
3. rear-left
4. rear-right

```bash
python3 -m pip install -r audio/requirements.txt
python3 -m audio.engine --list-devices
python3 -m audio.engine --device <output-device> --channels 4
```

Enable the audio bridge in the dashboard and set its OSC host/port. This engine is a reference implementation, not a final mastered soundtrack; speaker routing, limiter headroom, room EQ, and artistic material must be commissioned in the actual forest.

## Show modes

| Mode | Behaviour |
|---|---|
| `auto` | Ambient global field plus platform influence, activation waves, calm synchronization, and automatic chorus |
| `forest` | Force the calm ambient world-space field without platform modulation |
| `chorus` | Force the full synchronized global pattern for testing |
| `fallback` | Run the selected per-jelly legacy pattern using shared parameter controls |
| `blackout` | Render black while keeping sensing, networking, and control online |

## Legacy fallback parameters

A controller dialog can be set to **Follow global conductor** or given a persisted local override. A local `fallback` override keeps one jellyfish on its selected test pattern even while the rest of the installation remains in the global show. Disable the override to rejoin the conductor.

Every retained pattern uses one shared parameter block rather than unrelated constants:

- brightness
- speed
- spatial scale
- density
- primary and secondary hue
- contrast
- sparkle amount

The retained names are `heartbeat`, `demo`, `ripple`, `fireSpread`, `waterfall`, `twoToneDiffuse`, `colorwheel`, `bottomfill`, `sensordemo`, `fallingRain`, `movementSimulation`, `innerSpreadWave`, `rainbowWave`, `crazy`, `sparkle`, and `none`.

## Validation

Install the lightweight test dependencies and run the repeatable checks with:

```bash
python3 -m pip install -r requirements-dev.txt
./scripts/check.sh
```

This runs Python tests, bytecode compilation, and JavaScript syntax validation. When PlatformIO is installed it also performs the real ESP32 build. The delivered source was additionally checked with a host-side C++17 syntax harness; actual LED timing, HX711 polarity/calibration, network topology, current limiting, and audio-interface routing still require hardware commissioning.

## Important design invariants

- Calmness changes animation character and synchronization; it does not make an occupied area dimmer.
- A collective chorus requires every currently visible platform to be occupied, not merely the expected number.
- Occupancy and chorus use separate enter/release thresholds to prevent chatter.
- Broadcast packets are versioned and stale peers expire.
- Lighting remains operational if the optional audio process fails.
- Network loss does not stop local sensing or rendering.

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Network and control protocol](docs/PROTOCOL.md)
- [Commissioning checklist](docs/COMMISSIONING.md)
- [Migration from the uploaded sketches](docs/MIGRATION.md)
