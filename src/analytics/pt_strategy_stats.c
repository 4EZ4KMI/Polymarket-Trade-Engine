#include "analytics/pt_strategy_stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void pt_strat_stats_init(pt_strategy_stats_tracker_t *st,
                         pt_calibration_tracker_t *calib,
                         pt_lifecycle_tracker_t *lc,
                         pt_adverse_tracker_t *adv,
                         pt_portfolio_t *portf,
                         const char *persist_path)
{
    if (!st) return;
    memset(st, 0, sizeof(*st));
    st->calibration = calib;
    st->lifecycle = lc;
    st->adverse = adv;
    st->portfolio = portf;
    if (persist_path) {
        strncpy(st->persist_path, persist_path, sizeof(st->persist_path) - 1);
        pt_strat_stats_load(st, persist_path);
    }
}

static pt_single_strat_stats_t *get_strat_(pt_strategy_stats_tracker_t *st, int strategy)
{
    if (!st) return NULL;
    if (strategy == PT_STRAT_PARITY5M) return &st->strat_a;
    return &st->strat_b;
}

void pt_strat_stats_record_signal(pt_strategy_stats_tracker_t *st,
                                  int strategy,
                                  int is_approved,
                                  int is_would_trade,
                                  double expected_edge,
                                  double executable_edge)
{
    pt_single_strat_stats_t *s = get_strat_(st, strategy);
    if (!s) return;

    s->signals++;
    if (is_would_trade) s->would_trade++;
    if (is_approved) {
        s->risk_approved++;
        s->sum_expected_edge += expected_edge;
        s->sum_executable_edge += executable_edge;
    }
}

void pt_strat_stats_record_order(pt_strategy_stats_tracker_t *st,
                                 int strategy,
                                 pt_size_t queue_ahead)
{
    pt_single_strat_stats_t *s = get_strat_(st, strategy);
    if (!s) return;
    s->orders_submitted++;
    s->sum_queue_ahead += (double)queue_ahead;
    s->queue_observations++;
}

void pt_strat_stats_record_fill(pt_strategy_stats_tracker_t *st,
                                int strategy,
                                double fill_price,
                                pt_size_t size,
                                double slippage_bps,
                                double fee,
                                double rebate,
                                double queue_wait_ms,
                                int is_partial)
{
    pt_single_strat_stats_t *s = get_strat_(st, strategy);
    if (!s) return;
    (void)fill_price; (void)size;
    if (is_partial) {
        s->partial_fills++;
    } else {
        s->fills++;
    }
    s->fees += fee;
    s->rebates += rebate;
    s->sum_slippage_bps += slippage_bps;
    s->sum_queue_wait_ms += queue_wait_ms;
    s->net_pnl = s->realized_pnl - s->fees + s->rebates;
}

void pt_strat_stats_record_cancel(pt_strategy_stats_tracker_t *st, int strategy)
{
    pt_single_strat_stats_t *s = get_strat_(st, strategy);
    if (!s) return;
    s->cancelled++;
}

void pt_strat_stats_record_arb_hedge(pt_strategy_stats_tracker_t *st, double hedge_cost)
{
    if (!st) return;
    st->strat_a.arbs_hedged++;
    st->strat_a.sum_hedge_cost += hedge_cost;
}

void pt_strat_stats_record_arb_complete(pt_strategy_stats_tracker_t *st)
{
    if (!st) return;
    st->strat_a.arbs_completed++;
}

void pt_strat_stats_record_settlement(pt_strategy_stats_tracker_t *st,
                                      int strategy,
                                      int is_win,
                                      double pnl,
                                      double cost_basis,
                                      double realized_edge)
{
    if (!st) return;
    pt_single_strat_stats_t *s = get_strat_(st, strategy);
    if (!s) return;

    if (is_win) {
        s->wins++;
        s->gross_profit += pnl;
    } else {
        s->losses++;
        s->gross_loss += fabs(pnl);
    }
    s->realized_pnl += pnl;
    s->net_pnl = s->realized_pnl - s->fees + s->rebates;
    s->sum_realized_edge += realized_edge;

    uint64_t settled_count = s->wins + s->losses;
    if (settled_count > 0) {
        s->win_rate = (double)s->wins / (double)settled_count;
    } else {
        s->win_rate = 0.0;
    }

    if (st->settled_trades_count < PT_STATS_MAX_RETURNS) {
        pt_settled_trade_t *tr = &st->settled_trades[st->settled_trades_count++];
        tr->strategy = strategy;
        tr->pnl = pnl;
        tr->return_pct = (cost_basis > 0.0001) ? (pnl / cost_basis) : 0.0;
    }

    if (st->persist_path[0] != '\0') {
        pt_strat_stats_save(st, st->persist_path);
    }
}

