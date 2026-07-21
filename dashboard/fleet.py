"""mDNS discovery, status polling, and HTTP proxying for ESP32 controllers."""

from __future__ import annotations

import asyncio
import socket
import threading
import time
from collections.abc import Callable
from typing import Any

import httpx
from zeroconf import ServiceBrowser, ServiceListener, Zeroconf

from .device import DEVICE_TIMEOUT_SECONDS, Device, rssi_quality

SERVICE_TYPE = "_esp32art._tcp.local."
POLL_INTERVAL_SECONDS = 2.0
REQUEST_TIMEOUT_SECONDS = 2.5


def decode_txt(properties: dict[bytes, bytes | None]) -> dict[str, str]:
    decoded: dict[str, str] = {}
    for raw_key, raw_value in properties.items():
        key = raw_key.decode(errors="ignore") if isinstance(raw_key, bytes) else str(raw_key)
        value = "" if raw_value is None else (
            raw_value.decode(errors="ignore") if isinstance(raw_value, bytes) else str(raw_value)
        )
        decoded[key] = value
    return decoded


class FleetRegistry:
    def __init__(self) -> None:
        self._devices: dict[str, Device] = {}
        self._lock = threading.RLock()

    def upsert_discovery(
        self,
        *,
        key: str,
        name: str,
        host: str,
        ip: str,
        port: int,
        service_name: str,
        txt: dict[str, str],
    ) -> Device:
        with self._lock:
            device = self._devices.get(key)
            if device is None:
                device = Device(key=key, name=name, host=host, ip=ip, port=port)
                self._devices[key] = device
            device.name = name
            device.host = host.rstrip(".")
            device.ip = ip
            device.port = port
            device.service_name = service_name
            device.mac = txt.get("mac", device.mac)
            device.chip = txt.get("chip", device.chip)
            device.firmware = txt.get("fw", device.firmware)
            device.ssid = txt.get("ssid", device.ssid)
            if txt.get("id"):
                try:
                    device.device_id = int(txt["id"])
                except ValueError:
                    pass
            if txt.get("rssi"):
                try:
                    device.rssi = int(txt["rssi"])
                    device.signal_quality = rssi_quality(device.rssi)
                except ValueError:
                    pass
            device.online = True
            device.last_seen = time.time()
            device.status_error = ""
            return device

    def mark_service_removed(self, service_name: str) -> None:
        with self._lock:
            for device in self._devices.values():
                if device.service_name == service_name:
                    device.online = False

    def get(self, key: str) -> Device | None:
        with self._lock:
            return self._devices.get(key)

    def values(self) -> list[Device]:
        with self._lock:
            return list(self._devices.values())

    def sorted_dicts(self) -> list[dict[str, Any]]:
        devices = [device.as_dict() for device in self.values()]
        devices.sort(
            key=lambda item: (
                not item["online"],
                item.get("id", 0) == 0,
                item.get("id", 0) or 999,
                str(item.get("name", "")).lower(),
            )
        )
        return devices


class ESPListener(ServiceListener):
    def __init__(self, registry: FleetRegistry) -> None:
        self.registry = registry

    def update_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        self.add_service(zc, type_, name)

    def add_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        info = zc.get_service_info(type_, name, timeout=2000)
        if not info:
            return
        parsed = info.parsed_addresses() if hasattr(info, "parsed_addresses") else []
        ip = next((address for address in parsed if ":" not in address), "")
        if not ip and info.addresses:
            try:
                ip = socket.inet_ntoa(info.addresses[0])
            except (OSError, ValueError):
                return
        if not ip:
            return

        txt = decode_txt(info.properties)
        device_name = txt.get("name") or name.removesuffix(f".{SERVICE_TYPE}").removesuffix(".local.")
        key = txt.get("chip") or txt.get("mac") or name
        self.registry.upsert_discovery(
            key=key,
            name=device_name,
            host=info.server,
            ip=ip,
            port=info.port or 80,
            service_name=name,
            txt=txt,
        )

    def remove_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        del zc, type_
        self.registry.mark_service_removed(name)


class FleetManager:
    def __init__(
        self,
        status_sink: Callable[[dict[str, Any]], None] | None = None,
    ) -> None:
        self.registry = FleetRegistry()
        self.status_sink = status_sink
        self.zeroconf: Zeroconf | None = None
        self.browser: ServiceBrowser | None = None
        self.poll_task: asyncio.Task[None] | None = None
        self.client: httpx.AsyncClient | None = None

    async def start(self) -> None:
        self.zeroconf = Zeroconf()
        self.browser = ServiceBrowser(
            self.zeroconf, SERVICE_TYPE, ESPListener(self.registry)
        )
        self.client = httpx.AsyncClient(timeout=REQUEST_TIMEOUT_SECONDS)
        self.poll_task = asyncio.create_task(self._poll_loop(), name="fleet-poll")

    async def stop(self) -> None:
        if self.poll_task:
            self.poll_task.cancel()
            try:
                await self.poll_task
            except asyncio.CancelledError:
                pass
        if self.client:
            await self.client.aclose()
        if self.zeroconf:
            self.zeroconf.close()

    async def _poll_loop(self) -> None:
        semaphore = asyncio.Semaphore(12)
        while True:
            devices = self.registry.values()
            await asyncio.gather(
                *(self._poll_device(device, semaphore) for device in devices),
                return_exceptions=True,
            )
            await asyncio.sleep(POLL_INTERVAL_SECONDS)

    async def _poll_device(
        self, device: Device, semaphore: asyncio.Semaphore
    ) -> None:
        if not self.client:
            return
        async with semaphore:
            try:
                response = await self.client.get(f"{device.base_url}/status")
                if response.status_code == 404:
                    response = await self.client.get(f"{device.base_url}/number")
                response.raise_for_status()
                data = response.json()
                if not isinstance(data, dict):
                    raise ValueError("Status was not a JSON object")
                device.update_status(data)
                if self.status_sink:
                    self.status_sink(device.as_dict())
            except Exception as exc:  # Network errors are reflected in device state.
                if time.time() - device.last_seen > DEVICE_TIMEOUT_SECONDS:
                    device.online = False
                device.status_error = str(exc)

    async def request(
        self,
        device: Device,
        method: str,
        path: str,
        *,
        data: dict[str, str] | None = None,
        files: dict[str, tuple[str, bytes, str]] | None = None,
        timeout: float | None = None,
    ) -> dict[str, Any]:
        try:
            async with httpx.AsyncClient(
                timeout=timeout or REQUEST_TIMEOUT_SECONDS
            ) as client:
                response = await client.request(
                    method,
                    f"{device.base_url}{path}",
                    data=data,
                    files=files,
                )
            body: Any
            try:
                body = response.json()
            except ValueError:
                body = response.text[:20_000]
            return {
                "ok": response.status_code < 300,
                "http": response.status_code,
                "name": device.name,
                "body": body,
            }
        except Exception as exc:
            return {"ok": False, "name": device.name, "error": str(exc)}
