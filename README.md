# Polymarket HFT Engine (C11 Core + Next.js Dashboard)

High-frequency quantitative trading engine for Polymarket binary options:
- **Strategy A: 5-minute BTC Parity Arbitrage** (YES + NO ask-sum mispricing with book-walking & partial fill hedge).
- **Strategy B: 15-minute BTC Flow Skew** (Binance BTC sub-second momentum + Polymarket order book imbalance).
- **Hardcoded Paper-Only Default** (`LIVE_TRADING=false` lock guard).

---

## Quick Start

### 1. Run Everything (C Engine + Next.js Dashboard)
```bash
./run.sh
```
- C Engine starts on `http://127.0.0.1:8080`.
- Next.js Dashboard starts on `http://localhost:3000`.

### 2. Run Test Suites
```bash
make run_tests
```

### 3. Build C Engine Standalone
```bash
make engine
./build/bin/pmt_engine 8080

### 3. Run with 100% Real Live Feeds (Binance WebSocket + Polymarket CLOB)
```bash
./scripts/start_live_collector.sh
```
Or run the engine and live feeder separately:
```bash
# Terminal 1: C Engine
./build/bin/pmt_engine config/engine.ini

# Terminal 2: Live Market Feeder (real Binance trades & Polymarket books)
python3 scripts/feed_bridge.py --yes-token <token_id_yes> --no-token <token_id_no>
```
All real incoming ticks are automatically recorded to `data/live_stream.bin` for deterministic historical replay backtesting.


---

## Core C Architecture (`include/` and `src/`)

- `include/core/ptypes.h`: Integer-scaled arithmetic (`PT_PRICE_SCALE = 1000`), nanosecond timers, direction enums.
- `include/util/pt_ring.h`: Lock-free single-producer / single-consumer ring buffer.
- `include/util/pt_winbuf.h`: Fixed-capacity rolling window circular buffer for sub-second trade aggregations.
- `include/orderbook/pt_book.h`: Pure C L2 order book with delta updates, snapshots, depth volume, imbalance, and book-walking average price calculation.
- `include/features/pt_features.h`: Microprice, L1/L3/L5/L10 imbalance, distance-weighted imbalance, trade-flow ratios, BTC momentum windows (10ms to 10s).
- `include/execution/pt_arb.h` & `pt_arb_mgr.h`: Arbitrage edge calculator with fee/slippage/latency buffers and state-machine execution manager with partial-fill hedging.
- `include/strategies/pt_flow_skew.h`: 15-minute BTC Momentum & Polymarket Flow Skew strategy evaluator.
- `include/risk/pt_risk.h`: Risk Engine tracking position limits, total exposure, daily loss, drawdown, feed staleness, consecutive loss breaker, and emergency kill switch.
- `include/portfolio/pt_portfolio.h`: Portfolio position tracker, cash, equity, realized/unrealized mark-to-market PnL, win rate.
- `include/net/pt_reactor.h`: Portable non-blocking reactor (`kqueue` on macOS, `epoll` on Linux).
- `include/net/pt_http_server.h`: In-process non-blocking HTTP REST server (`/api/status`, `/api/portfolio`, `/api/book`, `/api/telemetry`, `/api/kill`, `/api/resume`).
- `include/storage/pt_csv_log.h` & `pt_event_log.h`: Append-only CSV audit logs and binary zero-copy mmap event log.

---

## Safety & Compliance

- **Hardcoded Paper-Only Default**: The engine validates `LIVE_TRADING=false` on startup and refuses to run with `LIVE_TRADING=true`.
- **Zero API Key Requirements**: Paper simulation operates out-of-the-box using realistic broker queue simulation.
- **Emergency Kill Switch**: Accessible via CLI, HTTP API (`POST /api/kill`), or Next.js UI banner button. Automatically tripped on drawdown or feed disconnection.
```