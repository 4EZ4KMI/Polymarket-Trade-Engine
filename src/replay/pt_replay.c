#include "core/ptypes.h"
#include "util/pt_clock.h"
#include "orderbook/pt_book.h"
#include "features/pt_features.h"
#include "strategies/pt_flow_skew.h"
#include "execution/pt_arb.h"
#include "execution/pt_arb_mgr.h"
#include "execution/pt_broker.h"
#include "risk/pt_risk.h"
#include "portfolio/pt_portfolio.h"
#include "analytics/pt_calibration.h"
#include "analytics/pt_lifecycle_tracker.h"
#include "analytics/pt_metrics.h"
#include "storage/pt_dataset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void create_synthetic_dataset(const char *path)
{
    pt_dataset_writer_t w;
    if (pt_dataset_writer_open(&w, path) != 0) return;

    pt_nsec_t t = 1000000000ULL;
    pt_dataset_event_t ev1 = { .event_type = PT_DATA_EV_POLY_SNAP, .timestamp_ns = t, .market_id = 101, .is_yes = 1, .side = PT_SIDE_BID, .price = 480, .size = 500 };
    pt_dataset_writer_append(&w, &ev1);
    pt_dataset_event_t ev2 = { .event_type = PT_DATA_EV_POLY_SNAP, .timestamp_ns = t, .market_id = 101, .is_yes = 1, .side = PT_SIDE_ASK, .price = 490, .size = 200 };
    pt_dataset_writer_append(&w, &ev2);
    pt_dataset_event_t ev3 = { .event_type = PT_DATA_EV_POLY_SNAP, .timestamp_ns = t, .market_id = 101, .is_yes = 0, .side = PT_SIDE_BID, .price = 490, .size = 300 };
    pt_dataset_writer_append(&w, &ev3);
    pt_dataset_event_t ev4 = { .event_type = PT_DATA_EV_POLY_SNAP, .timestamp_ns = t, .market_id = 101, .is_yes = 0, .side = PT_SIDE_ASK, .price = 495, .size = 400 };
    pt_dataset_writer_append(&w, &ev4);

    double btc = 87500.0;
    for (int i = 0; i < 500; i++) {
        t += 10000000ULL; /* 10ms steps */
        double move = ((i % 10 < 5) ? 1.5 : -1.2);
        btc += move;
        pt_dataset_event_t btc_ev = {
            .event_type = PT_DATA_EV_BTC_TICK,
            .timestamp_ns = t,
            .btc_mid = btc,
            .btc_bid = btc - 0.5,
            .btc_ask = btc + 0.5
        };
        pt_dataset_writer_append(&w, &btc_ev);

        if (i % 20 == 0) {
            pt_dataset_event_t arb_ev = {
                .event_type = PT_DATA_EV_POLY_DELTA,
                .timestamp_ns = t,
                .market_id = 101,
                .is_yes = 0,
                .side = PT_SIDE_ASK,
                .price = 485,
                .size = 250
            };
            pt_dataset_writer_append(&w, &arb_ev);
        }
        if (i % 30 == 0) {
            pt_dataset_event_t tr_ev = {
                .event_type = PT_DATA_EV_POLY_TRADE,
                .timestamp_ns = t,
                .market_id = 101,
                .is_yes = 1,
                .side = PT_SIDE_ASK,
                .price = 490,
                .size = 150
            };
            pt_dataset_writer_append(&w, &tr_ev);
        }
    }
    pt_dataset_writer_close(&w);
}

