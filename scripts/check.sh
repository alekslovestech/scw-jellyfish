#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

python3 -m pytest -q
python3 -m compileall -q dashboard audio tools run_dashboard.py
node --check dashboard/static/app.js

if command -v g++ >/dev/null 2>&1; then
  ./scripts/check_firmware_gnu11.sh
else
  printf '%s\n' 'g++ not found; skipped the GNU++11 firmware syntax check.'
fi

if command -v pio >/dev/null 2>&1; then
  pio run -d firmware
else
  printf '%s\n' 'PlatformIO not found; skipped the real ESP32 build.'
fi
