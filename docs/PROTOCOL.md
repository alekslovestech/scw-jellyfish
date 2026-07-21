# Network and control protocol

## Transport and versioning

- UDP show port: `42120`
- Encoding: UTF-8 JSON, one object per datagram
- Current protocol: `2`
- Normal destination: local IPv4 broadcast
- HTTP controller port: `80`
- mDNS service: `_esp32art._tcp.local`
- OSC audio default: UDP `9000`

Packets that include a different `protocol` value are ignored. Receivers also accept a missing protocol field for limited compatibility with early development packets; new senders should always include it.

## Coordinate system

All spatial values are metres in a shared installation coordinate system:

- X: left/right across the mapped room
- Y: vertical height
- Z: front/rear across the mapped room

The dashboard speaker map uses X/Z only. Firmware light fields and activation spheres use X/Y/Z. Every jellyfish and platform therefore needs an accurate common transform.

## UDP packets

### Platform telemetry

Sent by each scale controller at approximately 5 Hz.

```json
{
  "protocol": 2,
  "type": "platform",
  "id": 3,
  "sequence": 1042,
  "x": 2.4,
  "y": 6.1,
  "z": -1.8,
  "weightKg": 71.3,
  "agitation": 0.08,
  "calmness": 0.76,
  "occupied": true,
  "showTimeMs": 193224
}
```

`agitation` and `calmness` are normalized to 0–1. Peers expire after about three seconds without a new packet.

### Activation event

Sent once on an empty-to-occupied edge and also used by dashboard test waves.

```json
{
  "protocol": 2,
  "type": "activation",
  "eventId": 29384932,
  "platformId": 3,
  "x": 2.4,
  "y": 6.1,
  "z": -1.8,
  "weightKg": 71.3,
  "agitation": 0.64,
  "showTimeMs": 193224
}
```

`eventId` is used to suppress duplicate local rebroadcast/rendering. `showTimeMs` is the wave's time origin.

### Shared clock

```json
{
  "protocol": 2,
  "type": "clock",
  "source": "dashboard",
  "priority": 100,
  "showTimeMs": 193250
}
```

Dashboard priority is 100. The fallback device conductor uses priority 10. A source must continue broadcasting or its authority expires.

### Canonical installation state

```json
{
  "protocol": 2,
  "type": "installation",
  "source": "dashboard",
  "priority": 100,
  "showTimeMs": 193250,
  "seenPlatforms": 4,
  "occupiedPlatforms": 4,
  "meanCalmness": 0.91,
  "minimumCalmness": 0.88,
  "maximumAgitation": 0.05,
  "chorus": 0.73,
  "allCalm": true,
  "chorusStartedShowTimeMs": 187500,
  "sequence": 770
}
```

Controllers prefer a fresh higher-priority canonical state but maintain their own local aggregate as fallback.

### Show settings

```json
{
  "protocol": 2,
  "type": "show",
  "source": "dashboard",
  "priority": 100,
  "mode": "auto",
  "masterBrightness": 0.9,
  "expectedPlatformCount": 4,
  "influenceRadiusMeters": 5.0,
  "calmThreshold": 0.86,
  "calmReleaseThreshold": 0.68,
  "maxAgitationForChorus": 0.16,
  "allCalmHoldSeconds": 12.0,
  "chorusFadeSeconds": 8.0,
  "activationWaveSpeedMetersPerSecond": 5.5,
  "activationWaveWidthMeters": 0.65,
  "activationWaveDurationSeconds": 5.0
}
```

The dashboard additionally POSTs these values to controllers when settings are saved, making them persistent rather than relying solely on broadcasts.

### Audio features

Optional low-rate feedback from the reference audio engine:

```json
{
  "protocol": 2,
  "type": "audio",
  "level": 0.42,
  "bass": 0.58,
  "mid": 0.31,
  "high": 0.18,
  "beat": false
}
```

Feature values decay locally when packets stop.

## Controller HTTP API

All POST bodies are `application/x-www-form-urlencoded` except OTA, which is multipart form data. Responses are JSON unless noted.

