"""UDP show conductor, platform event receiver, and optional OSC audio bridge."""

from __future__ import annotations

import asyncio
import json
import socket
import time
from collections import deque
from pathlib import Path
from typing import Any

from .interaction_model import (
    InstallationModel,
    PlatformTelemetry,
    ShowSettings,
    load_settings,
    save_settings,
    speaker_gains,
)

SHOW_UDP_PORT = 42120
PROTOCOL_VERSION = 2

try:
    from pythonosc.udp_client import SimpleUDPClient
except ImportError:  # The dashboard remains usable without audio integration.
    SimpleUDPClient = None  # type: ignore[assignment]


class _Protocol(asyncio.DatagramProtocol):
    def __init__(self, owner: "ShowConductor") -> None:
        self.owner = owner

    def datagram_received(self, data: bytes, addr: tuple[str, int]) -> None:
        self.owner.on_datagram(data, addr)


class ShowConductor:
    def __init__(self, settings_path: Path) -> None:
        self.settings_path = settings_path
        self.settings = load_settings(settings_path)
        self.model = InstallationModel(self.settings)
        self.events: deque[dict[str, Any]] = deque(maxlen=60)
        self._seen_event_ids: dict[int, float] = {}
        self.transport: asyncio.DatagramTransport | None = None
        self.task: asyncio.Task[None] | None = None
        self.audio_client: Any = None
        self.started_monotonic = time.monotonic()

    async def start(self) -> None:
        loop = asyncio.get_running_loop()
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.bind(("0.0.0.0", SHOW_UDP_PORT))
        sock.setblocking(False)
        transport, _ = await loop.create_datagram_endpoint(
            lambda: _Protocol(self), sock=sock
        )
        self.transport = transport
        self._configure_audio()
        self._send_show_audio()
        self.task = asyncio.create_task(self._run(), name="show-conductor")

    async def stop(self) -> None:
        if self.task:
            self.task.cancel()
            try:
                await self.task
            except asyncio.CancelledError:
                pass
        if self.transport:
            self.transport.close()

    def _configure_audio(self) -> None:
        self.audio_client = None
        if (
            self.settings.audio_enabled
            and SimpleUDPClient is not None
            and self.settings.audio_host
        ):
            self.audio_client = SimpleUDPClient(
                self.settings.audio_host, self.settings.audio_port
            )

    def update_settings(self, updates: dict[str, Any]) -> ShowSettings:
        data = self.settings.public_dict()
        data.update(updates)
        self.settings = ShowSettings.from_public_dict(data)
        self.model.settings = self.settings
        save_settings(self.settings_path, self.settings)
        self._configure_audio()
        self._send_show_audio()
        return self.settings

    def _accept_event(self, event_id: int) -> bool:
        if event_id <= 0:
            return False
        now = time.monotonic()
        self._seen_event_ids = {
            key: seen_at
            for key, seen_at in self._seen_event_ids.items()
            if now - seen_at < 30.0
        }
        if event_id in self._seen_event_ids:
            return False
        self._seen_event_ids[event_id] = now
        return True

    def on_datagram(self, raw: bytes, addr: tuple[str, int]) -> None:
        try:
            packet = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return
        if not isinstance(packet, dict):
            return
        protocol = packet.get("protocol")
        if protocol is not None and protocol != PROTOCOL_VERSION:
            return

        packet_type = packet.get("type")
        try:
            if packet_type == "platform":
                platform = PlatformTelemetry(
                    platform_id=int(packet.get("id", 0)),
                    x=float(packet.get("x", 0.0)),
                    y=float(packet.get("y", 0.0)),
                    z=float(packet.get("z", 0.0)),
                    weight_kg=float(packet.get("weightKg", 0.0)),
                    agitation=float(packet.get("agitation", 0.0)),
                    calmness=float(packet.get("calmness", 0.0)),
                    occupied=bool(packet.get("occupied", False)),
                    sequence=int(packet.get("sequence", 0)),
                )
                self.model.ingest(platform)
                self._send_platform_audio(platform)
            elif packet_type == "activation":
                event_id = int(packet.get("eventId", 0))
                if not self._accept_event(event_id):
                    return
                event = {
                    "eventId": event_id,
                    "platformId": int(packet.get("platformId", 0)),
                    "x": float(packet.get("x", 0.0)),
                    "y": float(packet.get("y", 0.0)),
                    "z": float(packet.get("z", 0.0)),
                    "weightKg": float(packet.get("weightKg", 0.0)),
                    "agitation": float(packet.get("agitation", 0.0)),
                    "receivedAt": time.time(),
                    "source": addr[0],
                }
                event["speakerGains"] = speaker_gains(
                    event["x"], event["z"], self.settings
                )
                self.events.appendleft(event)
                self._send_activation_audio(event)
        except (TypeError, ValueError):
            return

    def ingest_device_status(self, data: dict[str, Any]) -> None:
        """HTTP polling fallback when broadcast reception is unavailable."""
        if not data.get("hasScale") or not data.get("id"):
            return
        sensor = data.get("sensor") or {}
        existing = self.model.platforms.get(int(data["id"]))
        if existing and time.monotonic() - existing.last_seen_monotonic < 0.7:
            return
        self.model.ingest(
            PlatformTelemetry(
                platform_id=int(data["id"]),
                x=float(data.get("posX", 0.0)),
                y=float(data.get("posY", 0.0)),
                z=float(data.get("posZ", 0.0)),
                weight_kg=float(sensor.get("weightKg", 0.0)),
                agitation=float(sensor.get("agitation", 0.0)),
                calmness=float(sensor.get("calmness", 0.0)),
                occupied=bool(sensor.get("occupied", False)),
                sequence=int(time.monotonic() * 10),
            )
        )

    async def _run(self) -> None:
        while True:
            now = time.monotonic()
            show_time_ms = int((now - self.started_monotonic) * 1000)
            snapshot = self.model.update(now, show_time_ms)
            self._send_installation_audio(snapshot)
            self._broadcast(
                {
                    "protocol": PROTOCOL_VERSION,
                    "type": "clock",
                    "source": "dashboard",
                    "priority": 100,
                    "showTimeMs": show_time_ms,
                }
            )
            installation = snapshot.public_dict()
            installation.update(
                {
                    "protocol": PROTOCOL_VERSION,
                    "type": "installation",
                    "source": "dashboard",
                    "priority": 100,
                    "showTimeMs": show_time_ms,
                }
            )
            self._broadcast(installation)
            self._broadcast(self.show_packet())
            await asyncio.sleep(0.25)

    def show_packet(self) -> dict[str, Any]:
        return {
            "protocol": PROTOCOL_VERSION,
            "type": "show",
            "source": "dashboard",
            "priority": 100,
            "mode": self.settings.mode,
            "masterBrightness": self.settings.master_brightness,
            "expectedPlatformCount": self.settings.expected_platform_count,
            "influenceRadiusMeters": self.settings.influence_radius_meters,
            "calmThreshold": self.settings.calm_threshold,
            "calmReleaseThreshold": self.settings.calm_release_threshold,
            "maxAgitationForChorus": self.settings.max_agitation_for_chorus,
            "allCalmHoldSeconds": self.settings.all_calm_hold_seconds,
            "chorusFadeSeconds": self.settings.chorus_fade_seconds,
            "activationWaveSpeedMetersPerSecond": self.settings.activation_wave_speed_mps,
            "activationWaveWidthMeters": self.settings.activation_wave_width_meters,
            "activationWaveDurationSeconds": self.settings.activation_wave_duration_seconds,
        }

    def trigger_wave(
        self,
        x: float,
        y: float,
        z: float,
        platform_id: int = 0,
        weight_kg: float = 0.0,
    ) -> dict[str, Any]:
        show_time_ms = int((time.monotonic() - self.started_monotonic) * 1000)
        event = {
            "protocol": PROTOCOL_VERSION,
            "type": "activation",
            "eventId": int(time.time_ns() & 0xFFFFFFFF) or 1,
            "platformId": platform_id,
            "x": x,
            "y": y,
            "z": z,
            "weightKg": weight_kg,
            "agitation": 0.0,
            "showTimeMs": show_time_ms,
        }
        self._broadcast(event)
        local_event = dict(event)
        local_event["receivedAt"] = time.time()
        local_event["source"] = "dashboard"
        local_event["speakerGains"] = speaker_gains(x, z, self.settings)
        self.events.appendleft(local_event)
        self._send_activation_audio(local_event)
        return local_event

    def _broadcast(self, packet: dict[str, Any]) -> None:
        if not self.transport:
            return
        self.transport.sendto(
            json.dumps(packet, separators=(",", ":")).encode("utf-8"),
            ("255.255.255.255", SHOW_UDP_PORT),
        )

    def _send_osc(self, address: str, values: list[Any]) -> None:
        if not self.audio_client:
            return
        try:
            self.audio_client.send_message(address, values)
        except OSError:
            # Audio is an optional bridge; a stopped audio engine must never
            # interrupt lighting or the dashboard conductor.
            pass

    def _send_activation_audio(self, event: dict[str, Any]) -> None:
        gains = event["speakerGains"]
        self._send_osc(
            "/jelly/activation",
            [
                event["platformId"],
                event["x"],
                event["y"],
                event["z"],
                event["weightKg"],
                event["agitation"],
                *gains,
            ],
        )

    def _send_platform_audio(self, platform: PlatformTelemetry) -> None:
        gains = speaker_gains(platform.x, platform.z, self.settings)
        self._send_osc(
            "/jelly/platform",
            [
                platform.platform_id,
                int(platform.occupied),
                platform.weight_kg,
                platform.agitation,
                platform.calmness,
                *gains,
            ],
        )

    def _send_installation_audio(self, snapshot: Any) -> None:
        self._send_osc(
            "/jelly/installation",
            [
                snapshot.seen_platforms,
                snapshot.occupied_platforms,
                snapshot.mean_calmness,
                snapshot.minimum_calmness,
                snapshot.maximum_agitation,
                snapshot.chorus,
                int(snapshot.all_calm),
            ],
        )

    def _send_show_audio(self) -> None:
        settings = self.settings
        self._send_osc(
            "/jelly/show",
            [
                settings.mode,
                settings.master_brightness,
                settings.expected_platform_count,
                settings.influence_radius_meters,
                settings.calm_threshold,
                settings.calm_release_threshold,
                settings.max_agitation_for_chorus,
                settings.all_calm_hold_seconds,
                settings.chorus_fade_seconds,
                settings.activation_wave_speed_mps,
                settings.activation_wave_width_meters,
                settings.activation_wave_duration_seconds,
            ],
        )

    def status(self) -> dict[str, Any]:
        snapshot = self.model.update()
        return {
            "settings": self.settings.public_dict(),
            "installation": snapshot.public_dict(),
            "platforms": [
                platform.as_dict()
                for platform in sorted(
                    self.model.platforms.values(), key=lambda item: item.platform_id
                )
            ],
            "events": list(self.events),
            "audioBridgeAvailable": SimpleUDPClient is not None,
            "audioBridgeActive": self.audio_client is not None,
        }
