"""Pure DSP/state model for the optional four-channel Jellyfish Forest soundscape."""

from __future__ import annotations

import math
import threading
from dataclasses import dataclass, field
from typing import Iterable

import numpy as np


def _clamp(value: float, low: float = 0.0, high: float = 1.0) -> float:
    return max(low, min(high, float(value)))


def _normalize_gains(values: Iterable[float]) -> tuple[float, float, float, float]:
    gains = np.asarray(tuple(values), dtype=np.float64)
    if gains.shape != (4,):
        return (0.5, 0.5, 0.5, 0.5)
    gains = np.maximum(gains, 0.0)
    power = float(np.sqrt(np.sum(gains * gains)))
    if power < 1e-9:
        return (0.5, 0.5, 0.5, 0.5)
    gains /= power
    return tuple(float(value) for value in gains)


@dataclass(slots=True)
class PlatformControl:
    platform_id: int
    occupied: bool
    weight_kg: float
    agitation: float
    calmness: float
    gains: tuple[float, float, float, float]


@dataclass(slots=True)
class ActivationVoice:
    platform_id: int
    start_sample: int
    weight_kg: float
    agitation: float
    gains: tuple[float, float, float, float]
    seed: float


@dataclass(slots=True)
class InstallationControl:
    seen_platforms: int = 0
    occupied_platforms: int = 0
    mean_calmness: float = 0.0
    minimum_calmness: float = 0.0
    maximum_agitation: float = 0.0
    chorus: float = 0.0
    all_calm: bool = False


@dataclass(slots=True)
class ShowControl:
    mode: str = "auto"
    master_brightness: float = 1.0
    expected_platforms: int = 4
    influence_radius_meters: float = 5.0
    calm_threshold: float = 0.86
    calm_release_threshold: float = 0.68
    max_agitation_for_chorus: float = 0.16
    all_calm_hold_seconds: float = 12.0
    chorus_fade_seconds: float = 8.0
    activation_wave_speed_mps: float = 5.5
    activation_wave_width_meters: float = 0.65
    activation_wave_duration_seconds: float = 5.0


@dataclass(slots=True)
class AudioFeatures:
    level: float = 0.0
    bass: float = 0.0
    mid: float = 0.0
    high: float = 0.0
    beat: bool = False


