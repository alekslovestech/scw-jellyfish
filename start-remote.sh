#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VITE_PORT=5173
TUNNEL_LOG=/tmp/cf-tunnel.log

# Install cloudflared if missing
if ! command -v cloudflared &>/dev/null; then
    echo "Installing cloudflared..."
    curl -fsSL --output /tmp/cloudflared.deb \
        https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb
    sudo dpkg -i /tmp/cloudflared.deb
    rm /tmp/cloudflared.deb
    echo "cloudflared installed."
fi

# Install yarn deps if needed
if [ ! -d "$SCRIPT_DIR/visualizer-ts/node_modules" ]; then
    echo "Installing dependencies..."
    cd "$SCRIPT_DIR/visualizer-ts" && yarn install
fi

# Cleanup handler
cleanup() {
    echo ""
    echo "Stopping..."
    kill "$VITE_PID" "$CF_PID" 2>/dev/null || true
    rm -f "$TUNNEL_LOG"
    echo "Done."
}
trap cleanup EXIT INT TERM

# Start Vite
echo "Starting Vite dev server..."
cd "$SCRIPT_DIR/visualizer-ts"
yarn dev --host &
VITE_PID=$!

# Wait for Vite to be ready
echo "Waiting for Vite..."
for i in $(seq 1 15); do
    sleep 1
    curl -s "http://localhost:$VITE_PORT" &>/dev/null && break
done

# Start cloudflared tunnel
echo "Starting tunnel..."
rm -f "$TUNNEL_LOG"
cloudflared tunnel --url "http://localhost:$VITE_PORT" 2>"$TUNNEL_LOG" &
CF_PID=$!

# Extract public URL from cloudflared output
echo "Waiting for tunnel URL..."
URL=""
for i in $(seq 1 30); do
    sleep 1
    URL=$(grep -o 'https://[a-zA-Z0-9-]*\.trycloudflare\.com' "$TUNNEL_LOG" 2>/dev/null | head -1 || true)
    [ -n "$URL" ] && break
done

if [ -z "$URL" ]; then
    echo "Could not get tunnel URL. Check $TUNNEL_LOG for details."
    exit 1
fi

echo ""
echo "========================================"
echo "  Open on your phone:"
echo ""
echo "  $URL"
echo ""
echo "  Ctrl+C to stop"
echo "========================================"

wait "$VITE_PID"
