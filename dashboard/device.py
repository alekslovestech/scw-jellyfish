"""Device model and tolerant status parsing for the Jellyfish Forest dashboard."""

from __future__ import annotations

import time
from dataclasses import dataclass, field
from typing import Any

DEVICE_TIMEOUT_SECONDS = 8.0


def _float(value: Any, fallback: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return fallback


def _int(value: Any, fallback: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return fallback


@dataclass(slots=True)
class Device:
    key: str
    name: str
    host: str
    ip: str
    port: int = 80
    service_name: str = ""
    mac: str = ""
    chip: str = ""
    firmware: str = ""
    protocol: int = 0
    ssid: str = ""
    device_id: int = 0
    rssi: int | None = None
    signal_quality: str = ""
    last_seen: float = field(default_factory=time.time)
    online: bool = False
    status_error: str = ""

    number: float | None = None
    has_number: bool = False
    has_scale: bool = False
    is_jelly: bool = False
    is_big: bool = False
    pattern: str = "demo"
    show_mode: str = "auto"
    local_show_override: bool = False
    local_override_mode: str = "fallback"
    scene: str = "unknown"
    pos_x: float = 0.0
    pos_y: float = 0.0
    pos_z: float = 0.0
    rotation_y: float = 0.0
    master_brightness: float = 1.0
    power_limit_milliamps: int = 12000
    clock_synchronized: bool = False
    pattern_parameters: dict[str, float] = field(default_factory=dict)
    sensor: dict[str, Any] = field(default_factory=dict)
    sensor_tuning: dict[str, Any] = field(default_factory=dict)
    interaction_tuning: dict[str, Any] = field(default_factory=dict)
    local_field: dict[str, Any] = field(default_factory=dict)
    installation: dict[str, Any] = field(default_factory=dict)

    @property
    def base_url(self) -> str:
        return f"http://{self.ip}:{self.port}"

    def update_status(self, data: dict[str, Any]) -> None:
        self.online = True
        self.last_seen = time.time()
        self.status_error = ""
        self.number = data.get("number", self.number)
        self.has_number = bool(data.get("hasNumber", self.has_number))
        self.name = str(data.get("name") or self.name)
        self.mac = str(data.get("mac") or self.mac)
        self.chip = str(data.get("chip") or self.chip)
        self.firmware = str(data.get("firmware") or self.firmware)
        self.protocol = _int(data.get("protocol"), self.protocol)
        self.ssid = str(data.get("ssid") or self.ssid)
        self.device_id = _int(data.get("id"), self.device_id)
        self.has_scale = bool(data.get("hasScale", self.has_scale))
        self.is_jelly = bool(data.get("isJelly", self.is_jelly))
        self.is_big = bool(data.get("isBig", self.is_big))
        self.pattern = str(data.get("pattern") or self.pattern)
        self.show_mode = str(data.get("showMode") or self.show_mode)
        self.local_show_override = bool(
            data.get("localShowOverride", self.local_show_override)
        )
        self.local_override_mode = str(
            data.get("localOverrideMode") or self.local_override_mode
        )
        self.scene = str(data.get("scene") or self.scene)
        self.pos_x = _float(data.get("posX"), self.pos_x)
        self.pos_y = _float(data.get("posY"), self.pos_y)
        self.pos_z = _float(data.get("posZ"), self.pos_z)
        self.rotation_y = _float(data.get("rotationY"), self.rotation_y)
        self.master_brightness = _float(data.get("masterBrightness"), self.master_brightness)
        self.power_limit_milliamps = _int(
            data.get("powerLimitMilliAmps"), self.power_limit_milliamps
        )
        self.clock_synchronized = bool(
            data.get("clockSynchronized", self.clock_synchronized)
        )
        if isinstance(data.get("patternParameters"), dict):
            self.pattern_parameters = {
                str(key): _float(value)
                for key, value in data["patternParameters"].items()
            }
        if isinstance(data.get("sensor"), dict):
            self.sensor = dict(data["sensor"])
        if isinstance(data.get("sensorTuning"), dict):
            self.sensor_tuning = dict(data["sensorTuning"])
        if isinstance(data.get("interactionTuning"), dict):
            self.interaction_tuning = dict(data["interactionTuning"])
        if isinstance(data.get("field"), dict):
            self.local_field = dict(data["field"])
        if isinstance(data.get("installation"), dict):
            self.installation = dict(data["installation"])
        if data.get("rssi") is not None:
            self.rssi = _int(data.get("rssi"))
            self.signal_quality = rssi_quality(self.rssi)

    def is_online(self) -> bool:
        return self.online and time.time() - self.last_seen < DEVICE_TIMEOUT_SECONDS

    def as_dict(self) -> dict[str, Any]:
        return {
            "key": self.key,
            "name": self.name,
            "host": self.host,
            "ip": self.ip,
            "port": self.port,
            "url": self.base_url,
            "mac": self.mac,
            "chip": self.chip,
            "firmware": self.firmware,
            "protocol": self.protocol,
            "ssid": self.ssid,
            "id": self.device_id,
            "rssi": self.rssi,
            "signalQuality": self.signal_quality,
            "lastSeenAgo": round(time.time() - self.last_seen, 1),
            "online": self.is_online(),
            "number": self.number,
            "hasNumber": self.has_number,
            "statusError": self.status_error,
            "hasScale": self.has_scale,
            "isJelly": self.is_jelly,
            "isBig": self.is_big,
            "pattern": self.pattern,
            "showMode": self.show_mode,
            "localShowOverride": self.local_show_override,
            "localOverrideMode": self.local_override_mode,
            "scene": self.scene,
            "posX": self.pos_x,
            "posY": self.pos_y,
            "posZ": self.pos_z,
            "rotationY": self.rotation_y,
            "masterBrightness": self.master_brightness,
            "powerLimitMilliAmps": self.power_limit_milliamps,
            "clockSynchronized": self.clock_synchronized,
            "patternParameters": self.pattern_parameters,
            "sensor": self.sensor,
            "sensorTuning": self.sensor_tuning,
            "interactionTuning": self.interaction_tuning,
            "field": self.local_field,
            "installation": self.installation,
        }


def rssi_quality(rssi: int | None) -> str:
    if rssi is None:
        return ""
    if rssi >= -55:
        return "Excellent"
    if rssi >= -65:
        return "Good"
    if rssi >= -75:
        return "Fair"
    return "Weak"
