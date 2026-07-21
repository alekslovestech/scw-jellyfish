from __future__ import annotations

import json

from dashboard.conductor import PROTOCOL_VERSION, ShowConductor


def test_show_packet_contains_every_global_interaction_parameter(tmp_path) -> None:
    conductor = ShowConductor(tmp_path / "settings.json")
    packet = conductor.show_packet()
    expected = {
        "mode",
        "masterBrightness",
        "expectedPlatformCount",
        "influenceRadiusMeters",
        "calmThreshold",
        "calmReleaseThreshold",
        "maxAgitationForChorus",
        "allCalmHoldSeconds",
        "chorusFadeSeconds",
        "activationWaveSpeedMetersPerSecond",
        "activationWaveWidthMeters",
        "activationWaveDurationSeconds",
    }
    assert packet["protocol"] == PROTOCOL_VERSION
    assert expected <= packet.keys()


def test_malformed_or_wrong_protocol_udp_packets_are_ignored(tmp_path) -> None:
    conductor = ShowConductor(tmp_path / "settings.json")
    conductor.on_datagram(b"not-json", ("127.0.0.1", 1))
    conductor.on_datagram(
        json.dumps({"protocol": 99, "type": "platform", "id": 1}).encode(),
        ("127.0.0.1", 1),
    )
    conductor.on_datagram(
        json.dumps({"protocol": PROTOCOL_VERSION, "type": "platform", "id": "bad"}).encode(),
        ("127.0.0.1", 1),
    )
    assert not conductor.model.platforms


def test_activation_ids_are_deduplicated(tmp_path) -> None:
    conductor = ShowConductor(tmp_path / "settings.json")
    packet = {
        "protocol": PROTOCOL_VERSION,
        "type": "activation",
        "eventId": 12345,
        "platformId": 2,
        "x": 1.0,
        "y": 5.0,
        "z": -2.0,
        "weightKg": 70.0,
        "agitation": 0.2,
    }
    raw = json.dumps(packet).encode()
    conductor.on_datagram(raw, ("127.0.0.1", 1))
    conductor.on_datagram(raw, ("127.0.0.1", 1))
    assert len(conductor.events) == 1
