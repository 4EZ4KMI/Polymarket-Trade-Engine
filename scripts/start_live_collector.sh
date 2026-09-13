#!/usr/bin/env bash
set -e

# ==============================================================================
# Polymarket 100% Real Live Market Data Collector & Paper Trading Engine
# ==============================================================================

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "=== Building C11 Trading Core ==="
make all

mkdir -p data data/logs

echo ""
echo "=== Starting C11 Paper Engine on Port 8080 (IPC Port 9999) ==="
./build/bin/pmt_engine config/engine.ini &
ENGINE_PID=$!

trap "echo ''; echo 'Shutting down engine and feeds...'; kill $ENGINE_PID 2>/dev/null || true; exit 0" SIGINT SIGTERM EXIT

sleep 1

echo ""
echo "=== Connecting 100% Real Binance & Polymarket Feeds ==="
python3 scripts/feed_bridge.py "$@"

wait $ENGINE_PID
