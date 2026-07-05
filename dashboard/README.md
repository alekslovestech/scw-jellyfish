# ESP32 Fleet Dashboard

Run this on a laptop to discover ESP32 devices via mDNS and control them from one page.

## Install on Windows Command Prompt

```bat
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python app.py
```

Open http://localhost:8080

## Install on macOS/Linux

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python app.py
```

Open http://localhost:8080

## ESP32 requirement

Each ESP32 should advertise `_esp32art._tcp.local` and expose these HTTP endpoints:

- `GET /status` recommended; used for name, firmware, number, Wi-Fi SSID, and RSSI/signal strength
- `GET /log` recommended; used by the dashboard Log button
- `GET /number` existing fallback
- `POST /setNumber` with `value`
- `POST /identify`
- `POST /rename` with `name`
- `POST /update` multipart file field `update`

See `ESP32_DASHBOARD_SUPPORT_SNIPPETS.cpp` for the ESP32 code to add.

## New in this version

- Wi-Fi signal strength column, showing RSSI in dBm and an approximate quality percentage.
- Per-device Log button, which fetches `GET /log` from the ESP32 and displays the returned text in the Result panel.