class Soundscape:
    """Thread-safe, deterministic four-channel generative soundscape.

    The renderer deliberately avoids external DSP libraries. It is intended as a
    reference engine and integration test target; tune levels and speaker order on
    the installation's actual interface before public use.
    """

    def __init__(self, sample_rate: int = 48_000, master_level: float = 0.7) -> None:
        if sample_rate < 8_000:
            raise ValueError("sample_rate must be at least 8000 Hz")
        self.sample_rate = int(sample_rate)
        self.master_level = _clamp(master_level)
        self._lock = threading.RLock()
        self._sample_index = 0
        self._platforms: dict[int, PlatformControl] = {}
        self._voices: list[ActivationVoice] = []
        self._installation = InstallationControl()
        self._show = ShowControl()
        self._noise_state = np.zeros(4, dtype=np.float64)
        self._features = AudioFeatures()
        self._beat_until_sample = 0

    def update_platform(
        self,
        platform_id: int,
        occupied: bool,
        weight_kg: float,
        agitation: float,
        calmness: float,
        gains: Iterable[float],
    ) -> None:
        if platform_id <= 0:
            return
        control = PlatformControl(
            platform_id=int(platform_id),
            occupied=bool(occupied),
            weight_kg=max(0.0, float(weight_kg)),
            agitation=_clamp(agitation),
            calmness=_clamp(calmness),
            gains=_normalize_gains(gains),
        )
        with self._lock:
            self._platforms[control.platform_id] = control

    def trigger_activation(
        self,
        platform_id: int,
        weight_kg: float,
        agitation: float,
        gains: Iterable[float],
    ) -> None:
        with self._lock:
            seed = ((int(platform_id) * 0.173) + (len(self._voices) * 0.317)) % 1.0
            self._voices.append(
                ActivationVoice(
                    platform_id=int(platform_id),
                    start_sample=self._sample_index,
                    weight_kg=max(0.0, float(weight_kg)),
                    agitation=_clamp(agitation),
                    gains=_normalize_gains(gains),
                    seed=seed,
                )
            )
            self._voices = self._voices[-16:]
            self._beat_until_sample = self._sample_index + int(0.12 * self.sample_rate)

    def update_installation(
        self,
        seen_platforms: int,
        occupied_platforms: int,
        mean_calmness: float,
        minimum_calmness: float,
        maximum_agitation: float,
        chorus: float,
        all_calm: bool,
    ) -> None:
        with self._lock:
            self._installation = InstallationControl(
                seen_platforms=max(0, int(seen_platforms)),
                occupied_platforms=max(0, int(occupied_platforms)),
                mean_calmness=_clamp(mean_calmness),
                minimum_calmness=_clamp(minimum_calmness),
                maximum_agitation=_clamp(maximum_agitation),
                chorus=_clamp(chorus),
                all_calm=bool(all_calm),
            )

    def update_show(self, values: ShowControl) -> None:
        values.master_brightness = _clamp(values.master_brightness)
        values.expected_platforms = max(1, min(12, int(values.expected_platforms)))
        with self._lock:
            self._show = values

    def set_master_level(self, value: float) -> None:
        with self._lock:
            self.master_level = _clamp(value)

    def features(self) -> AudioFeatures:
        with self._lock:
            return AudioFeatures(
                level=self._features.level,
                bass=self._features.bass,
                mid=self._features.mid,
                high=self._features.high,
                beat=self._features.beat,
            )

    def render(self, frames: int) -> np.ndarray:
        if frames <= 0:
            return np.zeros((0, 4), dtype=np.float32)

        with self._lock:
            start_sample = self._sample_index
            platforms = tuple(self._platforms.values())
            voices = tuple(self._voices)
            installation = InstallationControl(**{
                field: getattr(self._installation, field)
                for field in self._installation.__dataclass_fields__
            })
            show = ShowControl(**{
                field: getattr(self._show, field)
                for field in self._show.__dataclass_fields__
            })
            master_level = self.master_level

        sample_numbers = start_sample + np.arange(frames, dtype=np.float64)
        t = sample_numbers / self.sample_rate
        output = np.zeros((frames, 4), dtype=np.float64)

        calm = installation.mean_calmness
        agitation = installation.maximum_agitation
        chorus = installation.chorus
        occupied_fraction = _clamp(
            installation.occupied_platforms / max(1, show.expected_platforms)
        )

        self._render_ambient(output, t, calm, agitation, chorus, occupied_fraction)
        self._render_platform_beds(output, t, platforms)
        self._render_voices(output, t, voices)
        self._render_chorus(output, t, chorus)
        self._render_water_noise(output, frames, calm, agitation, occupied_fraction)

        # Soft saturation gives headroom when several visitors activate at once.
        output = np.tanh(output * 1.35) * master_level
        output = np.nan_to_num(output, copy=False, nan=0.0, posinf=1.0, neginf=-1.0)
        output = np.clip(output, -1.0, 1.0)

        rms_channels = np.sqrt(np.mean(output * output, axis=0) + 1e-12)
        total_rms = float(np.sqrt(np.mean(output * output) + 1e-12))
        with self._lock:
            self._sample_index += frames
            voice_lifetime = max(4.5, show.activation_wave_duration_seconds)
            self._voices = [
                voice
                for voice in self._voices
                if (self._sample_index - voice.start_sample) / self.sample_rate < voice_lifetime
            ]
            self._features = AudioFeatures(
                level=_clamp(total_rms * 8.0),
                bass=_clamp(float(np.mean(rms_channels[:2])) * 10.0),
                mid=_clamp((calm * 0.35 + occupied_fraction * 0.30 + total_rms * 4.0)),
                high=_clamp((agitation * 0.55 + chorus * 0.25 + total_rms * 3.0)),
                beat=self._sample_index <= self._beat_until_sample,
            )

        return output.astype(np.float32, copy=False)

    def _render_ambient(
        self,
        output: np.ndarray,
        t: np.ndarray,
        calm: float,
        agitation: float,
        chorus: float,
        occupied_fraction: float,
    ) -> None:
        dissonant_ratios = (1.0, 1.366, 1.887, 2.377)
        harmonious_ratios = (1.0, 1.5, 2.0, 2.5)
        spatial = np.asarray(
            (
                (0.74, 0.22, 0.59, 0.22),
                (0.20, 0.73, 0.24, 0.58),
                (0.58, 0.20, 0.25, 0.74),
                (0.22, 0.56, 0.74, 0.24),
            ),
            dtype=np.float64,
        )
        spatial /= np.sqrt(np.sum(spatial * spatial, axis=1, keepdims=True))
        base = 36.0 + calm * 4.0
        breath = 0.72 + 0.28 * np.sin(2.0 * math.pi * 0.045 * t)
        amplitude = 0.018 + occupied_fraction * 0.011 + chorus * 0.008
        for index, (dissonant, harmonious) in enumerate(
            zip(dissonant_ratios, harmonious_ratios, strict=True)
        ):
            ratio = dissonant + (harmonious - dissonant) * calm
            frequency = base * ratio
            phase_mod = agitation * 0.7 * np.sin(2.0 * math.pi * (0.17 + index * 0.03) * t)
            signal = np.sin(2.0 * math.pi * frequency * t + phase_mod)
            output += (signal * breath * amplitude / (1.0 + index * 0.18))[:, None] * spatial[index]

    def _render_platform_beds(
        self,
        output: np.ndarray,
        t: np.ndarray,
        platforms: tuple[PlatformControl, ...],
    ) -> None:
        pentatonic = (1.0, 9 / 8, 5 / 4, 3 / 2, 5 / 3)
        for platform in platforms:
            if not platform.occupied:
                continue
            ratio = pentatonic[(platform.platform_id - 1) % len(pentatonic)]
            octave = 1.0 if ((platform.platform_id - 1) // len(pentatonic)) % 2 == 0 else 2.0
            frequency = 55.0 * ratio * octave
            wobble = platform.agitation * (
                0.8 * np.sin(2.0 * math.pi * 3.1 * t + platform.platform_id)
                + 0.35 * np.sin(2.0 * math.pi * 6.7 * t)
            )
            coherent = np.sin(2.0 * math.pi * frequency * t)
            animated = np.sin(2.0 * math.pi * frequency * t + wobble)
            signal = coherent * platform.calmness + animated * (1.0 - platform.calmness)
            breath = 0.66 + 0.34 * np.sin(
                2.0 * math.pi * (0.055 + platform.platform_id * 0.002) * t
            )
            amplitude = (
                0.010
                + platform.calmness * 0.012
                + platform.agitation * 0.020
                + _clamp(platform.weight_kg / 120.0) * 0.004
            )
            output += (signal * breath * amplitude)[:, None] * np.asarray(platform.gains)

    def _render_voices(
        self,
        output: np.ndarray,
        t: np.ndarray,
        voices: tuple[ActivationVoice, ...],
    ) -> None:
        for voice in voices:
            age = t - (voice.start_sample / self.sample_rate)
            active = age >= 0.0
            if not np.any(active):
                continue
            safe_age = np.maximum(age, 0.0)
            attack = 1.0 - np.exp(-safe_age * 34.0)
            chime_envelope = attack * np.exp(-safe_age * (0.72 + voice.agitation * 0.6))
            whoosh_envelope = attack * np.exp(-safe_age * 1.7)
            base = 196.0 * (2.0 ** (((voice.platform_id or 1) % 7) / 12.0))
            downward_chirp = base * (1.0 + 0.32 * np.exp(-safe_age * 2.2))
            phase = 2.0 * math.pi * downward_chirp * safe_age
            chime = (
                np.sin(phase)
                + 0.45 * np.sin(phase * 1.5 + voice.seed * math.pi)
                + 0.22 * np.sin(phase * 2.01)
            )
            pseudo_noise = (
                np.sin(2.0 * math.pi * (83.0 + voice.seed * 31.0) * safe_age)
                * np.sin(2.0 * math.pi * 137.3 * safe_age + 1.2)
                * np.sin(2.0 * math.pi * 211.7 * safe_age + 0.4)
            )
            weight_gain = 0.78 + _clamp(voice.weight_kg / 100.0) * 0.22
            signal = (
                chime * chime_envelope * 0.14
                + pseudo_noise * whoosh_envelope * (0.035 + voice.agitation * 0.04)
            ) * weight_gain
            signal = np.where(active, signal, 0.0)
            output += signal[:, None] * np.asarray(voice.gains)

    def _render_chorus(self, output: np.ndarray, t: np.ndarray, chorus: float) -> None:
        if chorus < 1e-4:
            return
        gains = np.asarray((0.5, 0.5, 0.5, 0.5), dtype=np.float64)
        pulse = 0.72 + 0.28 * np.sin(2.0 * math.pi * 0.075 * t)
        chord = np.zeros_like(t)
        for index, frequency in enumerate((110.0, 165.0, 220.0, 330.0, 440.0)):
            chord += np.sin(2.0 * math.pi * frequency * t + index * 0.31) / (1.0 + index * 0.32)
        shimmer = np.sin(2.0 * math.pi * 880.0 * t + 0.8 * np.sin(2.0 * math.pi * 0.13 * t))
        signal = (chord * 0.018 + shimmer * 0.004) * pulse * chorus
        output += signal[:, None] * gains

    def _render_water_noise(
        self,
        output: np.ndarray,
        frames: int,
        calm: float,
        agitation: float,
        occupied_fraction: float,
    ) -> None:
        # Deterministic generator seed is tied to block position for repeatable tests.
        with self._lock:
            seed = (self._sample_index ^ 0x5A17) & 0xFFFFFFFF
            state = self._noise_state.copy()
        rng = np.random.default_rng(seed)
        noise = rng.standard_normal((frames, 4))
        alpha = 0.012 + agitation * 0.12 + (1.0 - calm) * 0.025
        filtered = np.empty_like(noise)
        for sample in range(frames):
            state += alpha * (noise[sample] - state)
            filtered[sample] = state
        with self._lock:
            self._noise_state = state
        amplitude = 0.004 + occupied_fraction * 0.002 + agitation * 0.012
        output += filtered * amplitude
