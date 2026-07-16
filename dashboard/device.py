"""Device model for the ESP32 Fleet Dashboard."""

from __future__ import annotations

import time
from dataclasses import dataclass, field
from typing import Any

# A device is considered offline once its status has been stale this long.
DEVICE_TIMEOUT_SECONDS = 8.0


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
            "ssid": self.ssid,
            "id": self.device_id,
            "rssi": self.rssi,
            "signalQuality": self.signal_quality,
            "lastSeenAgo": round(time.time() - self.last_seen, 1),
            "online": self.online and (time.time() - self.last_seen < DEVICE_TIMEOUT_SECONDS),
            "number": self.number,
            "hasNumber": self.has_number,
            "statusError": self.status_error,
            "hasScale": self.has_scale,
            "isJelly": self.is_jelly,
            "isBig": self.is_big,
            "pattern": self.pattern,
            "posX": self.posX,
            "posY": self.posY,
            "posZ": self.posZ,
        }