double pt_strat_stats_calc_sharpe_strat(const pt_strategy_stats_tracker_t *st, int strategy, int *has_enough_data)
{
    if (has_enough_data) *has_enough_data = 0;
    if (!st || st->settled_trades_count < 5) {
        return 0.0;
    }

    double sum = 0.0;
    size_t count = 0;
    for (size_t i = 0; i < st->settled_trades_count; i++) {
        if (strategy < 0 || st->settled_trades[i].strategy == strategy) {
            sum += st->settled_trades[i].return_pct;
            count++;
        }
    }
    if (count < 5) return 0.0;

    double mean = sum / (double)count;
    double sq_diff_sum = 0.0;
    for (size_t i = 0; i < st->settled_trades_count; i++) {
        if (strategy < 0 || st->settled_trades[i].strategy == strategy) {
            double d = st->settled_trades[i].return_pct - mean;
            sq_diff_sum += d * d;
        }
    }
    double variance = sq_diff_sum / (double)(count - 1);
    double stddev = sqrt(variance);

    if (stddev < 1e-9) {
        return 0.0;
    }

    if (has_enough_data) *has_enough_data = 1;
    /* Trade-level Sharpe ratio: mean trade return over sample standard deviation */
    return (mean / stddev);
}

double pt_strat_stats_calc_sharpe(const pt_strategy_stats_tracker_t *st, int *has_enough_data)
{
    return pt_strat_stats_calc_sharpe_strat(st, -1, has_enough_data);
}

int pt_strat_stats_save(const pt_strategy_stats_tracker_t *st, const char *path)
{
    if (!st || !path) return -1;
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp, "{\n"
      "  \"a_sig\": %llu, \"a_wld\": %llu, \"a_app\": %llu, \"a_ord\": %llu, \"a_fil\": %llu, \"a_win\": %llu, \"a_los\": %llu, \"a_pnl\": %.4f, \"a_fee\": %.4f,\n"
      "  \"b_sig\": %llu, \"b_wld\": %llu, \"b_app\": %llu, \"b_ord\": %llu, \"b_fil\": %llu, \"b_win\": %llu, \"b_los\": %llu, \"b_pnl\": %.4f, \"b_fee\": %.4f\n"
      "}\n",
      (unsigned long long)st->strat_a.signals,
      (unsigned long long)st->strat_a.would_trade,
      (unsigned long long)st->strat_a.risk_approved,
      (unsigned long long)st->strat_a.orders_submitted,
      (unsigned long long)st->strat_a.fills,
      (unsigned long long)st->strat_a.wins,
      (unsigned long long)st->strat_a.losses,
      (double)st->strat_a.realized_pnl,
      (double)st->strat_a.fees,
      (unsigned long long)st->strat_b.signals,
      (unsigned long long)st->strat_b.would_trade,
      (unsigned long long)st->strat_b.risk_approved,
      (unsigned long long)st->strat_b.orders_submitted,
      (unsigned long long)st->strat_b.fills,
      (unsigned long long)st->strat_b.wins,
      (unsigned long long)st->strat_b.losses,
      (double)st->strat_b.realized_pnl,
      (double)st->strat_b.fees);

    fclose(fp);
    return 0;
}

