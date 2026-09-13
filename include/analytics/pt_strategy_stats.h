#ifndef PMT_PT_STRATEGY_STATS_H
#define PMT_PT_STRATEGY_STATS_H

#include "core/ptypes.h"
#include "analytics/pt_calibration.h"
#include "analytics/pt_lifecycle_tracker.h"
#include "analytics/pt_adverse_selection.h"
#include "portfolio/pt_portfolio.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PT_STATS_MAX_RETURNS 1024

typedef struct {
    uint64_t signals;
    uint64_t would_trade;
    uint64_t risk_approved;
    uint64_t orders_submitted;
    uint64_t fills;
    uint64_t partial_fills;
    uint64_t cancelled;
    uint64_t wins;
    uint64_t losses;
    uint64_t arbs_completed;
    uint64_t arbs_hedged;
    uint64_t arbs_failed;
    double   sum_hedge_cost;
    double   win_rate;
    double   gross_profit;
    double   gross_loss;
    double   realized_pnl;
    double   fees;
    double   rebates;
    double   net_pnl;
    double   sum_expected_edge;
    double   sum_executable_edge;
    double   sum_realized_edge;
    double   sum_slippage_bps;
    double   sum_queue_wait_ms;
    double   sum_queue_ahead;
    uint64_t queue_observations;
} pt_single_strat_stats_t;

typedef struct {
    double return_pct;
    double pnl;
    int    strategy;
} pt_settled_trade_t;

typedef struct pt_strategy_stats_tracker_s {
    pt_single_strat_stats_t   strat_a; /* 5m Parity Arbitrage */
    pt_single_strat_stats_t   strat_b; /* 15m Flow Skew */
    pt_settled_trade_t        settled_trades[PT_STATS_MAX_RETURNS];
    size_t                    settled_trades_count;
    pt_calibration_tracker_t *calibration;
    pt_lifecycle_tracker_t   *lifecycle;
    pt_adverse_tracker_t     *adverse;
    pt_portfolio_t           *portfolio;
    char                      persist_path[256];
} pt_strategy_stats_tracker_t;

void pt_strat_stats_init(pt_strategy_stats_tracker_t *st,
                         pt_calibration_tracker_t *calib,
                         pt_lifecycle_tracker_t *lc,
                         pt_adverse_tracker_t *adv,
                         pt_portfolio_t *portf,
                         const char *persist_path);

/* Record a generated signal for Strategy A or B */
void pt_strat_stats_record_signal(pt_strategy_stats_tracker_t *st,
                                  int strategy,
                                  int is_approved,
                                  int is_would_trade,
                                  double expected_edge,
                                  double executable_edge);

/* Record an order submission */
void pt_strat_stats_record_order(pt_strategy_stats_tracker_t *st,
                                 int strategy,
                                 pt_size_t queue_ahead);

/* Record a fill */
void pt_strat_stats_record_fill(pt_strategy_stats_tracker_t *st,
                                int strategy,
                                double fill_price,
                                pt_size_t size,
                                double slippage_bps,
                                double fee,
                                double rebate,
                                double queue_wait_ms,
                                int is_partial);

/* Record trade cancellation */
void pt_strat_stats_record_cancel(pt_strategy_stats_tracker_t *st, int strategy);

/* Record trade settlement outcome ($Y \in \{0, 1\}$) */
void pt_strat_stats_record_settlement(pt_strategy_stats_tracker_t *st,
                                      int strategy,
                                      int is_win,
                                      double pnl,
                                      double cost_basis,
                                      double realized_edge);

/* Record Strategy A arbitrage hedge / complete events */
void pt_strat_stats_record_arb_hedge(pt_strategy_stats_tracker_t *st, double hedge_cost);
void pt_strat_stats_record_arb_complete(pt_strategy_stats_tracker_t *st);

/* Compute Sharpe ratio from settled returns (returns 0.0 if insufficient data) */
double pt_strat_stats_calc_sharpe(const pt_strategy_stats_tracker_t *st, int *has_enough_data);

/* Save and load cumulative statistics to disk (JSON) */
int  pt_strat_stats_save(const pt_strategy_stats_tracker_t *st, const char *path);
int  pt_strat_stats_load(pt_strategy_stats_tracker_t *st, const char *path);

/* Export 100% dynamic analytics JSON for REST HTTP /api/analytics */
void pt_strat_stats_json_analytics(const pt_strategy_stats_tracker_t *st, char *buf, size_t max_len);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_STRATEGY_STATS_H */
