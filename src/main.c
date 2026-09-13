#include "core/ptypes.h"
#include "util/pt_clock.h"
#include "util/pt_ring.h"
#include "orderbook/pt_book.h"
#include "features/pt_features.h"
#include "strategies/pt_flow_skew.h"
#include "strategies/pt_signal.h"
#include "execution/pt_arb.h"
#include "execution/pt_arb_mgr.h"
#include "execution/pt_broker.h"
#include "risk/pt_risk.h"
#include "portfolio/pt_portfolio.h"
#include "storage/pt_csv_log.h"
#include "telemetry/pt_telemetry.h"
#include "net/pt_reactor.h"
#include "net/pt_http_server.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile int g_running = 1;
static void sig_handler(int sig) { (void)sig; g_running = 0; }

int main(int argc, char **argv)
{
    printf("====================================================\n");
    printf("  POLYMARKET HFT ARBITRAGE & MOMENTUM TRADING ENGINE \n");
    printf("  C11 Pure Engine | Event-Driven Low-Latency Core   \n");
    printf("====================================================\n");

    const char *live_env = getenv("LIVE_TRADING");
    if (live_env && strcmp(live_env, "true") == 0) {
        fprintf(stderr, "[FATAL SAFETY] LIVE_TRADING=true is permanently locked.\n");
        return 1;
    }
    printf("[SAFETY] Execution Mode: PAPER TRADING (live keys disabled)\n");

    int port = 8080;
    if (argc > 1) port = atoi(argv[1]);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    pt_reactor_t reactor;
    if (pt_reactor_init(&reactor) != 0) return 1;

    pt_book_t yes_book, no_book;
    pt_book_init(&yes_book);
    pt_book_init(&no_book);

    pt_level_t y_bids[3] = { {480, 500}, {485, 300}, {490, 200} };
    pt_level_t y_asks[3] = { {495, 250}, {500, 400}, {505, 600} };
    pt_book_snapshot(&yes_book, PT_SIDE_BID, y_bids, 3, 1);
    pt_book_snapshot(&yes_book, PT_SIDE_ASK, y_asks, 3, 1);

    pt_level_t n_bids[3] = { {485, 400}, {490, 300}, {495, 200} };
    pt_level_t n_asks[3] = { {500, 300}, {505, 500}, {510, 800} };
    pt_book_snapshot(&no_book, PT_SIDE_BID, n_bids, 3, 1);
    pt_book_snapshot(&no_book, PT_SIDE_ASK, n_asks, 3, 1);

    pt_market_features_t yes_feat, no_feat;
    memset(&yes_feat, 0, sizeof(yes_feat));
    memset(&no_feat, 0, sizeof(no_feat));

    pt_btc_t btc_engine;
    pt_btc_init(&btc_engine, 1024);
    double btc_price = 87500.0;
    pt_btc_add(&btc_engine, pt_clock_mono_ns(), btc_price);

    pt_arb_cfg_t arb_cfg = {
        .taker_fee_bps = 2.0, .slippage_bps = 0.0,
        .latency_buffer_pct = 0.001, .risk_buffer_pct = 0.001,
        .min_edge_pct = 0.005, .min_liquidity = 10
    };
    pt_arb_mgr_cfg_t arb_mgr_cfg = {
        .max_loss_per_trade = 10.0, .requote_tolerance = 0.005, .allow_requote = 1
    };
    pt_arb_mgr_t arb_mgr;
    pt_arb_mgr_init(&arb_mgr, &arb_cfg, &arb_mgr_cfg, &yes_book, &no_book);

    pt_flow_skew_cfg_t skew_cfg = {
        .min_btc_momentum_pct = 0.0002, .min_btc_velocity = 10.0,
        .min_polymarket_imbalance = 0.10, .max_polymarket_spread = 0.03,
        .min_expected_edge = 0.005, .min_confidence = 0.50,
        .default_order_size = 100, .time_to_expiry_min_sec = 30.0,
        .time_to_expiry_max_sec = 900.0
    };

    pt_risk_cfg_t risk_cfg = {
        .max_position_per_market = 2000.0, .max_total_exposure = 10000.0,
        .max_daily_loss = 100.0, .max_drawdown = 150.0,
        .max_polymarket_stale_ns = 5000000000ULL, .max_binance_stale_ns = 5000000000ULL,
        .max_spread_for_entry = 0.04, .max_consecutive_losses = 5,
        .min_confidence_floor = 0.40
    };
    pt_risk_engine_t risk_engine;
    pt_risk_init(&risk_engine, &risk_cfg);

    pt_portfolio_t portfolio;
    pt_portfolio_init(&portfolio, 5000.0);

    pt_broker_cfg_t broker_cfg = {
        .latency_submit_ack_ms = 2.0, .latency_ack_fill_ms = 5.0,
        .slippage_bps = 1.0, .live_trading_enabled = 0
    };
    pt_broker_t broker;
    pt_broker_init(&broker, &broker_cfg, &portfolio);

    pt_telemetry_t telemetry;
    pt_telemetry_init(&telemetry);

    pt_csv_logger_t csv_log;
    pt_csv_init(&csv_log, "data/logs");

    pt_http_server_t http_server;
    if (pt_http_server_init(&http_server, port, &reactor, &portfolio,
                            &telemetry, &risk_engine, &yes_book,
                            &no_book, PT_MODE_PAPER) == 0) {
        printf("[HTTP] Monitoring API listening on http://127.0.0.1:%d/api/status\n", port);
    }
    uint64_t tick_count = 0;
    pt_nsec_t last_stat_print = pt_clock_mono_ns();

    while (g_running) {
        pt_nsec_t t0 = pt_clock_mono_ns();
        pt_risk_feed_touch_poly(&risk_engine, t0);
        pt_risk_feed_touch_binance(&risk_engine, t0);

        pt_reactor_poll(&reactor, 10);

        tick_count++;
        telemetry.poly_events_total++;
        telemetry.btc_events_total++;

        double btc_delta = (((rand() % 100) - 50) / 10.0);
        btc_price += btc_delta;
        pt_btc_add(&btc_engine, t0, btc_price);
        pt_http_server_update_btc(&http_server, btc_price);

        if (tick_count % 50 == 0) {
            pt_level_t y_arb[1] = { {480, 150} };
            pt_level_t n_arb[1] = { {490, 150} };
            pt_book_snapshot(&yes_book, PT_SIDE_ASK, y_arb, 1, tick_count);
            pt_book_snapshot(&no_book, PT_SIDE_ASK, n_arb, 1, tick_count);
        } else if (tick_count % 50 == 5) {
            pt_level_t y_norm[2] = { {495, 250}, {500, 400} };
            pt_level_t n_norm[2] = { {500, 300}, {505, 500} };
            pt_book_snapshot(&yes_book, PT_SIDE_ASK, y_norm, 2, tick_count);
            pt_book_snapshot(&no_book, PT_SIDE_ASK, n_norm, 2, tick_count);
        }

        pt_book_features_compute(&yes_book, &yes_feat.book);
        pt_book_features_compute(&no_book, &no_feat.book);
        pt_btc_window_t btc_win[PT_BTC_NTF];
        double last_btc = 0; int have_btc = 0;
        pt_btc_snapshot(&btc_engine, t0, btc_win, &last_btc, &have_btc);

        pt_arb_opp_t arb_opp;
        if (pt_arb_calc(&yes_book, &no_book, &arb_cfg, 200, &arb_opp) &&
            arb_opp.has_opportunity) {
            pt_signal_t sig = {
                .signal_id = tick_count * 10 + 1,
                .strategy  = PT_STRAT_PARITY5M,
                .market_id = 101,
                .timestamp = t0,
                .direction = PT_DIR_BUY,
                .is_yes    = 1,
                .target_price = (pt_price_t)arb_opp.avg_yes_scaled,
                .expected_edge = arb_opp.net_edge,
                .expected_profit = arb_opp.expected_profit,
                .confidence = arb_opp.confidence,
                .fill_probability = 0.95,
                .max_size = arb_opp.max_executable_size,
                .time_to_expiry = 240.0,
                .poly_spread = yes_feat.book.spread
            };
            telemetry.signals_generated++;
            pt_csv_log_signal(&csv_log, &sig);

            pt_size_t approved = 0;
            double exp = pt_portfolio_market_exposure(&portfolio, 101);
            int rej = pt_risk_evaluate_signal(&risk_engine, &sig, exp, t0, &approved);
            if (rej == PT_REJECT_NONE && approved > 0) {
                pt_arb_op_t op;
                int op_idx = pt_arb_mgr_open(&arb_mgr, &arb_opp, &op, 0, t0);
                if (op_idx >= 0) {
                    pt_order_id_t y_oid = pt_broker_submit(&broker, 101, 1, PT_SIDE_BID,
                                                           (pt_price_t)arb_opp.avg_yes_scaled,
                                                           approved, &yes_book,
                                                           PT_STRAT_PARITY5M, t0);
                    pt_order_id_t n_oid = pt_broker_submit(&broker, 101, 0, PT_SIDE_BID,
                                                           (pt_price_t)arb_opp.avg_no_scaled,
                                                           approved, &no_book,
                                                           PT_STRAT_PARITY5M, t0);
                    arb_mgr.ops[op_idx].yes_oid = y_oid;
                    arb_mgr.ops[op_idx].no_oid  = n_oid;
                    telemetry.orders_submitted += 2;
                }
            } else {
                pt_csv_log_rejection(&csv_log, t0, sig.signal_id, pt_risk_reject_str(rej), (double)rej);
            }
        }
        pt_flow_signal_t skew_sig;
        if (pt_flow_skew_eval(&skew_cfg, &yes_feat, &no_feat, btc_win,
                              btc_price, 600.0, &skew_sig) &&
            skew_sig.has_signal) {
            pt_signal_t sig = {
                .signal_id = tick_count * 10 + 2,
                .strategy  = PT_STRAT_FLOW15M,
                .market_id = 101,
                .timestamp = t0,
                .direction = skew_sig.dir,
                .is_yes    = skew_sig.is_yes,
                .target_price = skew_sig.target_price,
                .expected_edge = skew_sig.expected_edge,
                .expected_profit = skew_sig.expected_profit,
                .confidence = skew_sig.confidence,
                .fill_probability = skew_sig.fill_probability,
                .max_size = skew_sig.max_size,
                .time_to_expiry = 600.0,
                .poly_spread = skew_sig.poly_spread,
                .poly_imb_l3 = skew_sig.poly_imb_l3,
                .btc_mom_1s  = skew_sig.btc_mom_1s
            };
            telemetry.signals_generated++;
            pt_csv_log_signal(&csv_log, &sig);

            pt_size_t approved = 0;
            double exp = pt_portfolio_market_exposure(&portfolio, 101);
            int rej = pt_risk_evaluate_signal(&risk_engine, &sig, exp, t0, &approved);
            if (rej == PT_REJECT_NONE && approved > 0) {
                pt_broker_submit(&broker, 101, sig.is_yes, PT_SIDE_BID,
                                 sig.target_price, approved,
                                 sig.is_yes ? &yes_book : &no_book,
                                 PT_STRAT_FLOW15M, t0);
                telemetry.orders_submitted++;
            }
        }

        int fills = pt_broker_tick(&broker, 101, &yes_book, &no_book, t0, NULL, NULL);
        if (fills > 0) telemetry.orders_filled += fills;

        pt_nsec_t t1 = pt_clock_mono_ns();
        pt_telemetry_record_sample(&telemetry.loop_tick_time, t1 - t0);
        pt_telemetry_record_sample(&telemetry.e2e_latency, t1 - t0 + 3500ULL);

        if (t1 - last_stat_print >= 3000000000ULL) {
            last_stat_print = t1;
            printf("[STATUS] ticks=%llu | BTC=%.1f | Cash=$%.2f | Equity=$%.2f | Trades=%llu | WinRate=%.1f%% | p50=%.1fus\n",
                   (unsigned long long)tick_count,
                   btc_price,
                   portfolio.cash,
                   pt_portfolio_equity(&portfolio),
                   (unsigned long long)portfolio.trades_count,
                   pt_portfolio_win_rate(&portfolio) * 100.0,
                   pt_telemetry_percentile_us(&telemetry.e2e_latency, 50.0));
        }
    }

    printf("\n[ENGINE] Shutting down...\n");
    pt_http_server_stop(&http_server);
    pt_reactor_destroy(&reactor);
    pt_btc_destroy(&btc_engine);
    pt_csv_close(&csv_log);
    printf("[ENGINE] Shutdown complete.\n");
    return 0;
}