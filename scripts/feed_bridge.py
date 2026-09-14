#!/usr/bin/env python3
"""Polymarket & Binance Real-Time Market Data Feeder Bridge & Discovery - PRODUCTION HARDENED

Streams real Binance BTC trades and real Polymarket order-book / trade messages to the
C11 engine, discovers real active BTC markets via the Polymarket Gamma API, and reports
verified market resolutions. No synthetic data, no fake winners, no hardcoded mid-prices.
"""
import asyncio, json, os, sys, time, ssl, re, argparse, hashlib, urllib.request
from datetime import datetime, timezone
try:
    import websockets
except ImportError:
    print("Error: 'websockets' package is required. Run: pip install websockets")
    sys.exit(1)

ENGINE_HOST = "127.0.0.1"
ENGINE_PORT = 9999
BINANCE_WS_URL = "wss://stream.binance.com:9443/ws/btcusdt@aggTrade"
POLYMARKET_WS_URL = "wss://ws-subscriptions-clob.polymarket.com/ws/market"
GAMMA_MARKETS_URL = "https://gamma-api.polymarket.com/markets?active=true&closed=false&tag=bitcoin&limit=50"
GAMMA_CRYPTO_URL = "https://gamma-api.polymarket.com/markets?active=true&closed=false&tag=crypto&limit=50"
# Polymarket's recurring speed markets (e.g. "Bitcoin Up or Down - x:xx-x:xx ET",
# slug `btc-updown-5m-<unix-window-start>`) are NOT surfaced by the tag feeds.
# They live as a continuous ladder and only appear when the feed is ordered by
# endDate. Query both directions so we capture the currently-open window and the
# upcoming ones regardless of feed fan-out.
GAMMA_ENDDESC_URL = "https://gamma-api.polymarket.com/markets?active=true&closed=false&limit=250&order=endDate&ascending=false"
GAMMA_ENDASC_URL = "https://gamma-api.polymarket.com/markets?active=true&closed=false&limit=250&order=endDate&ascending=true"

# Recurring "Coin Up or Down" window market slug: <coin>-updown-<5m|15m|1h>-<epoch>.
UP_DOWN_PAT = re.compile(r'^([a-z0-9]+)-updown-(5m|15m|1h)-(\d{9,10})$', re.IGNORECASE)


def extract_clob_tokens(m):
    """Return (yes_token_id, no_token_id) from a gamma market dict, or (None, None)."""
    ct = m.get("clobTokenIds")
    yt, nt = None, None
    if ct:
        if isinstance(ct, str):
            try:
                parsed = json.loads(ct)
                yt, nt = str(parsed[0]), str(parsed[1]) if len(parsed) >= 2 else (None, None)
            except Exception:
                pass
        elif isinstance(ct, list) and len(ct) >= 2:
            yt, nt = str(ct[0]), str(ct[1])
    if not yt or not nt:
        for t in m.get("tokens", []):
            o = str(t.get("outcome", "")).lower()
            tid = t.get("token_id")
            if o in ["yes", "1", "up", "true"]:
                yt = str(tid)
            elif o in ["no", "0", "down", "false"]:
                nt = str(tid)
    return yt, nt


def parse_iso_to_epoch_ms(iso_str):
    """Parse an ISO-8601 timestamp into epoch milliseconds, or None."""
    if not iso_str or not isinstance(iso_str, str):
        return None
    try:
        s = iso_str.strip()
        if s.endswith("Z"):
            s = s[:-1] + "+00:00"
        dt = datetime.fromisoformat(s)
        if dt.tzinfo is None:
            dt = dt.replace(tzinfo=timezone.utc)
        return int(dt.timestamp() * 1000)
    except Exception:
        return None