int pt_strat_stats_load(pt_strategy_stats_tracker_t *st, const char *path)
{
    if (!st || !path) return -1;
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    unsigned long long a_sig=0, a_wld=0, a_app=0, a_ord=0, a_fil=0, a_win=0, a_los=0;
    unsigned long long b_sig=0, b_wld=0, b_app=0, b_ord=0, b_fil=0, b_win=0, b_los=0;
    double a_pnl=0, a_fee=0, b_pnl=0, b_fee=0;

    int n = fscanf(fp, " { \"a_sig\": %llu, \"a_wld\": %llu, \"a_app\": %llu, \"a_ord\": %llu, \"a_fil\": %llu, \"a_win\": %llu, \"a_los\": %llu, \"a_pnl\": %lf, \"a_fee\": %lf, \"b_sig\": %llu, \"b_wld\": %llu, \"b_app\": %llu, \"b_ord\": %llu, \"b_fil\": %llu, \"b_win\": %llu, \"b_los\": %llu, \"b_pnl\": %lf, \"b_fee\": %lf }",
           &a_sig, &a_wld, &a_app, &a_ord, &a_fil, &a_win, &a_los, &a_pnl, &a_fee,
           &b_sig, &b_wld, &b_app, &b_ord, &b_fil, &b_win, &b_los, &b_pnl, &b_fee);
    fclose(fp);
    if (n >= 18) {
        st->strat_a.signals = a_sig;
        st->strat_a.would_trade = a_wld;
        st->strat_a.risk_approved = a_app;
        st->strat_a.orders_submitted = a_ord;
        st->strat_a.fills = a_fil;
        st->strat_a.wins = a_win;
        st->strat_a.losses = a_los;
        st->strat_a.realized_pnl = a_pnl;
        st->strat_a.fees = a_fee;
        st->strat_a.net_pnl = a_pnl - a_fee;
        if (a_win + a_los > 0) st->strat_a.win_rate = (double)a_win / (double)(a_win + a_los);

        st->strat_b.signals = b_sig;
        st->strat_b.would_trade = b_wld;
        st->strat_b.risk_approved = b_app;
        st->strat_b.orders_submitted = b_ord;
        st->strat_b.fills = b_fil;
        st->strat_b.wins = b_win;
        st->strat_b.losses = b_los;
        st->strat_b.realized_pnl = b_pnl;
        st->strat_b.fees = b_fee;
        st->strat_b.net_pnl = b_pnl - b_fee;
        if (b_win + b_los > 0) st->strat_b.win_rate = (double)b_win / (double)(b_win + b_los);
    }
    return 0;
}

static void format_strat_json_(const pt_single_strat_stats_t *s, char *buf, size_t sz)
{
    uint64_t st = s->wins + s->losses;
    double wr = st > 0 ? (double)s->wins / (double)st : 0.0;
    double fr = s->orders_submitted > 0 ? (double)s->fills / (double)s->orders_submitted : 0.0;
    double a_exp = s->risk_approved > 0 ? s->sum_expected_edge / (double)s->risk_approved : 0.0;
    double a_exe = s->risk_approved > 0 ? s->sum_executable_edge / (double)s->risk_approved : 0.0;
    double a_rea = st > 0 ? s->sum_realized_edge / (double)st : 0.0;

    snprintf(buf, sz,
        "{\"signals\":%llu,\"would_trade\":%llu,\"risk_approved\":%llu,\"orders_submitted\":%llu,"
        "\"fills\":%llu,\"partial_fills\":%llu,\"cancelled\":%llu,\"wins\":%llu,\"losses\":%llu,"
        "\"arbs_completed\":%llu,\"arbs_hedged\":%llu,\"sum_hedge_cost\":%.2f,"
        "\"win_rate\":%.4f,\"gross_profit\":%.2f,\"gross_loss\":%.2f,\"realized_pnl\":%.2f,"
        "\"fees\":%.2f,\"rebates\":%.2f,\"net_pnl\":%.2f,"
        "\"average_expected_edge\":%.4f,\"average_executable_edge\":%.4f,\"average_realized_edge\":%.4f,\"fill_rate\":%.3f}",
        (unsigned long long)s->signals, (unsigned long long)s->would_trade, (unsigned long long)s->risk_approved,
        (unsigned long long)s->orders_submitted, (unsigned long long)s->fills, (unsigned long long)s->partial_fills,
        (unsigned long long)s->cancelled, (unsigned long long)s->wins, (unsigned long long)s->losses,
        (unsigned long long)s->arbs_completed, (unsigned long long)s->arbs_hedged, s->sum_hedge_cost,
        wr, s->gross_profit, s->gross_loss, s->realized_pnl, s->fees, s->rebates, s->net_pnl,
        a_exp, a_exe, a_rea, fr);
}

