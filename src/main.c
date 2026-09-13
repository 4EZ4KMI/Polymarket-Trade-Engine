#include "core/ptypes.h"
#include "core/pt_config.h"
#include "core/pt_market_registry.h"
#include "core/pt_market_lifecycle.h"
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
#include "analytics/pt_calibration.h"
#include "analytics/pt_lifecycle_tracker.h"
#include "analytics/pt_strategy_stats.h"
#include "analytics/pt_adverse_selection.h"
#include "analytics/pt_metrics.h"
#include "telemetry/pt_telemetry.h"
#include "net/pt_reactor.h"
#include "net/pt_http_server.h"
#include "net/pt_feed_bridge.h"
#include "storage/pt_dataset.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

static volatile int g_running = 1;
static void sig_handler(int sig) { (void)sig; g_running = 0; }

typedef struct {
    pt_strategy_stats_tracker_t *stats;
    pt_lifecycle_tracker_t      *lifecycle;
    pt_adverse_tracker_t        *adverse;
    pt_csv_logger_t             *csv_log;
    pt_telemetry_t              *telemetry;
} engine_fill_ctx_t;

static void on_engine_fill_(const pt_order_t *order, pt_size_t filled_shares,
                            pt_price_t fill_price, void *ud)
{
    engine_fill_ctx_t *ctx = (engine_fill_ctx_t *)ud;
    if (!ctx || !order) return;

    pt_nsec_t now = pt_clock_mono_ns();
    double target_p = (double)order->price / PT_PRICE_SCALE;
    double fill_p = (double)fill_price / PT_PRICE_SCALE;
    double slippage_bps = (target_p > 0.0) ? (fabs(fill_p - target_p) / target_p * 10000.0) : 0.0;
    double latency_ms = (order->submit_t > 0 && now >= order->submit_t) ?
                        (double)(now - order->submit_t) / 1000000.0 : 0.0;
    double fee = (order->type == PT_OTYPE_LIMIT) ? 0.0 : (fill_p * (double)filled_shares * 0.001);
    double rebate = 0.0;

    if (ctx->stats) {
        pt_strat_stats_record_fill(ctx->stats, order->strategy, fill_p, filled_shares,
                                   slippage_bps, fee, rebate, latency_ms, order->queue_ahead_at_submit);
    }

    uint64_t fill_id = 0;
    if (ctx->adverse) {
        fill_id = pt_adverse_record_fill(ctx->adverse, now, order->market_id, order->strategy,
                                         order->is_yes, order->side, fill_price, filled_shares);
    }

    if (ctx->lifecycle) {
        pt_lifecycle_on_fill(ctx->lifecycle, order->signal_id, filled_shares, fill_price,
                             fill_p, latency_ms, fee, rebate, slippage_bps / 100.0, fill_id);
    }

    if (ctx->csv_log) {
        pt_csv_log_trade(ctx->csv_log, now, order->id, order->market_id,
                         order->strategy, order->is_yes, order->side, fill_price,
                         filled_shares, fee, rebate);
    }
}

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

    /* Load and Validate Configuration */
    pt_engine_config_t config;
    pt_config_set_defaults(&config);
    const char *cfg_path = (argc > 1) ? argv[1] : "config/engine.ini";
    if (pt_config_load_file(&config, cfg_path) == 0) {
        printf("[CONFIG] Loaded settings from %s\n", cfg_path);
    } else {
        printf("[CONFIG] Using default configuration\n");
    }

    char err_buf[256];
    if (pt_config_validate(&config, err_buf, sizeof(err_buf)) != 0) {
        fprintf(stderr, "[FATAL CONFIG] Validation error: %s\n", err_buf);
        return 1;
    }

    int port = config.http_port;
    if (argc > 2) port = atoi(argv[2]);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    pt_reactor_t reactor;
    if (pt_reactor_init(&reactor) != 0) return 1;

    /* Start with 100% EMPTY order books - NO synthetic orders */
    pt_book_t yes_book, no_book;
    pt_book_init(&yes_book);
    pt_book_init(&no_book);

    pt_market_features_t yes_feat, no_feat;
    memset(&yes_feat, 0, sizeof(yes_feat));
    memset(&no_feat, 0, sizeof(no_feat));

    /* BTC engine starts EMPTY - awaits real Binance trades */
    pt_btc_t btc_engine;
    pt_btc_init(&btc_engine, 1024);
    double btc_price = 0.0;

    pt_arb_mgr_t arb_mgr;
    pt_arb_mgr_init(&arb_mgr, &config.strategy_arb, &config.arb_mgr, &yes_book, &no_book);

    pt_risk_engine_t risk_engine;
    pt_risk_init(&risk_engine, &config.risk);

    pt_portfolio_t portfolio;
    pt_portfolio_init(&portfolio, config.initial_capital);

    pt_broker_t broker;
    pt_broker_init(&broker, &config.execution, &portfolio);

    /* Market Registry starts EMPTY - awaits discovery from live Polymarket API */
    pt_market_registry_t market_reg;
    pt_market_registry_init(&market_reg);

    pt_calibration_tracker_t calibration;
    pt_calibration_init(&calibration);

    pt_lifecycle_tracker_t lc_tracker;
    pt_lifecycle_tracker_init(&lc_tracker);

    pt_adverse_tracker_t adverse_tracker;
    pt_adverse_tracker_init(&adverse_tracker);

    pt_strategy_stats_tracker_t strat_stats;
    pt_strat_stats_init(&strat_stats, &calibration, &lc_tracker, &adverse_tracker, &portfolio, "data/strategy_stats.json");

    pt_telemetry_t telemetry;
    pt_telemetry_init(&telemetry);

    pt_csv_logger_t csv_log;
    pt_csv_init(&csv_log, config.log_dir);

    pt_dataset_writer_t dataset_writer;
    pt_dataset_writer_open(&dataset_writer, config.dataset_path);

    pt_feed_bridge_t feed_bridge;
    pt_feed_bridge_init(&feed_bridge, &reactor, config.feed_port,
                        &yes_book, &no_book, &btc_engine,
                        &risk_engine, &telemetry, &dataset_writer,
                        &btc_price, &market_reg, &portfolio,
                        &broker, &strat_stats, &lc_tracker, &adverse_tracker);

    pt_http_server_t http_server;
    if (pt_http_server_init(&http_server, port, &reactor, &portfolio,
                            &telemetry, &risk_engine, &yes_book,
                            &no_book, PT_MODE_PAPER) == 0) {
        pt_http_server_set_stats(&http_server, &strat_stats);
        pt_http_server_set_registry(&http_server, &market_reg);
        printf("[HTTP] Monitoring API listening on http://127.0.0.1:%d/api/status\n", port);
    }

    engine_fill_ctx_t fill_ctx = {
        .stats = &strat_stats,
        .lifecycle = &lc_tracker,
        .adverse = &adverse_tracker,
        .csv_log = &csv_log,
        .telemetry = &telemetry
    };
    pt_broker_set_fill_callback(&broker, on_engine_fill_, &fill_ctx);

    uint64_t tick_count = 0;
    pt_nsec_t last_stat_print = pt_clock_mono_ns();

    printf("[ENGINE] Core online. Awaiting 100%% real market feeds via feed bridge...\n");

    while (g_running) {
        pt_nsec_t t0 = pt_clock_mono_ns();

        /* Poll non-blocking network I/O (Feeds, HTTP REST) */
        pt_reactor_poll(&reactor, 5);

        tick_count++;
        pt_http_server_update_btc(&http_server, btc_price);

        pt_market_entry_t *act_m = pt_market_registry_get_active(&market_reg);
        int has_books = (yes_book.bids.count > 0 && yes_book.asks.count > 0 &&
                         no_book.bids.count > 0 && no_book.asks.count > 0);

        pt_btc_window_t btc_win[PT_BTC_NTF];
        double last_btc = 0; int have_btc = 0;
        pt_btc_snapshot(&btc_engine, t0, btc_win, &last_btc, &have_btc);

        int can_trade = (act_m != NULL && has_books && have_btc && btc_price > 0.0 &&
                         pt_market_entry_can_trade(act_m, t0));
        double tte = act_m ? pt_market_entry_time_to_expiry_sec(act_m, t0) : 0.0;
        pt_market_id_t active_mid = act_m ? act_m->market_id : 0;

        if (has_books) {
            pt_book_features_compute(&yes_book, &yes_feat.book);
            pt_book_features_compute(&no_book, &no_feat.book);
        }

        /* Strategy A: 5m Parity Arbitrage */
        pt_arb_opp_t arb_opp;
        if (can_trade && pt_arb_calc(&yes_book, &no_book, &config.strategy_arb, 200, &arb_opp) &&
            arb_opp.has_opportunity) {
            uint64_t sig_id = tick_count * 10 + 1;
            pt_signal_t sig = {
                .signal_id = sig_id,
                .strategy  = PT_STRAT_PARITY5M,
                .market_id = active_mid,
                .timestamp = t0,
                .direction = PT_DIR_BUY,
                .is_yes    = 1,
                .target_price = (pt_price_t)arb_opp.avg_yes_scaled,
                .expected_edge = arb_opp.net_edge,
                .expected_profit = arb_opp.expected_net_profit,
                .confidence = arb_opp.confidence,
                .fill_probability = arb_opp.joint_fill_probability,
                .max_size = arb_opp.max_executable_size,
                .time_to_expiry = tte,
                .poly_spread = yes_feat.book.spread
            };
            telemetry.signals_generated++;
            pt_csv_log_signal(&csv_log, &sig);

            pt_lifecycle_on_signal(&lc_tracker, sig_id, t0, active_mid, PT_STRAT_PARITY5M,
                                   1, PT_SIDE_BID, arb_opp.raw_edge, arb_opp.executable_edge,
                                   arb_opp.net_edge, arb_opp.joint_fill_probability,
                                   arb_opp.max_executable_size, arb_opp.expected_net_profit);

            pt_size_t approved = 0;
            double exp = pt_portfolio_market_exposure(&portfolio, active_mid);
            int rej = pt_risk_evaluate_signal(&risk_engine, &sig, exp, t0, &approved);
            int is_approved = (rej == PT_REJECT_NONE && approved > 0);
            pt_strat_stats_record_signal(&strat_stats, PT_STRAT_PARITY5M, is_approved, 1, arb_opp.net_edge, arb_opp.executable_edge);

            if (is_approved) {
                pt_arb_op_t op;
                int op_idx = pt_arb_mgr_open(&arb_mgr, &arb_opp, &op, 0, t0);
                if (op_idx >= 0) {
                    pt_order_id_t y_oid = pt_broker_submit_ex(&broker, active_mid, 1, PT_SIDE_BID,
                                                              PT_OTYPE_LIMIT,
                                                              (pt_price_t)arb_opp.avg_yes_scaled,
                                                              approved, &yes_book,
                                                              PT_STRAT_PARITY5M, sig_id, t0);
                    pt_order_id_t n_oid = pt_broker_submit_ex(&broker, active_mid, 0, PT_SIDE_BID,
                                                              PT_OTYPE_LIMIT,
                                                              (pt_price_t)arb_opp.avg_no_scaled,
                                                              approved, &no_book,
                                                              PT_STRAT_PARITY5M, sig_id, t0);
                    arb_mgr.ops[op_idx].yes_oid = y_oid;
                    arb_mgr.ops[op_idx].no_oid  = n_oid;
                    telemetry.orders_submitted += 2;
                    pt_lifecycle_on_arb_orders(&lc_tracker, sig_id, y_oid, n_oid, arb_opp.net_edge);
                    pt_strat_stats_record_order(&strat_stats, PT_STRAT_PARITY5M, yes_book.bids.count > 0 ? yes_book.bids.levels[0].size : 0);
                    pt_strat_stats_record_order(&strat_stats, PT_STRAT_PARITY5M, no_book.bids.count > 0 ? no_book.bids.levels[0].size : 0);
                }
            } else {
                pt_csv_log_rejection(&csv_log, t0, sig.signal_id, pt_risk_reject_str(rej), (double)rej);
            }
        }
        /* Strategy B: 15m Flow Skew */
        pt_flow_signal_t skew_sig;
        if (can_trade && have_btc &&
            pt_flow_skew_eval(&config.strategy_flow, &yes_feat, &no_feat, btc_win,
                              btc_price, tte, &skew_sig) &&
            skew_sig.has_signal) {
            uint64_t sig_id = tick_count * 10 + 2;
            pt_signal_t sig = {
                .signal_id = sig_id,
                .strategy  = PT_STRAT_FLOW15M,
                .market_id = active_mid,
                .timestamp = t0,
                .direction = skew_sig.dir,
                .is_yes    = skew_sig.is_yes,
                .target_price = skew_sig.target_price,
                .expected_edge = skew_sig.executable_edge,
                .expected_profit = skew_sig.expected_profit,
                .confidence = skew_sig.confidence,
                .fill_probability = skew_sig.fill_probability,
                .max_size = skew_sig.max_size,
                .time_to_expiry = tte,
                .poly_spread = skew_sig.features.spread,
                .poly_imb_l3 = skew_sig.features.imb_l3,
                .btc_mom_1s  = skew_sig.features.btc_ret_1s
            };
            telemetry.signals_generated++;
            pt_csv_log_signal(&csv_log, &sig);

            pt_lifecycle_on_signal(&lc_tracker, sig_id, t0, active_mid, PT_STRAT_FLOW15M,
                                   skew_sig.is_yes, (skew_sig.dir == PT_DIR_BUY ? PT_SIDE_BID : PT_SIDE_ASK),
                                   skew_sig.raw_edge, skew_sig.executable_edge,
                                   skew_sig.executable_edge, skew_sig.fill_probability,
                                   skew_sig.max_size, skew_sig.expected_profit);

            pt_size_t approved = 0;
            double exp = pt_portfolio_market_exposure(&portfolio, active_mid);
            int rej = pt_risk_evaluate_signal(&risk_engine, &sig, exp, t0, &approved);
            int is_approved = (rej == PT_REJECT_NONE && approved > 0);
            pt_strat_stats_record_signal(&strat_stats, PT_STRAT_FLOW15M, is_approved, 1, skew_sig.raw_edge, skew_sig.executable_edge);

            if (is_approved) {
                const pt_book_t *tgt_bk = sig.is_yes ? &yes_book : &no_book;
                pt_order_id_t b_oid = pt_broker_submit_ex(&broker, active_mid, sig.is_yes, PT_SIDE_BID,
                                                          PT_OTYPE_LIMIT,
                                                          sig.target_price, approved,
                                                          tgt_bk,
                                                          PT_STRAT_FLOW15M, sig_id, t0);
                telemetry.orders_submitted++;
                pt_lifecycle_on_order(&lc_tracker, sig_id, b_oid, skew_sig.executable_edge);
                pt_strat_stats_record_order(&strat_stats, PT_STRAT_FLOW15M, tgt_bk->bids.count > 0 ? tgt_bk->bids.levels[0].size : 0);
            }
        }

        /* Broker tick evaluates matching engine queues with realistic fills */
        if (active_mid > 0) {
            int fills = pt_broker_tick(&broker, active_mid, &yes_book, &no_book, t0, on_engine_fill_, &fill_ctx);
            if (fills > 0) telemetry.orders_filled += fills;
        }

        /* Unhedged exposure manager tick */
        pt_arb_action_t arb_acts[8];
        int num_acts = 0;
        pt_arb_mgr_tick(&arb_mgr, t0, arb_acts, &num_acts);
        for (int a = 0; a < num_acts; a++) {
            if (arb_acts[a].action == PT_ARB_ACT_MARKET_HEDGE && active_mid > 0) {
                pt_broker_submit_ex(&broker, active_mid, (arb_acts[a].leg == 0 ? 1 : 0),
                                    PT_SIDE_ASK, PT_OTYPE_IOC, arb_acts[a].price,
                                    arb_acts[a].size, arb_acts[a].leg == 0 ? &yes_book : &no_book,
                                    PT_STRAT_PARITY5M, 999999, t0);
                pt_strat_stats_record_arb_hedge(&strat_stats, 0.0);
            }
        }

        pt_nsec_t t1 = pt_clock_mono_ns();
        pt_telemetry_record_sample(&telemetry.loop_tick_time, t1 - t0);
        pt_telemetry_record_sample(&telemetry.e2e_latency, t1 - t0);

        if (t1 - last_stat_print >= 3000000000ULL) {
            last_stat_print = t1;
            double a_wr = (strat_stats.strat_a.wins + strat_stats.strat_a.losses > 0) ?
                (double)strat_stats.strat_a.wins * 100.0 / (double)(strat_stats.strat_a.wins + strat_stats.strat_a.losses) : 0.0;
            double b_wr = (strat_stats.strat_b.wins + strat_stats.strat_b.losses > 0) ?
                (double)strat_stats.strat_b.wins * 100.0 / (double)(strat_stats.strat_b.wins + strat_stats.strat_b.losses) : 0.0;

            const char *m_name = act_m ? act_m->slug : "NO_MARKET_DISCOVERED";
            printf("[STATUS] BTC=%.1f | Mkt=%s | StratA: %llu sigs / %llu fills (WR: %.1f%%) | StratB: %llu sigs / %llu fills (WR: %.1f%%) | Cash=$%.2f | PnL=$%.2f\n",
                   btc_price,
                   m_name,
                   (unsigned long long)strat_stats.strat_a.signals,
                   (unsigned long long)strat_stats.strat_a.fills,
                   a_wr,
                   (unsigned long long)strat_stats.strat_b.signals,
                   (unsigned long long)strat_stats.strat_b.fills,
                   b_wr,
                   portfolio.cash,
                   portfolio.realized_pnl);
        }
    }

    printf("\n[ENGINE] Shutting down...\n");
    pt_strat_stats_save(&strat_stats, "data/strategy_stats.json");
    pt_feed_bridge_close(&feed_bridge);
    pt_dataset_writer_close(&dataset_writer);

    pt_http_server_stop(&http_server);
    pt_reactor_destroy(&reactor);
    pt_btc_destroy(&btc_engine);
    pt_csv_close(&csv_log);
    printf("[ENGINE] Shutdown complete.\n");
    return 0;
}
