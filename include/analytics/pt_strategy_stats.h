#ifndef PMT_PT_STRATEGY_STATS_H
#define PMT_PT_STRATEGY_STATS_H

#include "core/ptypes.h"
#include "analytics/pt_calibration.h"
#include "analytics/pt_lifecycle_tracker.h"
#include "portfolio/pt_portfolio.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t signals_total;
    uint64_t signals_would_trade;
    uint64_t signals_approved;
    uint64_t signals_rejected_risk;
    uint64_t orders_submitted;
    uint64_t orders_filled;
    uint64_t orders_cancelled;
    uint64_t trades_won;
    uint64_t trades_lost;
    double   realized_pnl;
    double   unrealized_pnl;
    double   gross_profit;
    double   gross_loss;
    double   fees_paid;
    double   rebates_earned;
    double   sum_expected_edge;
    double   sum_realized_edge;
    double   sum_slippage_bps;
    double   sum_queue_wait_ms;
} pt_single_strat_stats_t;

typedef struct {
    pt_single_strat_stats_t   strat_a; /* 5m Parity Arbitrage */
    pt_single_strat_stats_t   strat_b; /* 15m Flow Skew */
    pt_calibration_tracker_t *calibration;
    pt_lifecycle_tracker_t   *lifecycle;
    pt_portfolio_t           *portfolio;
    char                      persist_path[256];
} pt_strategy_stats_tracker_t;

void pt_strat_stats_init(pt_strategy_stats_tracker_t *st,
                         pt_calibration_tracker_t *calib,
                         pt_lifecycle_tracker_t *lc,
                         pt_portfolio_t *portf,
                         const char *persist_path);

/* Record a generated signal for Strategy A or B */
void pt_strat_stats_record_signal(pt_strategy_stats_tracker_t *st,
                                  int strategy,
                                  int is_approved,
                                  int is_would_trade,
                                  double expected_edge);

/* Record an order submission */
void pt_strat_stats_record_order(pt_strategy_stats_tracker_t *st,
                                 int strategy);

/* Record a fill */
void pt_strat_stats_record_fill(pt_strategy_stats_tracker_t *st,
                                int strategy,
                                double fill_price,
                                pt_size_t size,
                                double slippage_bps,
                                double fee,
                                double rebate,
                                double queue_wait_ms);

/* Record trade settlement outcome ($Y \in \{0, 1\}$) */
void pt_strat_stats_record_settlement(pt_strategy_stats_tracker_t *st,
                                      int strategy,
                                      int is_win,
                                      double pnl);

/* Save and load cumulative statistics to disk (JSON) */
int  pt_strat_stats_save(const pt_strategy_stats_tracker_t *st, const char *path);
int  pt_strat_stats_load(pt_strategy_stats_tracker_t *st, const char *path);

/* Export 100% dynamic analytics JSON for REST HTTP /api/analytics */
void pt_strat_stats_json_analytics(const pt_strategy_stats_tracker_t *st, char *buf, size_t max_len);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_STRATEGY_STATS_H */
