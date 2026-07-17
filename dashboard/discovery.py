"""mDNS discovery of ESP32 devices advertising the _esp32art._tcp.local service.

An ESPListener watches the network and keeps a shared device registry in sync as
devices appear, update their TXT records, or drop off.
"""

from __future__ import annotations

import socket
import time

from zeroconf import ServiceListener, Zeroconf

from device import Device

SERVICE_TYPE = "_esp32art._tcp.local."


def decode_txt(properties: dict[bytes, bytes | None]) -> dict[str, str]:
    out: dict[str, str] = {}
    for k, v in properties.items():
        key = k.decode(errors="ignore") if isinstance(k, bytes) else str(k)
        if v is None:
            out[key] = ""
        else:
            out[key] = v.decode(errors="ignore") if isinstance(v, bytes) else str(v)
    return out


class ESPListener(ServiceListener):
    def __init__(self, devices: dict[str, Device]) -> None:
        # The registry is owned by the app; the listener only populates it.
        self.devices = devices

    def update_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        self.add_service(zc, type_, name)

    def add_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        info = zc.get_service_info(type_, name, timeout=2000)
        if not info or not info.addresses:
            return

        ip = socket.inet_ntoa(info.addresses[0])
        txt = decode_txt(info.properties)
        device_name = txt.get("name") or name.replace(f".{SERVICE_TYPE}", "").replace(".local.", "")
        key = txt.get("chip") or txt.get("mac") or name

        d = self.devices.get(key) or Device(key=key, name=device_name, host=info.server, ip=ip, port=info.port or 80)
        d.name = device_name
        d.host = info.server.rstrip(".")
        d.ip = ip
        d.port = info.port or 80
        d.mac = txt.get("mac", d.mac)
        d.chip = txt.get("chip", d.chip)
        d.firmware = txt.get("fw", d.firmware)
        d.ssid = txt.get("ssid", d.ssid)
        if "id" in txt:
            d.set_device_id(txt["id"])
        if "rssi" in txt:
            try:
                d.set_rssi(int(txt["rssi"]))
            except ValueError:
                pass
        d.online = True
        d.last_seen = time.time()
        d.status_error = ""
        self.devices[key] = d

    def remove_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        for d in self.devices.values():
            if name.startswith(d.name):
                d.online = False
