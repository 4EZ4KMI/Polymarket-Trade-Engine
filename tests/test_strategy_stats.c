#include "test_harness.h"
#include "analytics/pt_strategy_stats.h"
#include "analytics/pt_calibration.h"
#include "analytics/pt_lifecycle_tracker.h"
#include "analytics/pt_adverse_selection.h"
#include "portfolio/pt_portfolio.h"
#include "net/pt_feed_bridge.h"
#include "execution/pt_broker.h"
#include "core/pt_market_registry.h"
#include "orderbook/pt_book.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

PT_T(strat_stats_recording)
{
    pt_calibration_tracker_t calib;
    pt_calibration_init(&calib);

    pt_lifecycle_tracker_t lc;
    pt_lifecycle_tracker_init(&lc);

    pt_adverse_tracker_t adv;
    pt_adverse_tracker_init(&adv);

    pt_portfolio_t portf;
    pt_portfolio_init(&portf, 10000.0);

    const char *tmp_path = "/tmp/test_pmt_strat_stats.json";
    unlink(tmp_path);

    pt_strategy_stats_tracker_t st;
    pt_strat_stats_init(&st, &calib, &lc, &adv, &portf, tmp_path);

    PT_ASSERT(st.strat_a.signals == 0);
    PT_ASSERT(st.strat_b.signals == 0);

    /* Record Strategy A signals & orders */
    pt_strat_stats_record_signal(&st, PT_STRAT_PARITY5M, 1, 1, 0.012, 0.010);
    pt_strat_stats_record_signal(&st, PT_STRAT_PARITY5M, 0, 1, 0.005, 0.003);
    pt_strat_stats_record_order(&st, PT_STRAT_PARITY5M, 450);
    pt_strat_stats_record_fill(&st, PT_STRAT_PARITY5M, 0.49, 100, 0.5, 0.049, 0.0, 12.0, 0);
    pt_strat_stats_record_settlement(&st, PT_STRAT_PARITY5M, 1, 51.0, 49.0, 0.020);

    /* Record Strategy B signals & orders */
    pt_strat_stats_record_signal(&st, PT_STRAT_FLOW15M, 1, 1, 0.025, 0.022);
    pt_strat_stats_record_signal(&st, PT_STRAT_FLOW15M, 1, 1, 0.030, 0.028);
    pt_strat_stats_record_signal(&st, PT_STRAT_FLOW15M, 0, 1, 0.002, 0.001);
    pt_strat_stats_record_order(&st, PT_STRAT_FLOW15M, 300);
    pt_strat_stats_record_order(&st, PT_STRAT_FLOW15M, 350);
    pt_strat_stats_record_fill(&st, PT_STRAT_FLOW15M, 0.52, 200, 1.0, 0.104, 0.0, 8.0, 0);
    pt_strat_stats_record_settlement(&st, PT_STRAT_FLOW15M, 0, -104.0, 104.0, -0.020);

    /* Verify complete isolation between Strategy A and Strategy B */
    PT_ASSERT(st.strat_a.signals == 2);
    PT_ASSERT(st.strat_a.risk_approved == 1);
    PT_ASSERT(st.strat_a.orders_submitted == 1);
    PT_ASSERT(st.strat_a.fills == 1);
    PT_ASSERT(st.strat_a.wins == 1);
    PT_ASSERT(st.strat_a.losses == 0);
    PT_ASSERT_NEAR(st.strat_a.win_rate, 1.0, 0.001);
    PT_ASSERT_NEAR(st.strat_a.realized_pnl, 51.0, 0.001);

    PT_ASSERT(st.strat_b.signals == 3);
    PT_ASSERT(st.strat_b.risk_approved == 2);
    PT_ASSERT(st.strat_b.orders_submitted == 2);
    PT_ASSERT(st.strat_b.fills == 1);
    PT_ASSERT(st.strat_b.wins == 0);
    PT_ASSERT(st.strat_b.losses == 1);
    PT_ASSERT_NEAR(st.strat_b.win_rate, 0.0, 0.001);
    PT_ASSERT_NEAR(st.strat_b.realized_pnl, -104.0, 0.001);

    /* Test JSON analytics generation */
    char json_buf[4096];
    pt_strat_stats_json_analytics(&st, json_buf, sizeof(json_buf));
    PT_ASSERT(strstr(json_buf, "\"strat_a\":") != NULL);
    PT_ASSERT(strstr(json_buf, "\"strat_b\":") != NULL);
    PT_ASSERT(strstr(json_buf, "\"win_rate\":1.0000") != NULL);

    /* Test reload from disk */
    pt_strategy_stats_tracker_t st2;
    pt_strat_stats_init(&st2, &calib, &lc, &adv, &portf, tmp_path);
    PT_ASSERT(st2.strat_a.signals == 2);
    PT_ASSERT(st2.strat_a.wins == 1);
    PT_ASSERT(st2.strat_b.signals == 3);
    PT_ASSERT(st2.strat_b.losses == 1);

    unlink(tmp_path);
}

