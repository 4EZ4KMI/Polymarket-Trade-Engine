# Polymarket HFT Engine (C11 Core + Python Feed Bridge)

Production-hardened shadow/simulation trading engine for Polymarket **BTC 5m/15m** binary options.
**PAPER TRADING ONLY** — the engine refuses to start if `LIVE_TRADING=true` is set.

- **Strategy A — 5-minute BTC Parity Arbitrage** (`PT_STRAT_PARITY5M`): YES + NO ask-sum
  mispricing with book-walking and partial-fill hedging.
- **Strategy B — 15-minute BTC Flow Skew** (`PT_STRAT_FLOW15M`): Binance BTC sub-second
  momentum + Polymarket order-book imbalance.
- **100% real data, zero synthesis**: the engine starts with empty books; every tick comes
  from live Binance WS + Polymarket CLOB via the Python bridge. Market identity is the real
  `condition_id` (internal `market_id` = SHA-256 of it). Resolutions are polled from the
  Gamma API and only ever report a *verified* winner — never fabricated.

---

## Quick Start

```bash
# 1. Build the C engine + tests
make          # or: make engine (engine only)

# 2. Run the test suite
make run_tests

# 3. Run the engine + live feeds (see "Running" below)
scripts/start_live_collector.sh            # segments of feeds + engine
# or run each piece manually
```

---

## Running

### A. Full stack (engine + live feeds) — one command
```bash
scripts/start_live_collector.sh
```
Builds the core, starts the engine (HTTP API on `:8080`, IPC feed listener on `:9999`),
and connects the real Binance + Polymarket feed bridge. Feed-bridge CLI args pass through:
```bash
scripts/start_live_collector.sh            # auto-discover BTC markets via Gamma
scripts/start_live_collector.sh --market-id 12345 --yes-token <id> --no-token <id>
```
Stop with `Ctrl+C` (cleans up the engine + feeds).

### B. Engine + dashboard together
```bash
./run.sh
```
Starts the C engine (config `config/engine.ini`, HTTP on `:8080`) plus the Next.js
dashboard at `http://localhost:3000`. Brings up the same network stack for UI monitoring.

### C. Manually

Engine CLI: `pmt_engine [config.ini] [http_port]`
```bash
make engine

# Terminal 1 — C engine (config file + HTTP port 8080, feed IPC on 9999)
./build/bin/pmt_engine config/engine.ini 8080

# Terminal 2 — real feed bridge (auto-discovers BTC markets)
python3 scripts/feed_bridge.py

# ...or pin a specific market
python3 scripts/feed_bridge.py \
  --market-id 12345 \
  --yes-token <token_id_yes> --no-token <token_id_no>
```

### Feed bridge options
```
python3 scripts/feed_bridge.py --help
  --host            engine host          (default 127.0.0.1)
  --port            engine feed port     (default 9999)
  --proxy           HTTP(S) proxy URL    (default: system proxy)
  --yes-token       pin YES  token id
  --no-token        pin NO   token id
  --market-id       pin a numeric market id (skip auto-discovery)
  --condition-id    pin a condition_id   (skip auto-discovery)
  --insecure-ssl    disable SSL verify (untrusted local proxy)
```
Without `--yes-token/--no-token`, the bridge discovers active BTC markets itself via the
Gamma API and classifies them as `eligible_for_strategy_a` / `eligible_for_strategy_b`.

---

## Architecture (`src/`)

- `core/pt_market_registry.c` — real discovered markets; `eligible_for_strategy_a/b`,
  `get_active_5m`/`get_active_15m`, verified-resolution state.
- `net/pt_feed_parsers.c` — native Polymarket wire parsers: `book` snapshot
  (`bids[]`/`asks[]`) and `price_changes[]` deltas; Binance trade parsing.
- `net/pt_feed_bridge.c` — TCP IPC server (port 9999) receiving bridge JSON and routing it
  into books/btc/registry/risk; writes every tick to `data/live_stream.bin`.
- `analytics/pt_lifecycle_tracker.c` — signal lifecycle state machine
  (`PENDING→PARTIAL→ONE_LEG→BOTH_LEGS→HEDGED→SETTLED/CANCELLED/EXPIRED`), carries `signal_id`
  through fills for settlement attribution.
- `execution/pt_broker.c` / `pt_sim_queue.c` — internal matching engine with realistic
  queue simulation (maker/taker fees, fill probability, partial fills).
- `analytics/pt_adverse_selection.c` — per-fill adverse-selection tracking; returns an
  availability flag instead of silently defaulting to 0.0.
- `analytics/pt_strategy_stats.c` — trade-level Sharpe (no fake annualization), win rate,
  hedge cost recorded from estimated fill price.
- `risk/pt_risk.c` — pre-trade rejects: position/total limits, daily loss, drawdown,
  consecutive-loss breaker, feed staleness, emergency kill.
- `portfolio/pt_portfolio.c` — cash, equity, realized/unrealized PnL, win rate.
- `net/pt_reactor.c` + `pt_http_server.c` — non-blocking reactor (`kqueue` macOS /
  `epoll` Linux) + REST API: `/api/status`, `/api/portfolio`, `/api/book`, `/api/telemetry`,
  `/api/kill`, `/api/resume`.

## Outputs
- `data/strategy_stats.json` — per-strategy signals/fills/win-rate/PNL on shutdown.
- `data/live_stream.bin` — binary capture of real ticks for replay backtests.
- `data/logs/` — append-only CSV audit logs (signals, orders, fills, rejections).
- `build/` — compiled engine, replay binary, and test binaries.

## Safety & Compliance
- **Paper-only lock**: `LIVE_TRADING=true` aborts startup (`[FATAL SAFETY] ... permanently locked`).
- **No synthesized market data**: books start empty; only real feed ticks populate them.
- **No fake resolutions**: winners come only from the Gamma API.
- Emergency kill via CLI, `POST /api/kill`, or dashboard button.
```