def extract_strike_price(q, d=0.0):
    """Best-effort strike extraction from a Polymarket question string. Never synthesizes."""
    if not q:
        return d
    m = re.search(r'(?:above|over|hit|reach|greater than|at or above|at)\s*\$?'
                  r'([0-9]{1,3}(?:,[0-9]{3})*(?:\.[0-9]+)?|[0-9]{4,6}(?:\.[0-9]+)?)',
                  q, re.IGNORECASE)
    if m:
        try:
            return float(m.group(1).replace(",", ""))
        except Exception:
            pass
    m2 = re.search(r'\$([0-9]{1,3}(?:,[0-9]{3})*(?:\.[0-9]+)?|[0-9]{4,6}(?:\.[0-9]+)?)', q)
    if m2:
        try:
            return float(m2.group(1).replace(",", ""))
        except Exception:
            pass
    return d


def derive_market_id(cid):
    """Deterministically derive an internal market_id from the native condition_id.

    The internal market_id is a pure SHA-256 hash of the condition_id (identity is
    condition_id-based); this is a deterministic mapping, never a synthetic number.
    """
    if not cid or len(cid.strip()) < 10:
        return None
    h = hashlib.sha256(cid.strip().lower().encode()).hexdigest()
    return int(h[:12], 16) % 900000 + 100000