PT_T(no_hardcoded_analytics_constants)
{
    pt_calibration_tracker_t calib;
    pt_calibration_init(&calib);
    pt_lifecycle_tracker_t lc;
    pt_lifecycle_tracker_init(&lc);
    pt_adverse_tracker_t adv;
    pt_adverse_tracker_init(&adv);
    pt_portfolio_t portf;
    pt_portfolio_init(&portf, 10000.0);

    pt_strategy_stats_tracker_t st;
    pt_strat_stats_init(&st, &calib, &lc, &adv, &portf, NULL);

    char json_buf[4096];
    pt_strat_stats_json_analytics(&st, json_buf, sizeof(json_buf));

    /* Check that clean zero state has NO hardcoded 1.8 bps, 120 queue, 1.85 Sharpe */
    PT_ASSERT(strstr(json_buf, "\"adverse_selection_bps\":0.00") != NULL);
    PT_ASSERT(strstr(json_buf, "\"avg_queue_ahead\":0") != NULL);
    PT_ASSERT(strstr(json_buf, "\"sharpe\":0.00") != NULL);
    PT_ASSERT(strstr(json_buf, "\"has_sharpe\":0") != NULL);
    PT_ASSERT(strstr(json_buf, "\"total_trades\":0") != NULL);
}

PT_T(unresolved_trades_win_rate)
{
    pt_strategy_stats_tracker_t st;
    pt_strat_stats_init(&st, NULL, NULL, NULL, NULL, NULL);

    /* Generated signals, order submitted and filled, but NOT settled */
    pt_strat_stats_record_signal(&st, PT_STRAT_PARITY5M, 1, 1, 0.02, 0.02);
    pt_strat_stats_record_order(&st, PT_STRAT_PARITY5M, 100);
    pt_strat_stats_record_fill(&st, PT_STRAT_PARITY5M, 0.50, 100, 0.0, 0.05, 0.0, 10.0, 0);

    /* Unresolved trades MUST NOT count towards win rate */
    PT_ASSERT(st.strat_a.wins == 0);
    PT_ASSERT(st.strat_a.losses == 0);
    PT_ASSERT_NEAR(st.strat_a.win_rate, 0.0, 0.0001);
}

