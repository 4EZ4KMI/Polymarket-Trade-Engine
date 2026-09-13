#!/usr/bin/env bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR"

echo "========================================================="
echo "   STARTING POLYMARKET HFT ENGINE & MONITORING STACK    "
echo "========================================================="

# 1. Build C engine
echo "[1/3] Building C HFT Engine..."
make -j4

# 2. Start C Engine in background
echo "[2/3] Launching C HFT Engine on :8080..."
mkdir -p data/logs
./build/bin/pmt_engine config/engine.ini 8080 &
ENGINE_PID=$!

cleanup() {
    echo ""
    echo "[!] Stopping C HFT Engine (PID $ENGINE_PID)..."
    kill $ENGINE_PID 2>/dev/null || true
    wait $ENGINE_PID 2>/dev/null || true
    echo "[!] Shutdown complete."
}
trap cleanup EXIT INT TERM

sleep 1

# 3. Start Next.js Dashboard
echo "[3/3] Starting Next.js Monitoring Dashboard on http://localhost:3000..."
cd dashboard
npm run dev