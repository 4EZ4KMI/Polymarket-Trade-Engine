#ifndef PMT_PT_METRICS_H
#define PMT_PT_METRICS_H

#include "core/ptypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PT_METRICS_MAX_TRADES 4096

typedef struct {
    double pnl;
    double return_pct;
    double hold_duration_sec;
    int    strategy;
    double initial_edge;
    double fill_prob;
    double time_to_expiry_sec;
} pt_trade_stat_t;

typedef struct {
    /* PnL metrics */
    double total_pnl;
    double realized_pnl;
    double unrealized_pnl;
    double gross_pnl;
    double net_pnl;
    double total_fees_paid;
    double total_rebates_earned;
    
    /* Performance ratios */
    uint64_t total_trades;
    uint64_t win_trades;
    uint64_t loss_trades;
    double   win_rate;
    double   loss_rate;
    double   profit_factor;
    double   expectancy;
    
    /* Risk & Drawdown */
    double   max_drawdown_usd;
    double   max_drawdown_pct;
    double   avg_drawdown_usd;
    double   sharpe_ratio;
    double   sortino_ratio;
    
    /* Trade distribution */
    double   avg_trade_pnl;
    double   median_trade_pnl;
    double   best_trade_pnl;
    double   worst_trade_pnl;
    
    /* Order execution metrics */
    uint64_t orders_submitted;
    uint64_t orders_filled;
    uint64_t orders_partially_filled;
    uint64_t orders_cancelled;
    double   fill_ratio;
    double   partial_fill_ratio;
    double   cancel_ratio;
    
    /* Latency percentiles (microseconds) */
    double   avg_latency_us;
    double   p50_latency_us;
    double   p95_latency_us;
    double   p99_latency_us;
    
    /* Edge decay */
    double   avg_theoretical_edge;
    double   avg_executable_edge;
    double   avg_realized_edge;
    double   avg_adverse_selection;
} pt_backtest_metrics_t;

typedef struct {
    pt_trade_stat_t       trades[PT_METRICS_MAX_TRADES];
    uint64_t              trade_count;
    double                latencies_us[PT_METRICS_MAX_TRADES];
    uint64_t              latency_count;
    pt_backtest_metrics_t summary;
} pt_metrics_collector_t;

void pt_metrics_init(pt_metrics_collector_t *mc);

void pt_metrics_add_trade(pt_metrics_collector_t *mc, double pnl,
                          int strategy, double initial_edge,
                          double fill_prob, double time_to_expiry_sec,
                          double duration_sec);

void pt_metrics_add_latency(pt_metrics_collector_t *mc, double latency_us);

void pt_metrics_compute(pt_metrics_collector_t *mc, double initial_capital);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_METRICS_H */
