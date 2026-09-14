#!/usr/bin/env bash
# =============================================================================
# Container entrypoint: starts the C engine, the live feed bridge and the
# monitoring dashboard together. All logs go to stdout for `docker logs -f`.
# Everything runs in one container because the dashboard reads the engine API
# at the hardcoded http://localhost:8080.
# =============================================================================
set -euo pipefail

cd /app
mkdir -p data/logs

ENGINE_PORT="${ENGINE_PORT:-8080}"
DASH_PORT="${DASH_PORT:-3000}"
PIDS=()

# 1) C11 engine (HTTP monitoring on $ENGINE_PORT, IPC feed on 9999 from config)
echo "[ENTRY] starting C engine on :${ENGINE_PORT} ..."
./build/bin/pmt_engine config/engine.ini "${ENGINE_PORT}" &
PIDS+=($!)

# 2) Live Binance + Polymarket feed bridge (unbuffered python)
echo "[ENTRY] starting feed bridge ..."
python3 -u scripts/feed_bridge.py --insecure-ssl &
PIDS+=($!)

# 3) Next.js monitoring dashboard (standalone)
echo "[ENTRY] starting dashboard on :${DASH_PORT} ..."
(cd dashboard && PORT="${DASH_PORT}" HOSTNAME=0.0.0.0 node server.js) &
PIDS+=($!)

shutdown() {
    echo "[ENTRY] shutting down ..."
    for p in "${PIDS[@]}"; do kill "$p" 2>/dev/null || true; done
    exit 0
}
trap shutdown SIGINT SIGTERM

echo "[ENTRY] all processes up. Engine=http://localhost:${ENGINE_PORT} Dashboard=http://localhost:${DASH_PORT}"
# Supervisor-style wait: exit the container if any child dies (so restart policy
# and healthchecks can react).
while :; do
    for p in "${PIDS[@]}"; do
        if ! kill -0 "$p" 2>/dev/null; then
            echo "[ENTRY] process $p exited; stopping container."
            shutdown
        fi
    done
    sleep 2
done