| Method and path | Purpose | Important fields |
|---|---|---|
| `GET /status` | Complete identity, role, transform, sensor, field, interaction, installation, clock, and renderer state | none |
| `GET /number` | Compatibility alias for status | none |
| `GET /log` | Ring-buffer log as UTF-8 plain text | none |
| `POST /identify` | Non-blocking visual identification pulse | none |
| `POST /rename` | Save mDNS/device name | `name` |
| `POST /device-id` | Save ID 1–32 and reboot | `id` |
| `POST /config` | Save hardware roles and power limit | `hasScale`, `isJelly`, `isBig`, `powerLimitMilliAmps` |
| `POST /position` | Save world transform | `posX`, `posY`, `posZ`, `rotationY` |
| `POST /pattern` | Select fallback pattern | `pattern` |
| `POST /pattern-parameters` | Tune shared fallback controls | `brightness`, `speed`, `scale`, `density`, `hue`, `hue2`, `contrast`, `sparkle` |
| `POST /show` | Save global/fallback mode or a local controller override | `mode`, `masterBrightness`, optional `localOverride` |
| `POST /sensor` | Save and apply load-cell model tuning | sensor fields below |
| `POST /interaction` | Save global-state and wave tuning | interaction fields below |
| `POST /tare` | Tare an available HX711 | none |
| `POST /test-wave` | Emit an activation wave | optional `x`, `y`, `z` |
| `POST /setNumber` | Compatibility/debug scalar | `value` |
| `POST /update` | ESP32 OTA firmware upload | multipart field `update` |

### Show override semantics

- Omitting `localOverride` updates the controller's global/persisted show mode. This is how the dashboard propagates installation settings.
- Supplying `localOverride=1` enables a per-controller commissioning override and stores `mode` as the override mode.
- Supplying `localOverride=0` disables the override and returns the controller to the global conductor. The accompanying valid `mode` remains the next local override choice.
- `/status` reports `showMode`, `localShowOverride`, `localOverrideMode`, and the currently rendered `scene` separately.

### Sensor fields

- `calibrationFactor` (must be non-zero)
- `occupancyOnKg`
- `occupancyOffKg` (normalized not to exceed the on threshold)
- `smoothing` (0–1, minimum 0.01)
- `noiseFloorKg`
- `movementScaleKg`
- `stillnessThreshold` (0–1)
- `calmBuildSeconds`
- `calmRiseSeconds`
- `calmFallSeconds`
- `sampleIntervalMs` (20–1000)

### Interaction fields

- `expectedPlatformCount` (1–12)
- `influenceRadiusMeters`
- `calmThreshold`
- `calmReleaseThreshold`
- `maxAgitationForChorus`
- `allCalmHoldSeconds`
- `chorusFadeSeconds`
- `activationWaveSpeedMetersPerSecond`
- `activationWaveWidthMeters`
- `activationWaveDurationSeconds`

## Dashboard API

The browser uses the following principal routes:

- `GET /api/installation`
- `GET /api/devices`
- `POST /api/installation/settings`
- `POST /api/installation/test-wave`
- `GET /api/device/{key}/log`
- per-device proxy routes for identity, role, position, pattern, parameters, show, sensor, interaction, tare, identify, and OTA
- `POST /api/broadcast/pattern`
- `POST /api/broadcast/update`

Global-setting saves return per-device persistence results so the operator can see which controllers accepted the new autonomous defaults.

## OSC audio routes

The dashboard sends these messages when the bridge is enabled:

| Address | Arguments |
|---|---|
| `/jelly/platform` | platform ID, occupied 0/1, weight, agitation, calmness, FL, FR, RL, RR gains |
| `/jelly/activation` | platform ID, X, Y, Z, weight, agitation, FL, FR, RL, RR gains |
| `/jelly/installation` | seen, occupied, mean calm, minimum calm, maximum agitation, chorus, all-calm 0/1 |
| `/jelly/show` | mode and all global show/interaction settings |

The four gains are square roots of bilinear map weights, producing equal-power interpolation at corners and across the room map.
