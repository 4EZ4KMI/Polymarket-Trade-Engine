#include "analytics/pt_metrics.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

void pt_metrics_init(pt_metrics_collector_t *mc)
{
    if (!mc) return;
    memset(mc, 0, sizeof(*mc));
}

void pt_metrics_add_trade(pt_metrics_collector_t *mc, double pnl,
                          int strategy, double initial_edge,
                          double fill_prob, double time_to_expiry_sec,
                          double duration_sec)
{
    if (!mc || mc->trade_count >= PT_METRICS_MAX_TRADES) return;
    pt_trade_stat_t *t = &mc->trades[mc->trade_count++];
    t->pnl = pnl;
    t->strategy = strategy;
    t->initial_edge = initial_edge;
    t->fill_prob = fill_prob;
    t->time_to_expiry_sec = time_to_expiry_sec;
    t->hold_duration_sec = duration_sec;
}

void pt_metrics_add_latency(pt_metrics_collector_t *mc, double latency_us)
{
    if (!mc || mc->latency_count >= PT_METRICS_MAX_TRADES) return;
    mc->latencies_us[mc->latency_count++] = latency_us;
}

static int dbl_cmp_(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

void pt_metrics_compute(pt_metrics_collector_t *mc, double initial_capital)
{
    if (!mc) return;
    pt_backtest_metrics_t *s = &mc->summary;
    memset(s, 0, sizeof(*s));

    uint64_t n = mc->trade_count;
    s->total_trades = n;
    if (n == 0) return;

    double sum_pnl = 0.0;
    double sum_win_pnl = 0.0;
    double sum_loss_pnl = 0.0;
    double peak_capital = initial_capital;
    double cur_capital = initial_capital;
    double max_dd_usd = 0.0;
    double sum_dd_usd = 0.0;

    double pnl_array[PT_METRICS_MAX_TRADES];

    for (uint64_t i = 0; i < n; i++) {
        double p = mc->trades[i].pnl;
        pnl_array[i] = p;
        sum_pnl += p;
        cur_capital += p;

        if (p > 0.0) {
            s->win_trades++;
            sum_win_pnl += p;
        } else if (p < 0.0) {
            s->loss_trades++;
            sum_loss_pnl += fabs(p);
        }

        if (cur_capital > peak_capital) {
            peak_capital = cur_capital;
        } else {
            double dd = peak_capital - cur_capital;
            if (dd > max_dd_usd) max_dd_usd = dd;
            sum_dd_usd += dd;
        }
    }

    s->total_pnl = sum_pnl;
    s->realized_pnl = sum_pnl;
    s->net_pnl = sum_pnl;
    s->max_drawdown_usd = max_dd_usd;
    s->max_drawdown_pct = peak_capital > 0 ? (max_dd_usd / peak_capital * 100.0) : 0.0;
    s->avg_drawdown_usd = sum_dd_usd / (double)n;

    s->win_rate = (double)s->win_trades / (double)n;
    s->loss_rate = (double)s->loss_trades / (double)n;
    s->profit_factor = (sum_loss_pnl > 1e-9) ? (sum_win_pnl / sum_loss_pnl) : (sum_win_pnl > 0 ? 999.0 : 0.0);

    double avg_win = (s->win_trades > 0) ? (sum_win_pnl / (double)s->win_trades) : 0.0;
    double avg_loss = (s->loss_trades > 0) ? (sum_loss_pnl / (double)s->loss_trades) : 0.0;
    s->expectancy = (s->win_rate * avg_win) - (s->loss_rate * avg_loss);
    s->avg_trade_pnl = sum_pnl / (double)n;

    /* Sort pnl for median, best, worst */
    qsort(pnl_array, (size_t)n, sizeof(double), dbl_cmp_);
    s->worst_trade_pnl = pnl_array[0];
    s->best_trade_pnl = pnl_array[n - 1];
    s->median_trade_pnl = (n % 2 == 1) ? pnl_array[n / 2] : (pnl_array[n / 2 - 1] + pnl_array[n / 2]) / 2.0;

    /* Sharpe & Sortino */
    double mean_return = sum_pnl / (double)n;
    double variance = 0.0;
    double downside_variance = 0.0;
    for (uint64_t i = 0; i < n; i++) {
        double diff = mc->trades[i].pnl - mean_return;
        variance += diff * diff;
        if (mc->trades[i].pnl < 0.0) {
            downside_variance += mc->trades[i].pnl * mc->trades[i].pnl;
        }
    }
    double std_dev = sqrt(variance / (double)n);
    double downside_std = sqrt(downside_variance / (double)n);

    s->sharpe_ratio = (std_dev > 1e-9) ? (mean_return / std_dev * sqrt(252.0 * 288.0)) : 0.0; /* 5m periods/day annualized */
    s->sortino_ratio = (downside_std > 1e-9) ? (mean_return / downside_std * sqrt(252.0 * 288.0)) : 0.0;

    /* Latencies */
    if (mc->latency_count > 0) {
        qsort(mc->latencies_us, (size_t)mc->latency_count, sizeof(double), dbl_cmp_);
        double sum_lat = 0.0;
        for (uint64_t i = 0; i < mc->latency_count; i++) sum_lat += mc->latencies_us[i];
        s->avg_latency_us = sum_lat / (double)mc->latency_count;
        s->p50_latency_us = mc->latencies_us[(uint64_t)(mc->latency_count * 0.50)];
        s->p95_latency_us = mc->latencies_us[(uint64_t)(mc->latency_count * 0.95)];
        s->p99_latency_us = mc->latencies_us[(uint64_t)(mc->latency_count * 0.99)];
    }
}
