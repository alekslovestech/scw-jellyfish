# Optional four-channel reference sound engine

`audio.engine` is a compact generative reference implementation for the four-speaker soundscape described by the installation concept. It is deliberately separate from the dashboard so a dedicated audio computer can run it with a stable multichannel interface.

It receives OSC from the dashboard:

- `/jelly/platform`: platform ID, occupied, weight, agitation, calmness, then four equal-power speaker gains.
- `/jelly/activation`: platform ID, XYZ, weight, agitation, then four gains.
- `/jelly/installation`: seen count, occupied count, mean calmness, minimum calmness, maximum agitation, chorus amount, all-calm flag.
- `/jelly/show`: global show and transition settings.

The engine produces a low generative underwater bed, spatial platform tones, activation chimes/whooshes, and a collective harmonic chorus. It also broadcasts a small `type: "audio"` feature packet to the ESP32 fleet for audio-reactive fallback patterns.

## Run

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r audio/requirements.txt
python -m audio.engine --list-devices
python -m audio.engine --device 3 --osc-port 9000
```

Select an output device exposing at least four channels. Speaker order is front-left, front-right, rear-left, rear-right. Confirm the interface's channel mapping and start at a conservative amplifier level.

This engine is a reference, not a mastered exhibition mix. Tune `master`, oscillator levels, filtering, limiter behavior, and speaker gains with the real room and loudspeakers.
