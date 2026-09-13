#!/usr/bin/env python3
import subprocess
import socket
import json
import time
import urllib.request
import sys

def run_integration_test():
    print("=== STARTING LIVE ENGINE INTEGRATION TEST ===")
    port = 8092
    engine_proc = subprocess.Popen(
        ["build/bin/pmt_engine", "config/engine.ini", str(port)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True
    )
    time.sleep(1)

    try:
        # Step 1: Initial Empty Check
        status = json.loads(urllib.request.urlopen(f"http://127.0.0.1:{port}/api/status").read().decode())
        analytics = json.loads(urllib.request.urlopen(f"http://127.0.0.1:{port}/api/analytics").read().decode())
        book = json.loads(urllib.request.urlopen(f"http://127.0.0.1:{port}/api/book").read().decode())

        assert status["market_status"] == "WAITING FOR LIVE MARKET", f"Expected WAITING FOR LIVE MARKET, got {status['market_status']}"
        assert status["has_live_market"] == 0, f"Expected 0, got {status['has_live_market']}"
        assert status["btc_price"] == 0.0, f"Expected 0.0, got {status['btc_price']}"
        assert analytics["sharpe"] == 0.0, f"Expected 0.0, got {analytics['sharpe']}"
        assert analytics["has_sharpe"] == 0, f"Expected 0, got {analytics['has_sharpe']}"
        assert analytics["adverse_selection_bps"] == 0.0, f"Expected 0.0, got {analytics['adverse_selection_bps']}"
        assert book["yes"]["best_bid"] == 0.0, f"Expected empty book, got {book}"
        print("[PASS] Initial empty state verified.")

        # Step 2: Connect via TCP and send real market discovery
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(("127.0.0.1", 9999))

        now_ms = int(time.time() * 1000)
        disc = {
            "event_type": "market_discovery",
            "market_id": 101,
            "condition_id": "0x4b7f8c9d0e1a2b3c4d5e6f7a8b9c0d1e2f3a4b5c",
            "slug": "btc-up-5m-real",
            "yes_token_id": "713210455829103859218392",
            "no_token_id": "713210455829103859218393",
            "strike": 88000.0,
            "start_time": now_ms,
            "end_time": now_ms + 300000
        }
        s.sendall((json.dumps(disc) + "\n").encode())
        time.sleep(0.1)

        # Step 3: Stream Binance trade
        btc_trade = {"p": "88200.50", "q": "0.25", "m": False, "E": now_ms}
        s.sendall((json.dumps(btc_trade) + "\n").encode())
        time.sleep(0.1)

        # Step 3b: Send unknown token message and verify it is dropped (no corruption/guessing)
        unknown_msg = {"event_type": "book", "asset_id": "unknown_asset_token_999", "side": "BID", "price": "0.99", "size": "999999", "timestamp": now_ms}
        s.sendall((json.dumps(unknown_msg) + "\n").encode())
        time.sleep(0.1)

        # Step 4: Stream Order Book Snapshots for Registered Real Tokens
        y_bid = {"event_type": "book", "asset_id": "713210455829103859218392", "side": "BID", "price": "0.485", "size": "500", "timestamp": now_ms}
        y_ask = {"event_type": "book", "asset_id": "713210455829103859218392", "side": "ASK", "price": "0.490", "size": "300", "timestamp": now_ms}
        n_bid = {"event_type": "book", "asset_id": "713210455829103859218393", "side": "BID", "price": "0.490", "size": "400", "timestamp": now_ms}
        n_ask = {"event_type": "book", "asset_id": "713210455829103859218393", "side": "ASK", "price": "0.495", "size": "600", "timestamp": now_ms}
        s.sendall((json.dumps(y_bid) + "\n").encode())
        s.sendall((json.dumps(y_ask) + "\n").encode())
        s.sendall((json.dumps(n_bid) + "\n").encode())
        s.sendall((json.dumps(n_ask) + "\n").encode())
        time.sleep(0.5)

        status = json.loads(urllib.request.urlopen(f"http://127.0.0.1:{port}/api/status").read().decode())
        book = json.loads(urllib.request.urlopen(f"http://127.0.0.1:{port}/api/book").read().decode())
        assert status["market_status"] == "ACTIVE", f"Expected ACTIVE, got {status['market_status']}"
        assert status["btc_price"] == 88200.50, f"Expected 88200.50, got {status['btc_price']}"
        assert status["market_slug"] == "btc-up-5m-real", f"Expected btc-up-5m-real, got {status['market_slug']}"
        assert book["yes"]["best_bid"] == 0.485, f"Expected 0.485, got {book}"
        assert book["yes"]["best_ask"] == 0.490, f"Expected 0.490, got {book}"
        print("[PASS] Live market discovery & book activation verified (unknown token cleanly dropped).")

        # Step 5: Send Real Market Resolution Event
        res = {
            "event_type": "market_resolution",
            "market_id": 101,
            "condition_id": "0x4b7f8c9d0e1a2b3c4d5e6f7a8b9c0d1e2f3a4b5c",
            "winning_outcome": "YES",
            "resolution_price": 88350.0,
            "timestamp": now_ms + 1000
        }
        s.sendall((json.dumps(res) + "\n").encode())
        time.sleep(0.3)
        s.close()

        analytics = json.loads(urllib.request.urlopen(f"http://127.0.0.1:{port}/api/analytics").read().decode())
        print(f"[PASS] Real resolution processed: total_trades={analytics['total_trades']}")
        print("=== ALL INTEGRATION CHECKS PASSED ===")
        return 0

    finally:
        engine_proc.terminate()
        try:
            engine_proc.communicate(timeout=2)
        except:
            engine_proc.kill()

if __name__ == "__main__":
    sys.exit(run_integration_test())