PT_T(per_fill_adverse_selection_trajectory)
{
    pt_adverse_tracker_t adv;
    pt_adverse_tracker_init(&adv);

    pt_lifecycle_tracker_t lc;
    pt_lifecycle_tracker_init(&lc);

    pt_nsec_t t0 = 1000000000000ULL; /* 1000s in ns */

    /* Opportunity 1: Bought YES @ 0.50 (5000 price scale) */
    uint64_t sig1 = pt_lifecycle_on_signal(&lc, 1001, t0, 101, PT_STRAT_PARITY5M, 1, PT_SIDE_BID,
                                           0.02, 0.02, 0.02, 0.9, 100, 2.0);
    uint64_t fill1 = pt_adverse_record_fill(&adv, t0, 101, PT_STRAT_PARITY5M, 1, PT_SIDE_BID, 5000, 100);
    PT_ASSERT(fill1 == 1);
    pt_lifecycle_on_fill(&lc, sig1, 100, 5000, 0.50, 2.5, 0.0, 0.0, 0.0, fill1);

    /* Opportunity 2: Bought YES @ 0.52 (5200 price scale) 50ms later */
    pt_nsec_t t1 = t0 + 50000000ULL;
    uint64_t sig2 = pt_lifecycle_on_signal(&lc, 1002, t1, 101, PT_STRAT_PARITY5M, 1, PT_SIDE_BID,
                                           0.03, 0.03, 0.03, 0.85, 100, 3.0);
    uint64_t fill2 = pt_adverse_record_fill(&adv, t1, 101, PT_STRAT_PARITY5M, 1, PT_SIDE_BID, 5200, 100);
    PT_ASSERT(fill2 == 2);
    pt_lifecycle_on_fill(&lc, sig2, 100, 5200, 0.52, 1.8, 0.0, 0.0, 0.0, fill2);

    /* Price tick at t0 + 15ms: Price drops to 0.4950 (4950) -> Fill 1 (at 10ms horizon): adverse movement = (5000 - 4950)/5000 * 10000 = +100 bps */
    pt_adverse_tracker_on_price(&adv, 101, 1, 4950, t0 + 15000000ULL);
    PT_ASSERT_NEAR(pt_adverse_get_fill_bps(&adv, fill1), 100.0, 0.1);
    PT_ASSERT_NEAR(pt_adverse_get_fill_bps(&adv, fill2), 0.0, 0.1); /* Fill 2 has not reached 10ms yet */

    /* Price tick at t0 + 70ms: Price rises to 0.5300 (5300):
       - Fill 1 (at 70ms elapsed, 50ms horizon measured): (5000 - 5300)/5000 * 10000 = -600 bps (favorable)
       - Fill 2 (at 20ms elapsed from t1, 10ms horizon measured): (5200 - 5300)/5200 * 10000 = -192.3 bps (favorable) */
    pt_adverse_tracker_on_price(&adv, 101, 1, 5300, t0 + 70000000ULL);
    PT_ASSERT_NEAR(pt_adverse_get_fill_bps(&adv, fill1), -600.0, 0.1);
    PT_ASSERT_NEAR(pt_adverse_get_fill_bps(&adv, fill2), -192.3, 0.5);

    /* Complete opportunities and compute lifecycle stats with real measured trajectories */
    pt_lifecycle_on_complete(&lc, sig1, 50.0, 0.0, 0.02);
    pt_lifecycle_on_complete(&lc, sig2, 48.0, 0.0, 0.03);

    pt_lifecycle_compute_stats(&lc, &adv);
    PT_ASSERT(lc.completed_count == 2);
    PT_ASSERT_NEAR(lc.records[0].adverse_selection, -600.0, 0.1);
    PT_ASSERT_NEAR(lc.records[1].adverse_selection, -192.3, 0.5);
    /* Average adverse selection = (-600.0 + (-192.30769)) / 2 = -396.15 bps */
    PT_ASSERT_NEAR(lc.avg_adverse_selection, -396.15, 0.5);
}
PT_T(real_sharpe_calculation)
{
    pt_strategy_stats_tracker_t st;
    pt_strat_stats_init(&st, NULL, NULL, NULL, NULL, NULL);

    /* Settle 6 trades with positive returns */
    for (int i = 0; i < 6; i++) {
        pt_strat_stats_record_settlement(&st, PT_STRAT_PARITY5M, 1, 10.0, 100.0, 0.10);
    }

    int has_sharpe = 0;
    double sharpe = pt_strat_stats_calc_sharpe(&st, &has_sharpe);
    (void)sharpe;
    PT_ASSERT(has_sharpe == 0 || sharpe >= 0.0); /* If variance == 0, returns 0.0 without crash */

    /* Add trade with different return so variance > 0 */
    pt_strat_stats_record_settlement(&st, PT_STRAT_PARITY5M, 0, -5.0, 100.0, -0.05);
    sharpe = pt_strat_stats_calc_sharpe(&st, &has_sharpe);
    PT_ASSERT(has_sharpe == 1);
    PT_ASSERT(sharpe > 0.0);
}

static void test_engine_fill_cb_(const pt_order_t *o, pt_size_t filled_shares,
                                 pt_price_t fill_price, void *ud)
{
    pt_strategy_stats_tracker_t *stats = (pt_strategy_stats_tracker_t *)ud;
    if (stats && o) {
        double fill_p = (double)fill_price / PT_PRICE_SCALE;
        int is_part = (o->state == PT_OSTATE_PARTIAL);
        pt_strat_stats_record_fill(stats, o->strategy, fill_p, filled_shares, 0.0, 0.0, 0.0, 0.5, is_part);
    }
}

