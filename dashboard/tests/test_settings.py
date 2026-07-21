from dashboard.interaction_model import ShowSettings


def test_show_settings_normalize_invalid_values() -> None:
    settings = ShowSettings(
        mode="not-a-mode",
        master_brightness=2.0,
        expected_platform_count=99,
        influence_radius_meters=-2.0,
        calm_threshold=0.4,
        calm_release_threshold=0.9,
        audio_port=99999,
    ).normalized()
    assert settings.mode == "auto"
    assert settings.master_brightness == 1.0
    assert settings.expected_platform_count == 12
    assert settings.influence_radius_meters == 0.25
    assert settings.calm_threshold == 0.4
    assert settings.calm_release_threshold == 0.4
    assert settings.audio_port == 65535