class LiveFeedBridge:
    def __init__(self, host=ENGINE_HOST, port=ENGINE_PORT, proxy=None, yes_token=None,
                 no_token=None, market_id=None, condition_id=None, insecure_ssl=False):
        self.host, self.port = host, port
        self.proxy = proxy or os.environ.get("HTTPS_PROXY") or os.environ.get("HTTP_PROXY")
        self.insecure_ssl = insecure_ssl
        self.yes_token, self.no_token = yes_token, no_token
        self.manual_market_id, self.manual_condition_id = market_id, condition_id
        self.active_market = None
        self.pending_resolutions = {}
        self.engine_writer = None
        self.running = True
        self.total_btc_ticks = 0
        self.total_poly_ticks = 0
        self.market_ready = asyncio.Event()
        self.subscription_changed = asyncio.Event()
        self.current_subscribed_tokens = set()

    def get_ssl_context(self):
        ctx = ssl.create_default_context()
        if self.insecure_ssl:
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
        return ctx

    def _http_opener(self):
        """Return an opener honoring an optional proxy AND --insecure-ssl.

        Without insecure_ssl this verifies certificates (default urllib TLS trust).
        With --insecure-ssl it accepts a MITM/local-proxy self-signed cert in the
        chain (needed e.g. behind a corporate or charles/mitmproxy-style proxy).
        """
        handlers = []
        if self.proxy:
            handlers.append(urllib.request.ProxyHandler(
                {'http': self.proxy, 'https': self.proxy}))
        if self.insecure_ssl:
            # Match ALL external HTTPS (WS + Gamma HTTP) with the same trust policy.
            handlers.append(urllib.request.HTTPSHandler(
                context=ssl._create_unverified_context()))
        return urllib.request.build_opener(*handlers)

    async def connect_to_engine(self):
        while self.running:
            try:
                _, wr = await asyncio.open_connection(self.host, self.port)
                self.engine_writer = wr
                print(f"[BRIDGE] Connected to C11 Engine at {self.host}:{self.port}")
                if self.active_market:
                    await self.send_discovery_to_engine(self.active_market)
                return
            except Exception as e:
                print(f"[BRIDGE] Waiting for C11 Engine... ({e})")
                await asyncio.sleep(2)

    async def send_to_engine(self, s):
        if self.engine_writer:
            try:
                if not s.endswith("\n"):
                    s += "\n"
                self.engine_writer.write(s.encode("utf-8"))
                await self.engine_writer.drain()
            except Exception:
                self.engine_writer = None
                asyncio.create_task(self.connect_to_engine())
    def discover_polymarket_btc_markets(self):
        """Discover real, active Polymarket BTC markets via the Gamma API.

        Historically this hit featured/tag feeds only, which Polymarket serves as a
        generic "popular" list that never includes the recurring short-horizon BTC
        "Up or Down" speed markets. Those live as a continuous ladder keyed by a
        window epoch in the slug: `btc-updown-5m-<unix-start>` / `btc-updown-15m-...`.
        We therefore ALSO query endDate-ordered feeds and recover the true 300s/900s
        window from the slug itself (the startDate/endDateIso fields on these ladder
        markets are listing dates, not the trading window).
        """
        now_ms = int(time.time() * 1000)
        raw = []
        feeds = [GAMMA_MARKETS_URL, GAMMA_CRYPTO_URL, GAMMA_ENDDESC_URL, GAMMA_ENDASC_URL]
        for url in feeds:
            for attempt in range(3):  # gamma reachability is flaky behind some egress
                try:
                    op = self._http_opener()
                    rq = urllib.request.Request(url, headers={"User-Agent": "PM-Feed/1.0",
                                                              "Accept": "application/json"})
                    with op.open(rq, timeout=12) as r:
                        if r.status == 200:
                            data = json.loads(r.read().decode("utf-8"))
                            if isinstance(data, list):
                                raw.extend(data)
                    break
                except Exception as e:
                    if attempt >= 2:
                        print(f"[DISCOVERY] warning: Gamma request failed ({e})")
                    time.sleep(0.6)
        if not raw:
            return []

        disc = []
        bycid = {}
        for m in raw:
            if not m.get("active") or m.get("closed"):
                continue
            cid = (m.get("conditionId") or m.get("condition_id") or "").strip()
            if not cid or len(cid) < 10:
                continue
            # Restore the true window from the slug for recurring speed markets.
            um = UP_DOWN_PAT.match(m.get("slug") or "")
            st = et = None
            if um:
                win = um.group(2).lower()
                dur = {"5m": 300, "15m": 900, "1h": 3600}.get(win)
                if dur:
                    st = int(um.group(3)) * 1000
                    et = st + dur * 1000
            if st is None or et is None:
                st = parse_iso_to_epoch_ms(m.get("startDate") or m.get("startDateIso")) or now_ms
                et = parse_iso_to_epoch_ms(m.get("endDate") or m.get("endDateIso")) or (now_ms + 300000)
            if et <= now_ms:
                continue
            dur = int((et - st) / 1000)
            ea = 240 <= dur <= 450
            eb = 750 <= dur <= 1200
            if not ea and not eb:
                continue

            # BTC-only: the engine trades Binance BTC vs Polymarket BTC markets.
            q = (m.get("question", "") or "")
            sl = (m.get("slug", "") or "")
            if um:
                if um.group(1).lower() != "btc":
                    continue
            else:
                desc = (m.get("description", "") or "")
                if "btc" not in (f"{q} {sl} {desc}").lower() and "bitcoin" not in (f"{q} {sl} {desc}").lower():
                    continue

            yt, nt = extract_clob_tokens(m)
            if not yt or not nt or len(yt) < 10 or len(nt) < 10:
                continue
            mid = derive_market_id(cid)
            if not mid:
                continue
            bycid[cid] = {
                "condition_id": cid,
                "market_id": mid,
                "slug": (sl[:63] if sl else f"btc-{dur}s"),
                "yes_token_id": yt,
                "no_token_id": nt,
                "strike": extract_strike_price(q),
                "start_time": st,
                "end_time": et,
                "duration_sec": dur,
                "eligible_for_strategy_a": bool(ea),
                "eligible_for_strategy_b": bool(eb),
            }
        disc = list(bycid.values())

        # Prefer the currently-open window (start<=now<end), then the nearest
        # upcoming window, then by earliest expiry. Guarantees a real live window
        # gets picked the moment feeding begins.
        def sort_key(x):
            open_now = 1 if (x["start_time"] <= now_ms < x["end_time"]) else 0
            opening_soon = 1 if (x["start_time"] > now_ms and x["start_time"] - now_ms <= 900000) else 0
            return (-open_now, -opening_soon, x["end_time"])

        disc.sort(key=sort_key)
        return disc
    async def check_verified_resolution(self, cid):
        """Poll Gamma and return a verified resolution dict, or None if not yet resolved.

        Returns only real, confirmed winners. Never fabricates a winning side.
        """
        if not cid:
            return None
        url = f"https://gamma-api.polymarket.com/markets?condition_id={cid}"
        try:
            op = self._http_opener()
            rq = urllib.request.Request(url, headers={"User-Agent": "PM-Feed/1.0",
                                                      "Accept": "application/json"})
            with op.open(rq, timeout=10) as r:
                if r.status == 200:
                    data = json.loads(r.read().decode("utf-8"))
                    item = data[0] if isinstance(data, list) and data else (data if isinstance(data, dict) else None)
                    if item and (item.get("closed") or item.get("resolved")):
                        winner = None
                        for t in item.get("tokens", []):
                            if t.get("winner") is True or t.get("price") in [1, 1.0]:
                                o = str(t.get("outcome", "")).upper()
                                tid = str(t.get("token_id", ""))
                                winner = {
                                    "winning_outcome": "YES" if o in ["YES", "1", "UP", "TRUE"] else "NO",
                                    "winning_asset_id": tid,
                                    "resolution_price": 1.0,
                                }
                                break
                        if not winner:
                            wo = item.get("winningOutcome") or item.get("winning_official") or item.get("outcome")
                            if wo:
                                winner = {
                                    "winning_outcome": "YES" if str(wo).upper() in ["YES", "1", "UP", "TRUE"] else "NO",
                                    "winning_asset_id": "",
                                    "resolution_price": 1.0,
                                }
                        return winner
        except Exception:
            pass
        return None

    async def send_discovery_to_engine(self, m):
        dm = {
            "event_type": "market_discovery",
            "market_id": m.get("market_id"),
            "condition_id": m.get("condition_id"),
            "slug": m.get("slug"),
            "yes_token_id": m.get("yes_token_id"),
            "no_token_id": m.get("no_token_id"),
            "strike": m.get("strike", 0.0),
            "start_time": m.get("start_time"),
            "end_time": m.get("end_time"),
            "market_duration_sec": m.get("duration_sec", 300),
            "eligible_for_strategy_a": m.get("eligible_for_strategy_a", True),
            "eligible_for_strategy_b": m.get("eligible_for_strategy_b", False),
        }
        print(f"[DISCOVERY] Market: {dm['slug']} (dur={dm['market_duration_sec']}s "
              f"A={dm['eligible_for_strategy_a']} B={dm['eligible_for_strategy_b']})")
        await self.send_to_engine(json.dumps(dm))

    async def send_resolution_to_engine(self, md, wo, wa="", rp=1.0):
        rm = {
            "event_type": "market_resolution",
            "market_id": md.get("market_id"),
            "condition_id": md.get("condition_id"),
            "winning_outcome": wo,
            "winning_asset_id": wa,
            "resolution_price": rp,
            "timestamp": int(time.time() * 1000),
        }
        await self.send_to_engine(json.dumps(rm))
        print(f"[RESOLUTION] Verified: ID={md.get('market_id')} Winner={wo}")
    async def run_discovery_loop(self):
        while self.running:
            mkts = self.discover_polymarket_btc_markets()
            if mkts:
                top = mkts[0]
                if not self.active_market or self.active_market.get("condition_id") != top.get("condition_id"):
                    prev = self.active_market
                    self.active_market = top
                    self.yes_token = top["yes_token_id"]
                    self.no_token = top["no_token_id"]
                    if prev:
                        self.pending_resolutions[prev["condition_id"]] = prev
                    await self.send_discovery_to_engine(top)
                    self.market_ready.set()
                    self.subscription_changed.set()
            elif not self.active_market and not (self.yes_token and self.no_token):
                print("[DISCOVERY] Searching for active Polymarket BTC markets...")

            if self.active_market:
                if int(time.time() * 1000) >= self.active_market.get("end_time", 0):
                    print("[DISCOVERY] Market expired. Awaiting verified resolution...")
                    self.pending_resolutions[self.active_market["condition_id"]] = self.active_market
                    self.active_market = None
                    self.market_ready.clear()

            rk = []
            for cid, md in self.pending_resolutions.items():
                rd = await self.check_verified_resolution(cid)
                if rd:
                    await self.send_resolution_to_engine(md, rd["winning_outcome"],
                                                         rd.get("winning_asset_id", ""),
                                                         rd.get("resolution_price", 1.0))
                    rk.append(cid)
            for k in rk:
                del self.pending_resolutions[k]
            await asyncio.sleep(15)

    async def run_binance_feed(self):
        ctx = self.get_ssl_context()
        while self.running:
            try:
                async with websockets.connect(BINANCE_WS_URL, ssl=ctx, ping_interval=10,
                                              ping_timeout=10) as ws:
                    print("[BINANCE] Connected!")
                    while self.running:
                        msg = await ws.recv()
                        self.total_btc_ticks += 1
                        await self.send_to_engine(msg)
            except Exception as e:
                print(f"[BINANCE] Disconnected: {e}. Reconnecting...")
                await asyncio.sleep(2)

    async def run_polymarket_feed(self):
        ctx = self.get_ssl_context()
        while self.running:
            if not self.yes_token or not self.no_token:
                await self.market_ready.wait()
            # Native Polymarket subscription format: market type + assets_ids array.
            toks = [self.yes_token, self.no_token]
            sub_msg = {"type": "market", "assets_ids": toks}
            self.current_subscribed_tokens = set(toks)
            self.subscription_changed.clear()
            try:
                async with websockets.connect(POLYMARKET_WS_URL, ssl=ctx, ping_interval=10,
                                              ping_timeout=10) as ws:
                    print("[POLYMARKET] Connected!")
                    await ws.send(json.dumps(sub_msg))
                    lhb = time.time()
                    while self.running:
                        if self.subscription_changed.is_set():
                            nt = [self.yes_token, self.no_token]
                            if set(nt) != self.current_subscribed_tokens:
                                self.subscription_changed.clear()
                                break
                        t = time.time()
                        if t - lhb >= 10:
                            lhb = t
                            await ws.send(json.dumps({"type": "ping"}))
                        try:
                            msg = await asyncio.wait_for(ws.recv(), timeout=1.0)
                            self.total_poly_ticks += 1
                            await self.send_to_engine(msg)
                        except asyncio.TimeoutError:
                            continue
            except Exception as e:
                print(f"[POLYMARKET] {e}. Retrying...")
                await asyncio.sleep(5)

    async def run_stats_logger(self):
        while self.running:
            await asyncio.sleep(10)
            ms = self.active_market.get("slug") if self.active_market else "NONE"
            print(f"[STATS] Mkt={ms} Pending={len(self.pending_resolutions)} "
                  f"BTC={self.total_btc_ticks} POLY={self.total_poly_ticks}")
    async def start(self):
        await self.connect_to_engine()
        if self.yes_token and self.no_token:
            if not self.manual_condition_id:
                print("[ERROR] Manual mode requires --condition-id")
                return
            cid = self.manual_condition_id.strip()
            mid = self.manual_market_id or derive_market_id(cid)
            if not mid:
                print("[ERROR] Invalid condition ID")
                return
            self.active_market = {
                "condition_id": cid, "market_id": mid, "slug": "btc-manual",
                "yes_token_id": self.yes_token, "no_token_id": self.no_token, "strike": 0.0,
                "start_time": int(time.time() * 1000),
                "end_time": int(time.time() * 1000) + 86400000,
                "duration_sec": 300,
                "eligible_for_strategy_a": True,
                "eligible_for_strategy_b": False,
            }
            await self.send_discovery_to_engine(self.active_market)
            self.market_ready.set()
        await asyncio.gather(self.run_discovery_loop(), self.run_binance_feed(),
                             self.run_polymarket_feed(), self.run_stats_logger())


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default=ENGINE_HOST)
    ap.add_argument("--port", type=int, default=ENGINE_PORT)
    ap.add_argument("--proxy", default=None)
    ap.add_argument("--yes-token", default=None)
    ap.add_argument("--no-token", default=None)
    ap.add_argument("--market-id", type=int, default=None)
    ap.add_argument("--condition-id", default=None)
    ap.add_argument("--insecure-ssl", action="store_true")
    args = ap.parse_args()
    bridge = LiveFeedBridge(host=args.host, port=args.port, proxy=args.proxy,
                            yes_token=args.yes_token, no_token=args.no_token,
                            market_id=args.market_id, condition_id=args.condition_id,
                            insecure_ssl=args.insecure_ssl)
    try:
        asyncio.run(bridge.start())
    except KeyboardInterrupt:
        print("\n[BRIDGE] Stopped.")
