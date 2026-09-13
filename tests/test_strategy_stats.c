#include "test_harness.h"
#include "analytics/pt_strategy_stats.h"
#include "analytics/pt_calibration.h"
#include "analytics/pt_lifecycle_tracker.h"
#include "portfolio/pt_portfolio.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

PT_T(strat_stats_recording)
{
    pt_calibration_tracker_t calib;
    pt_calibration_init(&calib);

    pt_lifecycle_tracker_t lc;
    pt_lifecycle_tracker_init(&lc);

    pt_portfolio_t portf;
    pt_portfolio_init(&portf, 10000.0);

    const char *tmp_path = "/tmp/test_pmt_strat_stats.json";
    unlink(tmp_path);

    pt_strategy_stats_tracker_t st;
    pt_strat_stats_init(&st, &calib, &lc, &portf, tmp_path);

    PT_ASSERT(st.strat_a.signals_total == 0);
    PT_ASSERT(st.strat_b.signals_total == 0);

    /* Record Strategy A signals */
    pt_strat_stats_record_signal(&st, PT_STRAT_PARITY5M, 1, 1, 0.012);
    pt_strat_stats_record_signal(&st, PT_STRAT_PARITY5M, 0, 1, 0.005);
    pt_strat_stats_record_order(&st, PT_STRAT_PARITY5M);
    pt_strat_stats_record_fill(&st, PT_STRAT_PARITY5M, 0.49, 100, 0.5, 0.049, 0.0, 12.0);
    pt_strat_stats_record_settlement(&st, PT_STRAT_PARITY5M, 1, 51.0);

    /* Record Strategy B signals */
    pt_strat_stats_record_signal(&st, PT_STRAT_FLOW15M, 1, 1, 0.025);
    pt_strat_stats_record_signal(&st, PT_STRAT_FLOW15M, 1, 1, 0.030);
    pt_strat_stats_record_signal(&st, PT_STRAT_FLOW15M, 0, 1, 0.002);
    pt_strat_stats_record_order(&st, PT_STRAT_FLOW15M);
    pt_strat_stats_record_order(&st, PT_STRAT_FLOW15M);
    pt_strat_stats_record_fill(&st, PT_STRAT_FLOW15M, 0.52, 200, 1.0, 0.104, 0.0, 8.0);
    pt_strat_stats_record_settlement(&st, PT_STRAT_FLOW15M, 0, -104.0);

    /* Verify in-memory state */
    PT_ASSERT(st.strat_a.signals_total == 2);
    PT_ASSERT(st.strat_a.signals_approved == 1);
    PT_ASSERT(st.strat_a.signals_rejected_risk == 1);
    PT_ASSERT(st.strat_a.orders_submitted == 1);
    PT_ASSERT(st.strat_a.orders_filled == 1);
    PT_ASSERT(st.strat_a.trades_won == 1);
    PT_ASSERT(st.strat_a.trades_lost == 0);
    PT_ASSERT(st.strat_a.realized_pnl == 51.0);

    PT_ASSERT(st.strat_b.signals_total == 3);
    PT_ASSERT(st.strat_b.signals_approved == 2);
    PT_ASSERT(st.strat_b.signals_rejected_risk == 1);
    PT_ASSERT(st.strat_b.orders_submitted == 2);
    PT_ASSERT(st.strat_b.orders_filled == 1);
    PT_ASSERT(st.strat_b.trades_won == 0);
    PT_ASSERT(st.strat_b.trades_lost == 1);
    PT_ASSERT(st.strat_b.realized_pnl == -104.0);

    /* Test JSON analytics generation */
    char json_buf[2048];
    pt_strat_stats_json_analytics(&st, json_buf, sizeof(json_buf));
    PT_ASSERT(strstr(json_buf, "\"strat_a\":") != NULL);
    PT_ASSERT(strstr(json_buf, "\"strat_b\":") != NULL);
    PT_ASSERT(strstr(json_buf, "\"win_rate\":1.0000") != NULL);

    /* Test reload from disk */
    pt_strategy_stats_tracker_t st2;
    pt_strat_stats_init(&st2, &calib, &lc, &portf, tmp_path);
    PT_ASSERT(st2.strat_a.signals_total == 2);
    PT_ASSERT(st2.strat_a.trades_won == 1);
    PT_ASSERT(st2.strat_b.signals_total == 3);
    PT_ASSERT(st2.strat_b.trades_lost == 1);

    unlink(tmp_path);
}