int main(int argc, char **argv)
{
    printf("====================================================\n");
    printf("  POLYMARKET DETERMINISTIC EVENT REPLAY BACKTESTER   \n");
    printf("====================================================\n");

    const char *dataset_path = (argc > 1) ? argv[1] : "data/sample_dataset.bin";

    pt_dataset_reader_t reader;
    if (pt_dataset_reader_open(&reader, dataset_path) != 0) {
        fprintf(stderr, "[REPLAY] Creating synthetic test dataset at '%s'...\n", dataset_path);
        create_synthetic_dataset(dataset_path);
        if (pt_dataset_reader_open(&reader, dataset_path) != 0) {
            fprintf(stderr, "[FATAL] Failed to initialize dataset reader.\n");
            return 1;
        }
    }

    printf("[REPLAY] Loaded dataset: %llu events\n", (unsigned long long)reader.header.total_events);

    pt_portfolio_t port;
    pt_portfolio_init(&port, 10000.0);

    pt_broker_cfg_t bcfg = {
        .latency_submit_ack_ms = 1.5,
        .latency_ack_fill_ms = 1.0,
        .latency_cancel_ms = 1.5,
        .queue_model = PT_QUEUE_MODEL_REALISTIC,
        .slippage_bps = 1.0,
        .adverse_selection_bps = 2.0,
        .live_trading_enabled = 0
    };
    pt_broker_t broker;
    pt_broker_init(&broker, &bcfg, &port);

    pt_book_t yes_book, no_book;
    pt_book_init(&yes_book);
    pt_book_init(&no_book);

    pt_btc_t btc_eng;
    pt_btc_init(&btc_eng, 1024);

    pt_arb_cfg_t arb_cfg = {
        .fees = { .taker_fee_bps = 2.0, .maker_fee_bps = 0.0, .maker_rebate_bps = 0.5, .dynamic_fee_scale = 1.0 },
        .slippage_bps = 1.0,
        .latency_buffer_pct = 0.001,
        .risk_buffer_pct = 0.001,
        .min_edge_pct = 0.005,
        .min_liquidity = 10
    };
    pt_arb_mgr_cfg_t arb_mgr_cfg = {
        .max_loss_per_trade = 10.0,
        .requote_tolerance = 0.005,
        .allow_requote = 1,
        .max_unhedged_time_ms = 500.0,
        .max_unhedged_size = 500,
        .hedge_timeout_ms = 1000.0
    };
    pt_arb_mgr_t arb_mgr;
    pt_arb_mgr_init(&arb_mgr, &arb_cfg, &arb_mgr_cfg, &yes_book, &no_book);

    pt_flow_skew_cfg_t skew_cfg = {
        .model = { .type = PT_MODEL_HEURISTIC },
        .min_btc_momentum_pct = 0.0001,
        .min_btc_velocity = 5.0,
        .min_polymarket_imbalance = 0.05,
        .max_polymarket_spread = 0.04,
        .min_expected_edge = 0.005,
        .min_confidence = 0.50,
        .default_order_size = 100,
        .time_to_expiry_min_sec = 30.0,
        .time_to_expiry_max_sec = 900.0,
        .taker_fee_bps = 2.0,
        .slippage_bps = 1.0
    };

    pt_risk_cfg_t risk_cfg = {
        .max_position_per_market = 2000.0,
        .max_total_exposure = 10000.0,
        .max_daily_loss = 200.0,
        .max_drawdown = 300.0,
        .max_polymarket_stale_ns = 5000000000ULL,
        .max_binance_stale_ns = 2000000000ULL,
        .max_spread_for_entry = 0.04,
        .max_consecutive_losses = 5,
        .min_confidence_floor = 0.50
    };
    pt_risk_engine_t risk;
    pt_risk_init(&risk, &risk_cfg);

    pt_metrics_collector_t metrics;
    pt_metrics_init(&metrics);

    pt_dataset_event_t ev;
    uint64_t processed = 0;
    double last_btc = 87500.0;
    pt_nsec_t wall_start = pt_clock_mono_ns();

    while (pt_dataset_reader_next(&reader, &ev)) {
        processed++;
        pt_nsec_t now = ev.timestamp_ns;

        if (ev.event_type == PT_DATA_EV_BTC_TICK) {
            pt_btc_add(&btc_eng, now, ev.btc_mid);
            last_btc = ev.btc_mid;
            pt_risk_feed_touch_binance(&risk, now);
        } else if (ev.event_type == PT_DATA_EV_POLY_DELTA || ev.event_type == PT_DATA_EV_POLY_SNAP) {
            pt_book_t *b = ev.is_yes ? &yes_book : &no_book;
            pt_book_update(b, ev.side, ev.price, (int64_t)ev.size, (ev.event_type == PT_DATA_EV_POLY_DELTA), 1);
            pt_broker_on_level_change(&broker, ev.market_id, ev.is_yes, ev.side, ev.price, (int64_t)ev.size + 100, (int64_t)ev.size, now);
            pt_risk_feed_touch_poly(&risk, now);
        } else if (ev.event_type == PT_DATA_EV_POLY_TRADE) {
            pt_broker_on_trade(&broker, ev.market_id, ev.is_yes, ev.price, ev.size, ev.side, now);
        }

        /* Strategy evaluations */
        pt_btc_window_t btc_win[PT_BTC_NTF];
        int have_btc = 0;
        pt_btc_snapshot(&btc_eng, now, btc_win, NULL, &have_btc);

        pt_market_features_t yf, nf;
        pt_book_features_compute(&yes_book, &yf.book);
        pt_book_features_compute(&no_book, &nf.book);

        pt_arb_opp_t opp;
        if (pt_arb_calc(&yes_book, &no_book, &arb_cfg, 200, &opp) && opp.has_opportunity) {
            pt_signal_t sig = {
                .signal_id = processed * 10 + 1,
                .strategy = PT_STRAT_PARITY5M,
                .market_id = 101,
                .timestamp = now,
                .direction = PT_DIR_BUY,
                .is_yes = 1,
                .target_price = (pt_price_t)opp.avg_yes_scaled,
                .expected_edge = opp.net_edge,
                .expected_profit = opp.expected_net_profit,
                .confidence = opp.confidence,
                .fill_probability = opp.joint_fill_probability,
                .max_size = opp.max_executable_size
            };
            pt_size_t approved = 0;
            double exp = pt_portfolio_market_exposure(&port, 101);
            if (pt_risk_evaluate_signal(&risk, &sig, exp, now, &approved) == PT_REJECT_NONE && approved > 0) {
                pt_arb_op_t op;
                int op_idx = pt_arb_mgr_open(&arb_mgr, &opp, &op, 0, now);
                if (op_idx >= 0) {
                    pt_order_id_t y_oid = pt_broker_submit(&broker, 101, 1, PT_SIDE_BID, (pt_price_t)opp.avg_yes_scaled, approved, &yes_book, PT_STRAT_PARITY5M, now);
                    pt_order_id_t n_oid = pt_broker_submit(&broker, 101, 0, PT_SIDE_BID, (pt_price_t)opp.avg_no_scaled, approved, &no_book, PT_STRAT_PARITY5M, now);
                    arb_mgr.ops[op_idx].yes_oid = y_oid;
                    arb_mgr.ops[op_idx].no_oid  = n_oid;
                    metrics.summary.orders_submitted += 2;
                }
            }
        }

        pt_flow_signal_t skew_sig;
        if (have_btc && pt_flow_skew_eval(&skew_cfg, &yf, &nf, btc_win, last_btc, 600.0, &skew_sig) && skew_sig.has_signal) {
            pt_signal_t sig = {
                .signal_id = processed * 10 + 2,
                .strategy = PT_STRAT_FLOW15M,
                .market_id = 101,
                .timestamp = now,
                .direction = skew_sig.dir,
                .is_yes = skew_sig.is_yes,
                .target_price = skew_sig.target_price,
                .expected_edge = skew_sig.executable_edge,
                .expected_profit = skew_sig.expected_profit,
                .confidence = skew_sig.confidence,
                .fill_probability = skew_sig.fill_probability,
                .max_size = skew_sig.max_size
            };
            pt_size_t approved = 0;
            double exp = pt_portfolio_market_exposure(&port, 101);
            if (pt_risk_evaluate_signal(&risk, &sig, exp, now, &approved) == PT_REJECT_NONE && approved > 0) {
                pt_broker_submit(&broker, 101, sig.is_yes, PT_SIDE_BID, sig.target_price, approved, sig.is_yes ? &yes_book : &no_book, PT_STRAT_FLOW15M, now);
                metrics.summary.orders_submitted++;
            }
        }

        int fills = pt_broker_tick(&broker, 101, &yes_book, &no_book, now, NULL, NULL);
        if (fills > 0) metrics.summary.orders_filled += fills;
    }

    pt_nsec_t wall_end = pt_clock_mono_ns();
    double elapsed_sec = (double)(wall_end - wall_start) / 1e9;

    pt_dataset_reader_close(&reader);
    pt_metrics_compute(&metrics, 10000.0);

    printf("\n==================== REPLAY SUMMARY ====================\n");
    printf("Processed Events: %llu in %.3f sec (%.0f events/sec)\n",
           (unsigned long long)processed, elapsed_sec, (double)processed / (elapsed_sec > 0 ? elapsed_sec : 1.0));
    printf("Final Cash:       $%.2f\n", port.cash);
    printf("Final Equity:     $%.2f\n", pt_portfolio_equity(&port));
    printf("Realized PnL:     $%.2f\n", port.realized_pnl);
    printf("Trades Completed: %llu | Win Rate: %.1f%%\n",
           (unsigned long long)port.trades_count, pt_portfolio_win_rate(&port) * 100.0);
    printf("Orders Submitted: %llu | Orders Filled: %llu\n",
           (unsigned long long)metrics.summary.orders_submitted, (unsigned long long)metrics.summary.orders_filled);
    printf("Profit Factor:    %.2f | Expectancy: $%.3f\n",
           metrics.summary.profit_factor, metrics.summary.expectancy);
    printf("Max Drawdown:     $%.2f (%.2f%%)\n",
           metrics.summary.max_drawdown_usd, metrics.summary.max_drawdown_pct);
    printf("Sharpe Ratio:     %.2f | Sortino Ratio: %.2f\n",
           metrics.summary.sharpe_ratio, metrics.summary.sortino_ratio);
    printf("========================================================\n");

    return 0;
}

