#!/usr/bin/env python3
"""Run the optional four-channel Jellyfish Forest reference sound engine."""

from __future__ import annotations

import argparse
import json
import socket
import sys
import threading
import time
from typing import Any

try:
    import sounddevice as sd
    from pythonosc.dispatcher import Dispatcher
    from pythonosc.osc_server import ThreadingOSCUDPServer
except ImportError as exc:  # pragma: no cover - depends on local audio installation.
    raise SystemExit(
        "Missing audio dependencies. Install audio/requirements.txt before running."
    ) from exc

try:
    from .soundscape import ShowControl, Soundscape
except ImportError:  # Support direct execution from the project directory.
    from soundscape import ShowControl, Soundscape  # type: ignore

SHOW_UDP_PORT = 42120
PROTOCOL_VERSION = 2


def _floats(values: tuple[Any, ...], count: int) -> tuple[float, ...] | None:
    if len(values) < count:
        return None
    try:
        return tuple(float(value) for value in values[:count])
    except (TypeError, ValueError):
        return None


class Runtime:
    def __init__(
        self,
        soundscape: Soundscape,
        osc_host: str,
        osc_port: int,
        broadcast_address: str,
    ) -> None:
        self.soundscape = soundscape
        self.osc_host = osc_host
        self.osc_port = osc_port
        self.broadcast_address = broadcast_address
        self.server: ThreadingOSCUDPServer | None = None
        self.server_thread: threading.Thread | None = None
        self.feature_thread: threading.Thread | None = None
        self.stop_event = threading.Event()

    def start(self) -> None:
        dispatcher = Dispatcher()
        dispatcher.map("/jelly/platform", self.on_platform)
        dispatcher.map("/jelly/activation", self.on_activation)
        dispatcher.map("/jelly/installation", self.on_installation)
        dispatcher.map("/jelly/show", self.on_show)
        self.server = ThreadingOSCUDPServer(
            (self.osc_host, self.osc_port), dispatcher
        )
        self.server_thread = threading.Thread(
            target=self.server.serve_forever,
            name="jelly-osc",
            daemon=True,
        )
        self.server_thread.start()
        self.feature_thread = threading.Thread(
            target=self._broadcast_features,
            name="jelly-audio-features",
            daemon=True,
        )
        self.feature_thread.start()

    def stop(self) -> None:
        self.stop_event.set()
        if self.server:
            self.server.shutdown()
            self.server.server_close()
        if self.server_thread:
            self.server_thread.join(timeout=2.0)
        if self.feature_thread:
            self.feature_thread.join(timeout=2.0)

    def callback(self, outdata: Any, frames: int, time_info: Any, status: Any) -> None:
        del time_info
        if status:
            print(status, file=sys.stderr)
        outdata[:] = self.soundscape.render(frames)

    def on_platform(self, address: str, *values: Any) -> None:
        del address
        parsed = _floats(values, 9)
        if parsed is None:
            return
        platform_id, occupied, weight, agitation, calmness, *gains = parsed
        self.soundscape.update_platform(
            int(platform_id), bool(int(occupied)), weight, agitation, calmness, gains
        )

    def on_activation(self, address: str, *values: Any) -> None:
        del address
        parsed = _floats(values, 10)
        if parsed is None:
            return
        platform_id, _x, _y, _z, weight, agitation, *gains = parsed
        self.soundscape.trigger_activation(
            int(platform_id), weight, agitation, gains
        )

    def on_installation(self, address: str, *values: Any) -> None:
        del address
        parsed = _floats(values, 7)
        if parsed is None:
            return
        seen, occupied, mean, minimum, maximum_agitation, chorus, all_calm = parsed
        self.soundscape.update_installation(
            int(seen),
            int(occupied),
            mean,
            minimum,
            maximum_agitation,
            chorus,
            bool(int(all_calm)),
        )

    def on_show(self, address: str, *values: Any) -> None:
        del address
        if len(values) < 12:
            return
        try:
            control = ShowControl(
                mode=str(values[0]),
                master_brightness=float(values[1]),
                expected_platforms=int(values[2]),
                influence_radius_meters=float(values[3]),
                calm_threshold=float(values[4]),
                calm_release_threshold=float(values[5]),
                max_agitation_for_chorus=float(values[6]),
                all_calm_hold_seconds=float(values[7]),
                chorus_fade_seconds=float(values[8]),
                activation_wave_speed_mps=float(values[9]),
                activation_wave_width_meters=float(values[10]),
                activation_wave_duration_seconds=float(values[11]),
            )
        except (TypeError, ValueError):
            return
        self.soundscape.update_show(control)

    def _broadcast_features(self) -> None:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        try:
            while not self.stop_event.wait(0.05):
                features = self.soundscape.features()
                packet = {
                    "protocol": PROTOCOL_VERSION,
                    "type": "audio",
                    "level": features.level,
                    "bass": features.bass,
                    "mid": features.mid,
                    "high": features.high,
                    "beat": features.beat,
                }
                try:
                    sock.sendto(
                        json.dumps(packet, separators=(",", ":")).encode("utf-8"),
                        (self.broadcast_address, SHOW_UDP_PORT),
                    )
                except OSError:
                    pass
        finally:
            sock.close()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list-devices", action="store_true")
    parser.add_argument("--device", help="sounddevice output device index or name")
    parser.add_argument("--sample-rate", type=int, default=48_000)
    parser.add_argument("--block-size", type=int, default=512)
    parser.add_argument("--master", type=float, default=0.7)
    parser.add_argument("--osc-host", default="0.0.0.0")
    parser.add_argument("--osc-port", type=int, default=9000)
    parser.add_argument("--broadcast", default="255.255.255.255")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.list_devices:
        print(sd.query_devices())
        return

    device: int | str | None = args.device
    if isinstance(device, str) and device.isdigit():
        device = int(device)

    soundscape = Soundscape(args.sample_rate, args.master)
    runtime = Runtime(
        soundscape,
        osc_host=args.osc_host,
        osc_port=args.osc_port,
        broadcast_address=args.broadcast,
    )
    runtime.start()
    try:
        print(
            f"Listening for Jellyfish OSC on {args.osc_host}:{args.osc_port}; "
            "starting four-channel output."
        )
        with sd.OutputStream(
            samplerate=args.sample_rate,
            blocksize=args.block_size,
            device=device,
            channels=4,
            dtype="float32",
            callback=runtime.callback,
        ):
            while True:
                time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        runtime.stop()


if __name__ == "__main__":
    main()
