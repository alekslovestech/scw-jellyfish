#!/usr/bin/env python3
"""Broadcast simulated meditation-platform telemetry for installation testing.

This tool does not bind the show port, so it can run beside the dashboard. It
sends protocol-v2 UDP packets to the dashboard and any ESP32s on the same
broadcast domain.
"""

from __future__ import annotations

import argparse
import json
import math
import socket
import time
from dataclasses import dataclass

PROTOCOL_VERSION = 2
SHOW_UDP_PORT = 42120


@dataclass(frozen=True, slots=True)
class Sample:
    occupied: bool
    agitation: float
    calmness: float
    weight_kg: float


def clamp(value: float) -> float:
    return max(0.0, min(1.0, value))


def platform_positions(count: int, radius: float, height: float) -> list[tuple[float, float, float]]:
    return [
        (
            math.cos(index * math.tau / count) * radius,
            height,
            math.sin(index * math.tau / count) * radius,
        )
        for index in range(count)
    ]


def sample_scenario(name: str, elapsed: float, index: int, count: int) -> Sample:
    phase = elapsed % 80.0
    base_weight = 62.0 + index * 3.0

    if name == "empty":
        return Sample(False, 0.0, 0.0, 0.0)
    if name == "agitated":
        agitation = 0.48 + 0.22 * math.sin(elapsed * 4.1 + index)
        return Sample(True, clamp(agitation), 0.05, base_weight)
    if name == "calm":
        return Sample(True, 0.025, 0.95, base_weight)

    # Demonstration loop: empty -> sequential arrival -> active/jostled ->
    # settling -> collective calm -> release.
    if phase < 7.0:
        return Sample(False, 0.0, 0.0, 0.0)

    arrival_time = 7.0 + index * (9.0 / max(1, count - 1))
    if phase < arrival_time:
        return Sample(False, 0.0, 0.0, 0.0)
    since_arrival = phase - arrival_time

    if phase < 28.0:
        agitation = 0.62 * math.exp(-since_arrival / 4.5)
        return Sample(True, clamp(agitation), 0.0, base_weight)
    if phase < 42.0:
        agitation = 0.28 + 0.26 * abs(math.sin(elapsed * 2.4 + index * 0.9))
        return Sample(True, clamp(agitation), 0.04, base_weight)
    if phase < 68.0:
        progress = clamp((phase - 42.0) / 24.0)
        agitation = 0.24 * (1.0 - progress) + 0.02
        calmness = progress * progress * (3.0 - 2.0 * progress)
        return Sample(True, agitation, calmness, base_weight)
    if phase < 77.0:
        return Sample(True, 0.015, 0.97, base_weight)
    return Sample(False, 0.0, 0.0, 0.0)


def encode(packet: dict[str, object]) -> bytes:
    return json.dumps(packet, separators=(",", ":")).encode("utf-8")


def send_activation(
    sock: socket.socket,
    target: tuple[str, int],
    platform_id: int,
    position: tuple[float, float, float],
    show_time_ms: int,
    weight_kg: float,
) -> None:
    x, y, z = position
    packet = {
        "protocol": PROTOCOL_VERSION,
        "type": "activation",
        "eventId": int(time.time_ns() & 0xFFFFFFFF) or 1,
        "platformId": platform_id,
        "x": x,
        "y": y,
        "z": z,
        "weightKg": weight_kg,
        "agitation": 0.65,
        "showTimeMs": show_time_ms,
    }
    sock.sendto(encode(packet), target)


def run(args: argparse.Namespace) -> None:
    target = (args.target, args.port)
    positions = platform_positions(args.count, args.radius, args.height)
    sequence = [0] * args.count
    previous_occupied = [False] * args.count
    interval = 1.0 / args.rate
    started = time.monotonic()
    deadline = started + args.duration if args.duration > 0 else None

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        print(
            f"Simulating {args.count} platforms to {args.target}:{args.port} "
            f"({args.scenario}; Ctrl-C to stop)"
        )
        try:
            while deadline is None or time.monotonic() < deadline:
                frame_started = time.monotonic()
                elapsed = frame_started - started
                show_time_ms = int(elapsed * 1000)
                occupied_total = 0
                calm_values: list[float] = []

                for index, position in enumerate(positions):
                    sample = sample_scenario(args.scenario, elapsed, index, args.count)
                    sequence[index] += 1
                    platform_id = args.first_id + index
                    x, y, z = position
                    if sample.occupied:
                        occupied_total += 1
                        calm_values.append(sample.calmness)
                    packet = {
                        "protocol": PROTOCOL_VERSION,
                        "type": "platform",
                        "id": platform_id,
                        "sequence": sequence[index],
                        "x": x,
                        "y": y,
                        "z": z,
                        "weightKg": sample.weight_kg,
                        "agitation": sample.agitation,
                        "calmness": sample.calmness,
                        "occupied": sample.occupied,
                        "showTimeMs": show_time_ms,
                    }
                    sock.sendto(encode(packet), target)
                    if sample.occupied and not previous_occupied[index]:
                        send_activation(
                            sock,
                            target,
                            platform_id,
                            position,
                            show_time_ms,
                            sample.weight_kg,
                        )
                    previous_occupied[index] = sample.occupied

                mean_calm = sum(calm_values) / len(calm_values) if calm_values else 0.0
                print(
                    f"\roccupied {occupied_total}/{args.count}  mean calm {mean_calm:0.2f}  "
                    f"show {show_time_ms / 1000:0.1f}s",
                    end="",
                    flush=True,
                )
                sleep_for = interval - (time.monotonic() - frame_started)
                if sleep_for > 0:
                    time.sleep(sleep_for)
        except KeyboardInterrupt:
            pass
        finally:
            print()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", default="255.255.255.255", help="broadcast or unicast address")
    parser.add_argument("--port", type=int, default=SHOW_UDP_PORT)
    parser.add_argument("--count", type=int, default=4, choices=range(1, 13), metavar="1-12")
    parser.add_argument("--first-id", type=int, default=1)
    parser.add_argument("--radius", type=float, default=4.0, help="platform layout radius in metres")
    parser.add_argument("--height", type=float, default=6.0, help="platform Y coordinate in metres")
    parser.add_argument("--rate", type=float, default=5.0, help="telemetry packets per platform per second")
    parser.add_argument("--duration", type=float, default=0.0, help="seconds; zero runs until interrupted")
    parser.add_argument(
        "--scenario",
        choices=("cycle", "empty", "agitated", "calm"),
        default="cycle",
    )
    return parser


if __name__ == "__main__":
    run(build_parser().parse_args())
