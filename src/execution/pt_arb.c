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
        return 0; /* insufficient depth on one or both legs */

    double pay_yes = (double)ay / (double)PT_PRICE_SCALE;
    double pay_no  = (double)an / (double)PT_PRICE_SCALE;
    double gross = 1.0 - (pay_yes + pay_no);

    pt_edge_breakdown_t bd;
    pt_economics_calc(&cfg->fees, 1, gross, (pt_price_t)((ay + an) / 2),
                      size, 0.0, cfg->slippage_bps, cfg->latency_buffer_pct,
                      0.0, cfg->risk_buffer_pct, &bd);

    if (avg_yes) *avg_yes = ay;
    if (avg_no)  *avg_no  = an;
    if (net_edge) *net_edge = bd.net_edge;
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
    pt_size_t y_sz = 0, n_sz = 0;
    if (pt_book_best_ask(yes, &ya, &y_sz) || pt_book_best_ask(no, &na, &n_sz))
        return 0; /* empty book */

    out->raw_edge = 1.0 - ((double)ya + (double)na) / (double)PT_PRICE_SCALE;
    if (out->raw_edge <= cfg->min_edge_pct)
        return 0;

    /* Binary search largest size with net_edge >= min_edge on [1, max_want] */
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
        out->has_opportunity = 0;
        return 0;
    }

    out->has_opportunity = 1;
    out->max_executable_size = best;
    out->avg_yes_scaled = bay;
    out->avg_no_scaled  = ban;
    out->executable_edge = 1.0 - ((double)bay + (double)ban) / (double)PT_PRICE_SCALE;
    out->net_edge = best_net;
    out->expected_net_profit = best_net * (double)best;

    /* Economic breakdown */
    pt_economics_calc(&cfg->fees, 1, out->executable_edge,
                      (pt_price_t)((bay + ban) / 2), best,
                      0.0, cfg->slippage_bps, cfg->latency_buffer_pct,
                      0.0, cfg->risk_buffer_pct, &out->breakdown);

    /* Fill probabilities per leg based on book depth at target level */
    double q_yes = (double)y_sz;
    double q_no  = (double)n_sz;
    out->fill_probability_leg_A = clamp01_((double)best / ((double)best + q_yes * 0.5));
    out->fill_probability_leg_B = clamp01_((double)best / ((double)best + q_no * 0.5));
    out->joint_fill_probability = out->fill_probability_leg_A * out->fill_probability_leg_B;

    /* Worst case loss: Leg A fills at bay, Leg B fails and we dump Leg A at best bid */
    pt_price_t y_bid = 0;
    pt_book_best_bid(yes, &y_bid, NULL);
    double dump_loss_per_share = ((double)bay - (double)y_bid) / (double)PT_PRICE_SCALE;
    if (dump_loss_per_share < 0.0) dump_loss_per_share = 0.02;
    out->hedge_cost = dump_loss_per_share * (double)best;
    out->worst_case_loss = out->hedge_cost + pt_economics_fee_usd(&cfg->fees, 1, (pt_price_t)bay, best);

    /* Confidence calculation */
    double margin_conf = clamp01_(out->net_edge / (out->raw_edge + 1e-6));
    double liq_conf = (cfg->min_liquidity > 0)
        ? clamp01_((double)best / (double)cfg->min_liquidity)
        : 1.0;
    out->confidence = clamp01_(margin_conf * 0.6 + out->joint_fill_probability * 0.4) * liq_conf;

    return 1;
}

double pt_arb_gross_edge_level1(const pt_book_t *yes, const pt_book_t *no)
{
    pt_price_t ya = 0, na = 0;
    if (pt_book_best_ask(yes, &ya, NULL) || pt_book_best_ask(no, &na, NULL))
        return NAN;
    return 1.0 - ((double)ya + (double)na) / (double)PT_PRICE_SCALE;
}
