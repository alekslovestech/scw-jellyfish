# Migration from the uploaded code

## What was preserved

The refactor deliberately retains the proven operational interfaces:

- mDNS service `_esp32art._tcp.local`;
- `/status` plus `/number` compatibility;
- identify, rename, ID, role, position, pattern, debug number, log, and OTA operations;
- small/big jellyfish geometry selection;
- per-device world position and Y rotation;
- all legacy dropdown pattern names;
- individual pattern selection as a test and fallback path.

## What changed

### Firmware structure

The previous Arduino build spread setup/network control and each pattern across separate `.ino` tabs with shared globals. The new sketch is a thin orchestrator and the implementation is divided by responsibility:

| New module | Responsibility |
|---|---|
| `Settings` | NVS schema, validation, migration, persistence |
| `Geometry` | strip coordinates and world transforms |
| `SensorModel` | HX711, occupancy, agitation, calmness, activation edge |
| `InteractionEngine` | peer platforms, spatial field, all-calm/chorus state |
| `ShowClock` | prioritized shared time |
| `PatternEngine` | global scenes, waves, fallback registry, transitions/current limit |
| `NetworkManager` | Wi-Fi, mDNS, UDP protocol, fallback conductor |
| `WebApi` | HTTP control, calibration, status, OTA |
| `LogBuffer` | bounded serial/web log |

### Pattern architecture

The old dropdown selected a complete private effect for one device. In normal `auto` operation the dropdown no longer drives the main show; instead each pixel samples the shared installation field. Dropdown patterns remain available in `fallback` mode and share a single parameter block. A per-controller override can keep one jellyfish in fallback for testing while every other unit continues following the global conductor.

The retained identifiers are:

```text
heartbeat demo ripple fireSpread waterfall twoToneDiffuse colorwheel
bottomfill sensordemo fallingRain movementSimulation innerSpreadWave
rainbowWave crazy sparkle none
```

The renderer re-expresses these effects through the new geometry/color utilities rather than copying their old global state verbatim. Visual timing may therefore differ, while the recognizable testing purposes and names remain.

### Platform behaviour

Scale input is no longer treated only as a raw number. It becomes occupancy, agitation, calmness, and an activation edge. Those values are broadcast with platform position and combined spatially by all jellyfish.

### Dashboard

The earlier fleet table was maintenance-first. The new interface is show-first:

- installation map and global state;
- chorus/calmness controls;
- spatial activation test;
- optional audio bridge;
- per-controller dialogs for role, transform, sensor calibration, fallback patterns, logs, and OTA.

The Python code is separated into device model, fleet manager, installation model, conductor, and API application. The browser code is split into HTML, CSS, and JavaScript.

## One-time NVS migration

On first boot of schema version 2, `SettingsStore` checks the previous `config` namespace and migrates available keys for:

- scale/jelly/big roles;
- fallback pattern;
- X/Y/Z position;
- Y rotation.

It then writes schema version 2 and uses the new namespaces. Identity and settings already available under new namespaces take precedence according to the load path.

This migration cannot recover values that were supplied only by source files absent from the upload. In particular, verify:

- Wi-Fi credentials;
- LED and HX711 pins;
- any network-specific static addressing;
- exact strip method/timing assumptions;
- load-cell calibration;
- power limits.

The refactor defaults to DHCP because the original network-specific configuration was not present. Legacy static addressing can be re-enabled in `AppConfig.h` after confirming the subnet and ID-to-address convention.

## Recommended deployment order

1. Archive the currently deployed binary and record all controller IDs, names, roles, positions, pattern selections, and load-cell readings.
2. Configure credentials/pins and build the refactor.
3. Flash one noncritical controller over cable.
4. Confirm NVS migration through `/status` and the dashboard.
5. Validate its LED geometry and current limit.
6. For a scale controller, tare and recalibrate before trusting occupancy.
7. Validate UDP waves and shared clock with two jellyfish.
8. Add the dashboard conductor and simulator.
9. Roll out a small batch by OTA, then the remaining fleet sequentially.
10. Commission global calm/chorus and audio only after all positions and scales are stable.

## Rollback

Keep the previous known-good `.bin`. The refactor writes new NVS namespaces and a schema marker but does not intentionally delete the old `config` namespace. A rollback may still interpret storage differently, so test rollback on one controller and be prepared to restore configuration manually or erase/reprovision NVS.
