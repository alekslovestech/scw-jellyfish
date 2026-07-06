import socket
import time
import math
import argparse
import numpy as np
import sounddevice as sd


UDP_PORT = 4210
BROADCAST_IP = "255.255.255.255"


def make_heartbeat_audio(sample_rate=44100, duration=0.55):
    """
    Creates a synthetic lub-dub heartbeat sound.
    Returns a numpy array of audio samples.
    """
    t = np.linspace(0, duration, int(sample_rate * duration), endpoint=False)
    audio = np.zeros_like(t)

    def thump(center, width, freq, amp):
        envelope = np.exp(-((t - center) ** 2) / (2 * width ** 2))
        tone = np.sin(2 * np.pi * freq * t)
        return amp * envelope * tone

    # "Lub"
    audio += thump(center=0.08, width=0.025, freq=55, amp=0.9)
    audio += thump(center=0.10, width=0.035, freq=90, amp=0.35)

    # "Dub"
    audio += thump(center=0.28, width=0.030, freq=48, amp=0.7)
    audio += thump(center=0.31, width=0.040, freq=80, amp=0.25)

    # Soft clipping / normalization
    audio = np.tanh(audio * 2.5)
    audio = audio / max(0.01, np.max(np.abs(audio)))

    return audio.astype(np.float32)


def pulse_shape(x):
    """
    x is phase from 0.0 to 1.0 through one heartbeat cycle.
    Produces two light pulses: lub and dub.
    """
    lub = math.exp(-((x - 0.08) ** 2) / (2 * 0.030 ** 2))
    dub = 0.70 * math.exp(-((x - 0.28) ** 2) / (2 * 0.040 ** 2))
    glow = 0.08
    return max(glow, min(1.0, lub + dub))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bpm", type=float, default=72, help="Heartbeat BPM")
    parser.add_argument("--port", type=int, default=UDP_PORT, help="UDP port")
    parser.add_argument("--ip", default=BROADCAST_IP, help="Broadcast IP or specific ESP32 IP")
    parser.add_argument("--fps", type=float, default=60, help="UDP lighting updates per second")
    parser.add_argument("--hue", type=int, default=0, help="Base hue, 0-255. 0 is red-ish.")
    parser.add_argument("--volume", type=float, default=0.8, help="Audio volume, 0.0-1.0")
    args = parser.parse_args()

    seconds_per_beat = 60.0 / args.bpm
    sample_rate = 44100
    heartbeat_audio = make_heartbeat_audio(sample_rate=sample_rate)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    start_time = time.perf_counter()
    next_audio_time = start_time
    next_udp_time = start_time

    print("Heartbeat pulse sender running.")
    print(f"BPM: {args.bpm}")
    print(f"UDP target: {args.ip}:{args.port}")
    print("Press Ctrl+C to stop.")

    try:
        while True:
            now = time.perf_counter()

            # Play heartbeat sound once per beat
            if now >= next_audio_time:
                sd.play(heartbeat_audio * args.volume, sample_rate, blocking=False)
                next_audio_time += seconds_per_beat

            # Send light pulse data continuously
            if now >= next_udp_time:
                elapsed = now - start_time
                phase = (elapsed % seconds_per_beat) / seconds_per_beat
                level = pulse_shape(phase)

                # beat = 1 near the start of the "lub"
                beat = 1 if phase < 0.05 else 0

                # CSV format:
                # mode,level,beat,hue,phase
                message = f"heartbeat,{level:.3f},{beat},{args.hue},{phase:.3f}"
                sock.sendto(message.encode("utf-8"), (args.ip, args.port))

                next_udp_time += 1.0 / args.fps

            time.sleep(0.001)

    except KeyboardInterrupt:
        print("\nStopped.")


if __name__ == "__main__":
    main()
