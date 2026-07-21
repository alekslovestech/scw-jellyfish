from __future__ import annotations

import numpy as np

from audio.soundscape import ShowControl, Soundscape


def test_soundscape_renders_finite_four_channel_audio() -> None:
    soundscape = Soundscape(sample_rate=48_000, master_level=0.7)
    soundscape.update_platform(
        platform_id=1,
        occupied=True,
        weight_kg=72.0,
        agitation=0.15,
        calmness=0.8,
        gains=(1.0, 0.0, 0.0, 0.0),
    )
    soundscape.update_installation(4, 4, 0.9, 0.86, 0.1, 0.7, True)
    soundscape.trigger_activation(1, 72.0, 0.15, (1.0, 0.0, 0.0, 0.0))
    block = soundscape.render(512)
    assert block.shape == (512, 4)
    assert block.dtype == np.float32
    assert np.isfinite(block).all()
    assert np.max(np.abs(block)) <= 1.0
    assert np.sqrt(np.mean(block[:, 0] ** 2)) > np.sqrt(np.mean(block[:, 1] ** 2))


def test_show_control_is_accepted_and_zero_frames_is_safe() -> None:
    soundscape = Soundscape()
    soundscape.update_show(ShowControl(mode="chorus", expected_platforms=6))
    block = soundscape.render(0)
    assert block.shape == (0, 4)
