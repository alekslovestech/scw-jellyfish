from __future__ import annotations

import math

from dashboard.interaction_model import (
    InstallationModel,
    PlatformTelemetry,
    ShowSettings,
    speaker_gains,
)


def platform(platform_id: int, *, calm: float, agitation: float, occupied: bool = True) -> PlatformTelemetry:
    return PlatformTelemetry(
        platform_id=platform_id,
        x=float(platform_id),
        y=4.0,
        z=0.0,
        weight_kg=70.0 if occupied else 0.0,
        agitation=agitation,
        calmness=calm,
        occupied=occupied,
        sequence=1,
        last_seen_monotonic=100.0,
    )


def test_chorus_requires_every_expected_platform_and_hold_time() -> None:
    settings = ShowSettings(
        expected_platform_count=3,
        calm_threshold=0.8,
        max_agitation_for_chorus=0.2,
        all_calm_hold_seconds=5.0,
    )
    model = InstallationModel(settings)
    model._last_update = 100.0
    for platform_id in range(1, 4):
        item = platform(platform_id, calm=0.9, agitation=0.05)
        model.platforms[platform_id] = item

    snapshot = model.update(now=100.0, show_time_ms=0)
    assert not snapshot.all_calm
    for item in model.platforms.values():
        item.last_seen_monotonic = 104.9
    snapshot = model.update(now=104.9, show_time_ms=4900)
    assert not snapshot.all_calm
    for item in model.platforms.values():
        item.last_seen_monotonic = 105.1
    snapshot = model.update(now=105.1, show_time_ms=5100)
    assert snapshot.all_calm
    assert snapshot.chorus_started_show_time_ms == 5100


def test_chorus_releases_quickly_when_platform_is_jostled() -> None:
    settings = ShowSettings(
        expected_platform_count=2,
        calm_threshold=0.8,
        calm_release_threshold=0.65,
        max_agitation_for_chorus=0.2,
        all_calm_hold_seconds=0.0,
    )
    model = InstallationModel(settings)
    model._last_update = 100.0
    model.platforms = {
        1: platform(1, calm=0.92, agitation=0.02),
        2: platform(2, calm=0.91, agitation=0.03),
    }
    assert model.update(now=100.1, show_time_ms=100).all_calm
    model.platforms[2].agitation = 0.7
    model.platforms[2].last_seen_monotonic = 100.2
    assert not model.update(now=100.2, show_time_ms=200).all_calm


def test_unoccupied_platform_does_not_count_as_calm() -> None:
    settings = ShowSettings(expected_platform_count=2, all_calm_hold_seconds=0.0)
    model = InstallationModel(settings)
    model._last_update = 100.0
    model.platforms = {
        1: platform(1, calm=1.0, agitation=0.0),
        2: platform(2, calm=1.0, agitation=0.0, occupied=False),
    }
    snapshot = model.update(now=100.1, show_time_ms=100)
    assert snapshot.occupied_platforms == 1
    assert not snapshot.all_calm


def test_extra_visible_unoccupied_platform_blocks_collective_chorus() -> None:
    settings = ShowSettings(expected_platform_count=2, all_calm_hold_seconds=0.0)
    model = InstallationModel(settings)
    model._last_update = 100.0
    model.platforms = {
        1: platform(1, calm=1.0, agitation=0.0),
        2: platform(2, calm=1.0, agitation=0.0),
        3: platform(3, calm=0.0, agitation=0.0, occupied=False),
    }
    snapshot = model.update(now=100.1, show_time_ms=100)
    assert snapshot.seen_platforms == 3
    assert snapshot.occupied_platforms == 2
    assert not snapshot.all_calm


def test_speaker_gains_are_equal_power_and_spatial() -> None:
    settings = ShowSettings(map_min_x=-10, map_max_x=10, map_min_z=-10, map_max_z=10)
    center = speaker_gains(0, 0, settings)
    assert all(math.isclose(gain, 0.5, rel_tol=1e-6) for gain in center)
    assert math.isclose(sum(gain * gain for gain in center), 1.0, rel_tol=1e-6)

    front_left = speaker_gains(-10, -10, settings)
    assert front_left == (1.0, 0.0, 0.0, 0.0)
