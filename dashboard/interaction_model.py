"""Reference installation-state model shared by the dashboard and tests."""

from __future__ import annotations

import json
import math
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any


@dataclass(slots=True)
class PlatformTelemetry:
    platform_id: int
    x: float
    y: float
    z: float
    weight_kg: float
    agitation: float
    calmness: float
    occupied: bool
    sequence: int = 0
    last_seen_monotonic: float = field(default_factory=time.monotonic)

    def as_dict(self) -> dict[str, Any]:
        return {
            "id": self.platform_id,
            "x": self.x,
            "y": self.y,
            "z": self.z,
            "weightKg": self.weight_kg,
            "agitation": self.agitation,
            "calmness": self.calmness,
            "occupied": self.occupied,
            "sequence": self.sequence,
            "lastSeenAgo": round(time.monotonic() - self.last_seen_monotonic, 2),
        }


@dataclass(slots=True)
class ShowSettings:
    mode: str = "auto"
    master_brightness: float = 1.0
    expected_platform_count: int = 4
    influence_radius_meters: float = 5.0
    calm_threshold: float = 0.86
    calm_release_threshold: float = 0.68
    max_agitation_for_chorus: float = 0.16
    all_calm_hold_seconds: float = 12.0
    chorus_fade_seconds: float = 8.0
    activation_wave_speed_mps: float = 5.5
    activation_wave_width_meters: float = 0.65
    activation_wave_duration_seconds: float = 5.0
    map_min_x: float = -10.0
    map_max_x: float = 10.0
    map_min_z: float = -10.0
    map_max_z: float = 10.0
    audio_enabled: bool = False
    audio_host: str = "127.0.0.1"
    audio_port: int = 9000

    def normalized(self) -> "ShowSettings":
        self.mode = self.mode if self.mode in {"auto", "fallback", "forest", "chorus", "blackout"} else "auto"
        self.master_brightness = _clamp(self.master_brightness)
        self.expected_platform_count = max(1, min(12, int(self.expected_platform_count)))
        self.influence_radius_meters = max(0.25, float(self.influence_radius_meters))
        self.calm_threshold = _clamp(self.calm_threshold)
        self.calm_release_threshold = min(
            self.calm_threshold, _clamp(self.calm_release_threshold)
        )
        self.max_agitation_for_chorus = _clamp(self.max_agitation_for_chorus)
        self.all_calm_hold_seconds = max(0.0, float(self.all_calm_hold_seconds))
        self.chorus_fade_seconds = max(0.1, float(self.chorus_fade_seconds))
        self.activation_wave_speed_mps = max(0.1, float(self.activation_wave_speed_mps))
        self.activation_wave_width_meters = max(0.05, float(self.activation_wave_width_meters))
        self.activation_wave_duration_seconds = max(0.5, float(self.activation_wave_duration_seconds))
        self.audio_port = max(1, min(65535, int(self.audio_port)))
        if self.map_max_x <= self.map_min_x:
            self.map_max_x = self.map_min_x + 1.0
        if self.map_max_z <= self.map_min_z:
            self.map_max_z = self.map_min_z + 1.0
        return self

    def public_dict(self) -> dict[str, Any]:
        return {
            "mode": self.mode,
            "masterBrightness": self.master_brightness,
            "expectedPlatformCount": self.expected_platform_count,
            "influenceRadiusMeters": self.influence_radius_meters,
            "calmThreshold": self.calm_threshold,
            "calmReleaseThreshold": self.calm_release_threshold,
            "maxAgitationForChorus": self.max_agitation_for_chorus,
            "allCalmHoldSeconds": self.all_calm_hold_seconds,
            "chorusFadeSeconds": self.chorus_fade_seconds,
            "activationWaveSpeedMetersPerSecond": self.activation_wave_speed_mps,
            "activationWaveWidthMeters": self.activation_wave_width_meters,
            "activationWaveDurationSeconds": self.activation_wave_duration_seconds,
            "mapMinX": self.map_min_x,
            "mapMaxX": self.map_max_x,
            "mapMinZ": self.map_min_z,
            "mapMaxZ": self.map_max_z,
            "audioEnabled": self.audio_enabled,
            "audioHost": self.audio_host,
            "audioPort": self.audio_port,
        }

    @classmethod
    def from_public_dict(cls, data: dict[str, Any]) -> "ShowSettings":
        return cls(
            mode=str(data.get("mode", "auto")),
            master_brightness=float(data.get("masterBrightness", 1.0)),
            expected_platform_count=int(data.get("expectedPlatformCount", 4)),
            influence_radius_meters=float(data.get("influenceRadiusMeters", 5.0)),
            calm_threshold=float(data.get("calmThreshold", 0.86)),
            calm_release_threshold=float(data.get("calmReleaseThreshold", 0.68)),
            max_agitation_for_chorus=float(data.get("maxAgitationForChorus", 0.16)),
            all_calm_hold_seconds=float(data.get("allCalmHoldSeconds", 12.0)),
            chorus_fade_seconds=float(data.get("chorusFadeSeconds", 8.0)),
            activation_wave_speed_mps=float(data.get("activationWaveSpeedMetersPerSecond", 5.5)),
            activation_wave_width_meters=float(data.get("activationWaveWidthMeters", 0.65)),
            activation_wave_duration_seconds=float(data.get("activationWaveDurationSeconds", 5.0)),
            map_min_x=float(data.get("mapMinX", -10.0)),
            map_max_x=float(data.get("mapMaxX", 10.0)),
            map_min_z=float(data.get("mapMinZ", -10.0)),
            map_max_z=float(data.get("mapMaxZ", 10.0)),
            audio_enabled=bool(data.get("audioEnabled", False)),
            audio_host=str(data.get("audioHost", "127.0.0.1")),
            audio_port=int(data.get("audioPort", 9000)),
        ).normalized()