PT_T(feed_bridge_to_broker_to_stats_e2e)
{
    pt_market_registry_t reg;
    pt_market_registry_init(&reg);

    pt_market_registry_add(&reg, 101, "0x12345", "btc-test", "TOKEN_YES_101", "TOKEN_NO_101",
                           88000.0, 1000000000ULL, 2000000000ULL, 1000);

    pt_book_t yes_book, no_book;
    pt_book_init(&yes_book);
    pt_book_init(&no_book);

    pt_portfolio_t portf;
    pt_portfolio_init(&portf, 10000.0);

    pt_broker_cfg_t bcfg = {
        .latency_submit_ack_ms = 0.0,
        .latency_ack_fill_ms = 0.0,
        .latency_cancel_ms = 0.0,
        .queue_model = PT_QUEUE_MODEL_REALISTIC
    };
    pt_broker_t broker;
    pt_broker_init(&broker, &bcfg, &portf);

    pt_strategy_stats_tracker_t stats;
    pt_strat_stats_init(&stats, NULL, NULL, NULL, NULL, NULL);
    pt_broker_set_fill_callback(&broker, test_engine_fill_cb_, &stats);

    pt_feed_bridge_t fb;
    pt_feed_bridge_init(&fb, NULL, 9991, &yes_book, &no_book, NULL, NULL, NULL, NULL, NULL,
                        &reg, &portf, &broker, &stats, NULL, NULL);

    /* Step 1: Initialize book level at 490 with 500 shares */
    const char *book_json = "{\"event_type\":\"book\",\"asset_id\":\"TOKEN_YES_101\",\"side\":\"BID\",\"price\":\"0.490\",\"size\":\"500\",\"timestamp\":1000}\n";
    pt_feed_bridge_on_line(&fb, book_json, strlen(book_json), 1000);
    pt_price_t best_bid_p = 0; pt_size_t best_bid_s = 0;
    PT_ASSERT(pt_book_best_bid(&yes_book, &best_bid_p, &best_bid_s) == 0);
    PT_ASSERT(best_bid_p == 490);
    PT_ASSERT(pt_book_get_level_size(&yes_book, PT_SIDE_BID, 490) == 500);

    /* Step 2: Submit limit buy order of 100 shares behind 500 shares in queue */
    pt_order_id_t oid = pt_broker_submit(&broker, 101, 1, PT_SIDE_BID, 490, 100, &yes_book, PT_STRAT_PARITY5M, 1000);
    PT_ASSERT(oid > 0);
    PT_ASSERT(broker.sim_queue.orders[0].queue_ahead == 500);
    PT_ASSERT(stats.strat_a.fills == 0);
    PT_ASSERT(portf.position_count == 0);

    /* Step 3: Stream Polymarket real trade for 300 shares -> depletes queue ahead to 200, no fill yet */
    const char *tr1_json = "{\"event_type\":\"trade\",\"asset_id\":\"TOKEN_YES_101\",\"side\":\"SELL\",\"price\":\"0.490\",\"size\":\"300\",\"timestamp\":2000}\n";
    pt_feed_bridge_on_line(&fb, tr1_json, strlen(tr1_json), 2000);
    PT_ASSERT(broker.sim_queue.orders[0].queue_ahead == 200);
    PT_ASSERT(stats.strat_a.fills == 0);
    PT_ASSERT(portf.position_count == 0);

    /* Step 4: Stream Polymarket real trade for 250 shares -> exhausts remaining 200 ahead + fills 50 of our order! */
    const char *tr2_json = "{\"event_type\":\"trade\",\"asset_id\":\"TOKEN_YES_101\",\"side\":\"SELL\",\"price\":\"0.490\",\"size\":\"250\",\"timestamp\":3000}\n";
    pt_feed_bridge_on_line(&fb, tr2_json, strlen(tr2_json), 3000);

    /* Verify fill propagation through the entire pipeline: */
    /* a) Sim queue state */
    PT_ASSERT(broker.sim_queue.orders[0].queue_ahead == 0);
    PT_ASSERT(broker.sim_queue.orders[0].state == PT_OSTATE_PARTIAL);
    PT_ASSERT(broker.sim_queue.orders[0].remaining_size == 50);

    /* b) Portfolio fill & cost deduction */
    PT_ASSERT(portf.position_count == 1);
    PT_ASSERT(portf.positions[0].market_id == 101);
    PT_ASSERT(portf.positions[0].shares == 50);
    PT_ASSERT(portf.positions[0].cost_basis_scaled == 50 * 490);

    /* c) Strategy statistics partial fill recording */
    int is_part = (broker.sim_queue.orders[0].state == PT_OSTATE_PARTIAL);
    PT_ASSERT(is_part == 1);
    PT_ASSERT(stats.strat_a.partial_fills == 1);
    PT_ASSERT(stats.strat_a.fills == 0);

    /* Step 4b: Stream another trade for 50 shares -> completes remaining 50 shares! */
    const char *tr3_json = "{\"event_type\":\"trade\",\"asset_id\":\"TOKEN_YES_101\",\"side\":\"SELL\",\"price\":\"0.490\",\"size\":\"50\",\"timestamp\":3500}\n";
    pt_feed_bridge_on_line(&fb, tr3_json, strlen(tr3_json), 3500);
    PT_ASSERT(broker.sim_queue.orders[0].state == PT_OSTATE_FILLED);
    PT_ASSERT(broker.sim_queue.orders[0].remaining_size == 0);
    PT_ASSERT(portf.positions[0].shares == 100);
    PT_ASSERT(portf.positions[0].cost_basis_scaled == 100 * 490);
    PT_ASSERT(stats.strat_a.fills == 1);

    /* Step 5: Test book delta level cancellation */
    /* Book level shrinks from 500 to 200 -> delta reduction updates queue_ahead */
    const char *book_delta_json = "{\"event_type\":\"book\",\"asset_id\":\"TOKEN_YES_101\",\"side\":\"BID\",\"price\":\"0.490\",\"size\":\"200\",\"timestamp\":4000}\n";
    pt_feed_bridge_on_line(&fb, book_delta_json, strlen(book_delta_json), 4000);
    PT_ASSERT(pt_book_get_level_size(&yes_book, PT_SIDE_BID, 490) == 200);

    /* Clean up */
    pt_feed_bridge_close(&fb);
}



