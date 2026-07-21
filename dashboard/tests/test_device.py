from __future__ import annotations

from dashboard.device import Device


def test_device_parses_legacy_flat_and_refactored_nested_status() -> None:
    device = Device(key="chip", name="old", host="jelly.local", ip="192.168.0.101")
    device.update_status(
        {
            "name": "jelly-one",
            "id": "1",
            "firmware": "2.0.0-refactor",
            "protocol": 2,
            "rssi": -63,
            "hasScale": True,
            "isJelly": True,
            "isBig": False,
            "pattern": "waterfall",
            "showMode": "auto",
            "localShowOverride": True,
            "localOverrideMode": "fallback",
            "scene": "legacy-fallback",
            "posX": "1.25",
            "posY": 4,
            "posZ": -2.5,
            "rotationY": "90",
            "patternParameters": {"speed": "0.6", "brightness": 0.8},
            "sensor": {"occupied": True, "weightKg": 68.2, "calmness": 0.7},
            "sensorTuning": {"calibrationFactor": -14000.0},
            "interactionTuning": {"expectedPlatformCount": 4},
            "field": {"harmony": 0.5},
            "installation": {"chorus": 0.25},
        }
    )

    payload = device.as_dict()
    assert payload["name"] == "jelly-one"
    assert payload["id"] == 1
    assert payload["signalQuality"] == "Good"
    assert payload["posX"] == 1.25
    assert payload["localShowOverride"] is True
    assert payload["localOverrideMode"] == "fallback"
    assert payload["patternParameters"]["speed"] == 0.6
    assert payload["sensor"]["occupied"] is True
    assert payload["sensorTuning"]["calibrationFactor"] == -14000.0
    assert payload["interactionTuning"]["expectedPlatformCount"] == 4


def test_malformed_optional_numbers_preserve_existing_values() -> None:
    device = Device(
        key="chip",
        name="jelly",
        host="jelly.local",
        ip="192.168.0.101",
        device_id=7,
        pos_x=3.5,
        power_limit_milliamps=9000,
    )
    device.update_status({"id": "bad", "posX": None, "powerLimitMilliAmps": "bad"})
    assert device.device_id == 7
    assert device.pos_x == 3.5
    assert device.power_limit_milliamps == 9000
