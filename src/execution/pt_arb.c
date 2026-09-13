#include "execution/pt_arb.h"
#include <math.h>
#include <string.h>

static double clamp01_(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }

int pt_arb_net_at(const pt_book_t *yes, const pt_book_t *no,
                  const pt_arb_cfg_t *cfg, pt_size_t size,
                  double *net_edge, int64_t *avg_yes, int64_t *avg_no)
{
    if (size == 0 || yes == NULL || no == NULL || cfg == NULL)
        return 0;
    int64_t ay = 0, an = 0; int uy = 0, un = 0;
    pt_size_t fy = pt_book_walk(yes, PT_SIDE_ASK, size, &ay, &uy);
    pt_size_t fn = pt_book_walk(no,  PT_SIDE_ASK, size, &an, &un);
    if (fy < size || fn < size)
        return 0; /* cannot fill this size on both legs */
    (void)uy; (void)un;

    /* average pay probabilities */
    double pay_yes = (double)ay / (double)PT_PRICE_SCALE;
    double pay_no  = (double)an / (double)PT_PRICE_SCALE;
    double gross = 1.0 - (pay_yes + pay_no);

    /* frictions in probability-point terms */
    double fee_rate = cfg->taker_fee_bps / 10000.0;
    double fees = (pay_yes + pay_no) * fee_rate;
    double slip = (pay_yes + pay_no) * (cfg->slippage_bps / 10000.0);
    double buff = cfg->latency_buffer_pct + cfg->risk_buffer_pct;

    double net = gross - fees - slip - buff;
    if (avg_yes) *avg_yes = ay;
    if (avg_no)  *avg_no  = an;
    if (net_edge) *net_edge = net;
    return 1;
}

int pt_arb_calc(const pt_book_t *yes, const pt_book_t *no,
                const pt_arb_cfg_t *cfg, pt_size_t max_want,
                pt_arb_opp_t *out)
{
    if (out == NULL) return -1;
    memset(out, 0, sizeof(*out));
    if (yes == NULL || no == NULL || cfg == NULL || max_want == 0)
        return -1;

    pt_price_t ya = 0, na = 0;
    if (pt_book_best_ask(yes, &ya, NULL) || pt_book_best_ask(no, &na, NULL))
        return 0; /* no book */
    out->gross_edge = 1.0 - ((double)ya + (double)na) / (double)PT_PRICE_SCALE;
    if (out->gross_edge <= cfg->min_edge_pct)
        return 0;

    /* Binary search largest size with net_edge >= min_edge, on [1,max_want]. */
    pt_size_t lo = 1, hi = max_want, best = 0;
    double best_net = 0.0; int64_t bay = 0, ban = 0;
    while (lo <= hi) {
        pt_size_t mid = lo + (hi - lo) / 2;
        double net = -INFINITY; int64_t ay = 0, an = 0;
        if (pt_arb_net_at(yes, no, cfg, mid, &net, &ay, &an) &&
            net >= cfg->min_edge_pct) {
            best = mid; best_net = net; bay = ay; ban = an;
            lo = mid + 1;
        } else {
            hi = (mid > 1) ? mid - 1 : 0;
        }
    }

    if (best == 0 || best < cfg->min_liquidity) {
        out->net_edge = 0.0; out->max_executable_size = 0;
        out->has_opportunity = 0; out->expected_profit = 0.0; out->confidence = 0.0;
        return 0;
    }

    out->has_opportunity = 1;
    out->net_edge = best_net;
    out->max_executable_size = best;
    out->avg_yes_scaled = bay;
    out->avg_no_scaled  = ban;
    out->expected_profit = best_net * (double)best;
    double fy = (best) ? best : 1;
    out->levels_used_yes = 1; out->levels_used_no = 1;
    /* robustness: fraction of gross edge that survives level-1 frictions */
    double fee_rate = cfg->taker_fee_bps / 10000.0;
    double l1_buff = (double)(ya + na) / (double)PT_PRICE_SCALE *
                     (fee_rate + cfg->slippage_bps / 10000.0) +
                     cfg->latency_buffer_pct + cfg->risk_buffer_pct;
    double gross = out->gross_edge;
    double margin_conf = (gross > 0) ? clamp01_((gross - l1_buff) / gross) : 0.0;
    double liq_conf = (cfg->min_liquidity > 0)
        ? clamp01_((double)best / (double)cfg->min_liquidity)
        : 1.0;
    out->confidence = margin_conf * liq_conf;
    (void)fy;
    return 1;
}

double pt_arb_gross_edge_level1(const pt_book_t *yes, const pt_book_t *no)
{
    pt_price_t ya = 0, na = 0;
    if (pt_book_best_ask(yes, &ya, NULL) || pt_book_best_ask(no, &na, NULL))
        return NAN;
    return 1.0 - ((double)ya + (double)na) / (double)PT_PRICE_SCALE;
}