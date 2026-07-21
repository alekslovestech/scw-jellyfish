# Commissioning checklist

Commission one controller, one scale, and one jellyfish before repeating the process across the fleet. Keep a known-good firmware binary and exported configuration notes at every deployment stage.

## 1. Physical and electrical prerequisites

- Confirm structural/net/platform engineering independently of this software.
- Confirm each LED power injection path, conductor size, fuse, connector, ground return, and controller ground reference.
- Use a current-limited bench supply during first light.
- Verify that the selected GPIOs are safe for the exact ESP32 board and do not conflict with boot strapping, flash, or other peripherals.
- Confirm HX711 supply level, load-cell wiring, mechanical preload, and overload protection.
- Provide a safety-rated way to remove LED and audio power. Dashboard blackout is not an emergency stop.

## 2. Configure and build one controller

1. Copy `Secrets.example.h` to `Secrets.h` and enter the installation Wi-Fi credentials.
2. Verify `AppConfig.h` LED/HX711 pins and strip lengths.
3. Set a conservative `powerLimitMilliAmps` in the dashboard after discovery.
4. Build and flash with PlatformIO.
5. Watch the serial log at 115200 baud.
6. Confirm mDNS discovery and `/status` before connecting the full LED load.

## 3. Assign fleet identity and roles

For every controller:

- assign a unique permanent ID;
- give it a stable, URL-safe name;
- mark `Jellyfish`, `Platform scale`, and `Big geometry` as appropriate;
- set its LED current limit;
- allow the controller to reboot after an ID or scale-role change.

Reserve ID 1 for a controller that should act as the autonomous fallback conductor. It does not have to be a scale controller, but it should be powered and networked whenever the installation is active.

## 4. Establish a common coordinate system

Choose a surveyed origin and measure all jellyfish and platform positions in metres. Recommended convention:

- X grows left-to-right when viewing the primary entrance;
- Y grows upward from floor level;
- Z grows entrance-to-rear;
- Y rotation is the jellyfish's rotation around vertical.

Enter every transform in the dashboard. Update the map extents so all units are visible. Trigger test waves at known corners and inspect the shell from multiple heights; discontinuities usually indicate a wrong axis, sign, unit, or origin.

## 5. Load-cell calibration

Perform calibration with the platform mechanically complete.

1. Ensure the platform is empty and mechanically settled.
2. Press **Tare now**.
3. Place a known calibration mass near the normal sitting location.
4. Adjust `calibrationFactor` until filtered weight matches the known mass. A negative factor may be correct depending on bridge orientation.
5. Remove and replace the mass several times and verify repeatability.
6. Repeat with a second mass if available; a large mismatch suggests mechanics, wiring, creep, or load distribution rather than a tuning problem.

Never set the calibration factor to zero. Re-tare after mechanical changes, temperature stabilization, or amplifier replacement.

## 6. Tune occupancy hysteresis

Set `occupancyOnKg` high enough that climbing, touching, or net tension does not accidentally activate the platform, but below the lightest intended visitor. Set `occupancyOffKg` lower so an occupied platform does not chatter empty when a visitor shifts weight.

Acceptance test:

- empty platform remains empty for at least ten minutes of normal installation motion;
- a visitor produces one activation edge;
- normal seated movement does not repeatedly reactivate;
- leaving clears occupancy promptly and consistently.

## 7. Tune agitation

Use the live `weightKg` and `agitation` values.

- `smoothing` controls how quickly filtered load follows the raw reading.
- `noiseFloorKg` removes normal sensor/electrical jitter.
- `movementScaleKg` determines how much remaining inter-sample movement maps to full agitation.
- `sampleIntervalMs` controls measurement rate.

Start with the defaults. Observe an empty platform, a still seated visitor, normal posture adjustment, and intentional bouncing. Raise the noise floor for false motion. Increase movement scale if normal shifts saturate agitation. Avoid over-smoothing: the arrival transient and responsive local animation should remain visible.

## 8. Tune calmness

- `stillnessThreshold`: maximum agitation that counts as stable.
- `calmBuildSeconds`: stable duration required for the smoothstep target to reach full calm.
- `calmRiseSeconds`: filtering time while calmness rises.
- `calmFallSeconds`: filtering time while calmness falls after movement.

The intended feel is forgiving but legible: small breathing/postural changes should not destroy all progress, while sustained jostling should quickly return the nearby field to energetic motion. Calm areas should remain at least as bright as occupied agitated areas; only color, smoothness, and synchronization should change character.

## 9. Tune spatial influence and activation waves

1. Set `influenceRadiusMeters` so a platform strongly affects nearby jellyfish and softly reaches its next-nearest neighbors.
2. Set wave speed by measuring the perceived travel time across the room.
3. Set wave width wide enough to be visible on sparse physical samples but narrow enough to read as a moving shell.
4. Set duration long enough for the shell to leave the installation bounds.
5. Trigger waves from the dashboard map and from every real platform.

Verify that event audio is weighted toward the same physical location as the light wave.

## 10. Commission four-channel audio

1. Map interface outputs in order: front-left, front-right, rear-left, rear-right.
2. Run the audio engine's device listing and choose the intended four-output device explicitly.
3. Test each speaker alone before enabling spatial layers.
4. Set dashboard map extents to the acoustic room rectangle.
5. Trigger corner waves and verify image direction.
6. Establish limiter, interface, amplifier, and loudspeaker headroom with a qualified audio engineer.
7. Tune the continuous bed below the activation cues and collective chorus.

The reference soundscape is intentionally generative and conservative. Replace or extend its synthesis/material while preserving the OSC contract if a composed soundtrack is preferred.

## 11. Collective chorus acceptance test

Use real visitors or `tools/simulate_platforms.py`.

Verify all of these cases:

- fewer than the expected number cannot enter chorus;
- the expected number plus an additional visible empty platform cannot enter chorus;
- all platforms occupied but one agitated cannot enter chorus;
- all visible platforms calm must hold for the configured time before latching;
- the chorus fades in smoothly;
- leaving or significant agitation releases the latch using hysteresis rather than flickering;
- dashboard shutdown transfers control to device ID 1 after source timeout;
- dashboard restart retakes priority without a visible phase jump.

## 12. Fleet rollout and operations

- Flash a small batch before the entire fleet.
- Save the exact `.bin`, source revision, pin map, calibration factors, transforms, IDs, and current limits.
- Use sequential broadcast OTA; simultaneous updates can overload Wi-Fi and ESP32 memory.
- After OTA, confirm firmware version, roles, transform, fallback pattern, and sensor tuning. The schema migration preserves old role/position/pattern keys, but missing installation-specific source files cannot be inferred.
- Keep `fallback` and `blackout` modes available in the operator runbook.
