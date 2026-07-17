"""Device model for the ESP32 Fleet Dashboard."""

from __future__ import annotations

import time
from collections.abc import Callable, Mapping
from dataclasses import dataclass, field
from typing import Any, NamedTuple

# A device is considered offline once its status has been stale this long.
DEVICE_TIMEOUT_SECONDS = 8.0

# Raw JSON received from an ESP /status response (read-only, keys may be absent).
StatusPayload = Mapping[str, Any]
# A device serialized to the wire format the dashboard frontend consumes.
DeviceDict = dict[str, Any]
# Converts a raw JSON value to the type stored on the Device.
Caster = Callable[[Any], Any]


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


class StatusField(NamedTuple):
    """One field synced between a Device attribute and the ESP JSON payload."""

    attr: str  # Device attribute name
    json_key: str  # key in the ESP JSON / wire format
    cast: Caster | None = None  # applied on read; None means pass through


# Fields synced with the ESP /status payload, listed once as the single source of
# truth for both reading (apply_status) and serializing (as_dict).
STATUS_FIELDS: list[StatusField] = [
    StatusField("name", "name"),
    StatusField("mac", "mac"),
    StatusField("chip", "chip"),
    StatusField("firmware", "firmware"),
    StatusField("ssid", "ssid"),
    StatusField("number", "number"),
    StatusField("has_number", "hasNumber", bool),
    StatusField("has_scale", "hasScale", bool),
    StatusField("is_jelly", "isJelly", bool),
    StatusField("is_big", "isBig", bool),
    StatusField("pattern", "pattern"),
    StatusField("posX", "posX", float),
    StatusField("posY", "posY", float),
    StatusField("posZ", "posZ", float),
]


@dataclass
class Device:
    key: str
    name: str
    host: str
    ip: str
    port: int = 80
    mac: str = ""
    chip: str = ""
    firmware: str = ""
    ssid: str = ""
    device_id: int = 0
    rssi: int | None = None
    signal_quality: str = ""
    last_seen: float = field(default_factory=time.time)
    online: bool = False
    number: float | None = None
    has_number: bool = False
    status_error: str = ""
    has_scale: bool = False
    is_jelly: bool = False
    is_big: bool = False
    pattern: str = "demo"
    posX: float = 0.0
    posY: float = 0.0
    posZ: float = 0.0

    @property
    def base_url(self) -> str:
        # Use IP for actual requests because .local resolution can vary per OS.
        return f"http://{self.ip}:{self.port}"

    def set_rssi(self, rssi: int) -> None:
        self.rssi = rssi
        self.signal_quality = rssi_quality(rssi)

    def set_device_id(self, value: Any) -> None:
        try:
            self.device_id = int(value)
        except (TypeError, ValueError):
            pass

    def apply_status(self, data: StatusPayload) -> None:
        """Update mutable fields from a /status (or /number) JSON payload."""
        self.online = True
        self.last_seen = time.time()
        self.status_error = ""

        for f in STATUS_FIELDS:
            if f.json_key in data:
                value = data[f.json_key]
                setattr(self, f.attr, f.cast(value) if f.cast else value)

        if data.get("id") is not None:
            self.set_device_id(data["id"])
        if data.get("rssi") is not None:
            self.set_rssi(int(data["rssi"]))

    def as_dict(self) -> DeviceDict:
        now = time.time()
        data: DeviceDict = {
            "key": self.key,
            "host": self.host,
            "ip": self.ip,
            "port": self.port,
            "url": self.base_url,
            "id": self.device_id,
            "rssi": self.rssi,
            "signalQuality": self.signal_quality,
            "statusError": self.status_error,
            "lastSeenAgo": round(now - self.last_seen, 1),
            "online": self.online and (now - self.last_seen < DEVICE_TIMEOUT_SECONDS),
        }
        for f in STATUS_FIELDS:
            data[f.json_key] = getattr(self, f.attr)
        return data