@dataclass(slots=True)
class InstallationSnapshot:
    seen_platforms: int = 0
    occupied_platforms: int = 0
    mean_calmness: float = 0.0
    minimum_calmness: float = 0.0
    maximum_agitation: float = 0.0
    chorus: float = 0.0
    all_calm: bool = False
    chorus_started_show_time_ms: int = 0
    sequence: int = 0

    def public_dict(self) -> dict[str, Any]:
        return {
            "seenPlatforms": self.seen_platforms,
            "occupiedPlatforms": self.occupied_platforms,
            "meanCalmness": self.mean_calmness,
            "minimumCalmness": self.minimum_calmness,
            "maximumAgitation": self.maximum_agitation,
            "chorus": self.chorus,
            "allCalm": self.all_calm,
            "chorusStartedShowTimeMs": self.chorus_started_show_time_ms,
            "sequence": self.sequence,
        }


class InstallationModel:
    def __init__(self, settings: ShowSettings) -> None:
        self.settings = settings
        self.platforms: dict[int, PlatformTelemetry] = {}
        self.snapshot = InstallationSnapshot()
        self._candidate_since: float | None = None
        self._last_update = time.monotonic()

    def ingest(self, platform: PlatformTelemetry) -> None:
        if platform.platform_id <= 0:
            return
        platform.agitation = _clamp(platform.agitation)
        platform.calmness = _clamp(platform.calmness)
        platform.last_seen_monotonic = time.monotonic()
        current = self.platforms.get(platform.platform_id)
        if current and platform.sequence < current.sequence and time.monotonic() - current.last_seen_monotonic < 1.0:
            return
        self.platforms[platform.platform_id] = platform

    def update(self, now: float | None = None, show_time_ms: int | None = None) -> InstallationSnapshot:
        now = time.monotonic() if now is None else now
        show_time_ms = int(now * 1000) if show_time_ms is None else show_time_ms
        dt = max(0.0, min(0.5, now - self._last_update))
        self._last_update = now

        self.platforms = {
            platform_id: platform
            for platform_id, platform in self.platforms.items()
            if now - platform.last_seen_monotonic <= 3.0
        }
        visible = list(self.platforms.values())
        occupied = [platform for platform in visible if platform.occupied]

        seen_count = len(visible)
        occupied_count = len(occupied)
        mean_calm = sum(item.calmness for item in occupied) / occupied_count if occupied else 0.0
        minimum_calm = min((item.calmness for item in occupied), default=0.0)
        maximum_agitation = max((item.agitation for item in occupied), default=0.0)

        expected = self.settings.expected_platform_count
        every_visible_platform_occupied = (
            seen_count > 0 and occupied_count == seen_count
        )
        candidate = (
            seen_count >= expected
            and every_visible_platform_occupied
            and minimum_calm >= self.settings.calm_threshold
            and maximum_agitation <= self.settings.max_agitation_for_chorus
        )

        if candidate:
            self._candidate_since = self._candidate_since or now
            if (
                not self.snapshot.all_calm
                and now - self._candidate_since >= self.settings.all_calm_hold_seconds
            ):
                self.snapshot.all_calm = True
                self.snapshot.chorus_started_show_time_ms = show_time_ms
        else:
            self._candidate_since = None
            release = (
                occupied_count < expected
                or occupied_count < seen_count
                or minimum_calm < self.settings.calm_release_threshold
                or maximum_agitation > min(1.0, self.settings.max_agitation_for_chorus * 1.8)
            )
            if release:
                self.snapshot.all_calm = False

        chorus_target = 1.0 if self.snapshot.all_calm else 0.0
        fade_seconds = self.settings.chorus_fade_seconds * (1.0 if chorus_target else 0.55)
        alpha = 1.0 - math.exp(-dt / max(0.1, fade_seconds))
        self.snapshot.chorus += (chorus_target - self.snapshot.chorus) * alpha
        self.snapshot.chorus = _clamp(self.snapshot.chorus)

        self.snapshot.seen_platforms = seen_count
        self.snapshot.occupied_platforms = occupied_count
        self.snapshot.mean_calmness = mean_calm
        self.snapshot.minimum_calmness = minimum_calm
        self.snapshot.maximum_agitation = maximum_agitation
        self.snapshot.sequence += 1
        return self.snapshot


def speaker_gains(x: float, z: float, settings: ShowSettings) -> tuple[float, float, float, float]:
    """Equal-power bilinear gains for front-left, front-right, rear-left, rear-right."""
    u = _clamp((x - settings.map_min_x) / (settings.map_max_x - settings.map_min_x))
    v = _clamp((z - settings.map_min_z) / (settings.map_max_z - settings.map_min_z))
    weights = (
        (1.0 - u) * (1.0 - v),
        u * (1.0 - v),
        (1.0 - u) * v,
        u * v,
    )
    return tuple(math.sqrt(max(0.0, value)) for value in weights)


def load_settings(path: Path) -> ShowSettings:
    if not path.exists():
        return ShowSettings()
    try:
        return ShowSettings.from_public_dict(json.loads(path.read_text(encoding="utf-8")))
    except (OSError, ValueError, TypeError):
        return ShowSettings()


def save_settings(path: Path, settings: ShowSettings) -> None:
    path.write_text(json.dumps(settings.public_dict(), indent=2), encoding="utf-8")


def _clamp(value: float) -> float:
    return max(0.0, min(1.0, float(value)))