void pt_strat_stats_json_analytics(const pt_strategy_stats_tracker_t *st, char *buf, size_t max_len)
{
    if (!st || !buf || max_len == 0) return;

    if (st->lifecycle) {
        pt_lifecycle_compute_stats((pt_lifecycle_tracker_t *)st->lifecycle, st->adverse);
    }

    uint64_t total_orders = st->strat_a.orders_submitted + st->strat_b.orders_submitted;
    uint64_t total_fills  = st->strat_a.fills + st->strat_b.fills;
    uint64_t total_trades = st->strat_a.wins + st->strat_a.losses + st->strat_b.wins + st->strat_b.losses;

    double fill_ratio = total_orders > 0 ? (double)total_fills / (double)total_orders : 0.0;
    double avg_slippage = total_fills > 0 ? (st->strat_a.sum_slippage_bps + st->strat_b.sum_slippage_bps) / (double)total_fills : 0.0;

    double exp_edge = 0.0, exec_edge = 0.0, real_edge = 0.0, adverse_bps = 0.0;
    if (st->lifecycle && st->lifecycle->completed_count > 0) {
        exp_edge = st->lifecycle->avg_theoretical_edge;
        exec_edge = st->lifecycle->avg_executable_edge;
        real_edge = st->lifecycle->avg_realized_edge;
        adverse_bps = (st->adverse && st->adverse->count > 0) ? st->adverse->overall_avg_adv_bps : st->lifecycle->avg_adverse_selection;
    } else {
        uint64_t app_signals = st->strat_a.risk_approved + st->strat_b.risk_approved;
        if (app_signals > 0) {
            exp_edge = (st->strat_a.sum_expected_edge + st->strat_b.sum_expected_edge) / (double)app_signals;
            exec_edge = (st->strat_a.sum_executable_edge + st->strat_b.sum_executable_edge) / (double)app_signals;
        }
        if (total_trades > 0) {
            real_edge = (st->strat_a.sum_realized_edge + st->strat_b.sum_realized_edge) / (double)total_trades;
        }
        if (st->adverse && st->adverse->count > 0) adverse_bps = st->adverse->overall_avg_adv_bps;
    }

    uint64_t total_q_obs = st->strat_a.queue_observations + st->strat_b.queue_observations;
    uint64_t avg_queue_ahead = total_q_obs > 0 ? (uint64_t)((st->strat_a.sum_queue_ahead + st->strat_b.sum_queue_ahead) / (double)total_q_obs) : 0;

    double brier = st->calibration ? st->calibration->brier_score : 0.0;
    double calib_err = st->calibration ? st->calibration->expected_calibration_error : 0.0;
    double gross_profit = st->strat_a.gross_profit + st->strat_b.gross_profit;
    double gross_loss = st->strat_a.gross_loss + st->strat_b.gross_loss;
    double profit_factor = (gross_loss > 0.0001) ? (gross_profit / gross_loss) : (gross_profit > 0.0001 ? 10.0 : 0.0);

    int has_sharpe = 0;
    double sharpe = pt_strat_stats_calc_sharpe(st, &has_sharpe);

    char a_buf[512], b_buf[512];
    format_strat_json_(&st->strat_a, a_buf, sizeof(a_buf));
    format_strat_json_(&st->strat_b, b_buf, sizeof(b_buf));

    snprintf(buf, max_len,
        "{"
        "\"expected_edge_avg\":%.4f,\"executable_edge_avg\":%.4f,\"realized_edge_avg\":%.4f,"
        "\"fill_ratio\":%.3f,\"adverse_selection_bps\":%.2f,\"avg_slippage_bps\":%.2f,"
        "\"avg_queue_ahead\":%llu,\"brier_score\":%.4f,\"calibration_error\":%.4f,"
        "\"profit_factor\":%.2f,\"sharpe\":%.2f,\"has_sharpe\":%d,\"total_trades\":%llu,"
        "\"strat_a\":%s,\"strat_b\":%s"
        "}",
        exp_edge, exec_edge, real_edge, fill_ratio, adverse_bps, avg_slippage,
        (unsigned long long)avg_queue_ahead, brier, calib_err, profit_factor, sharpe, has_sharpe,
        (unsigned long long)total_trades, a_buf, b_buf);
}
