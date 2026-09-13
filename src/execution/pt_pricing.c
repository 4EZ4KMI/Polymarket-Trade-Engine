#include "execution/pt_pricing.h"
#include <math.h>
#include <stddef.h>

/* Evaluate EV at a candidate limit price (scaled). For buying at price P,
 * EV/share = fp * (fair - P) / PT_PRICE_SCALE. For selling, symmetric. */
static double eval_(const pt_pricing_input_t *in, pt_price_t P, double *fp_out)
{
    int step = (in->ticks_per_step <= 0) ? 1 : in->ticks_per_step;
    pt_price_t best = in->best;
    double dist_ticks, qahead, sp, vel, tte, hfr;
    int atbest;

    if (in->side == PT_SIDE_ASK) {
        /* we are buying; price at or above best; inside means cheaper (<= best) */
        atbest = (P == best);
        dist_ticks = (P >= best) ? (double)((P - best) / step) : 0.0;
        qahead = dist_ticks == 0.0 ? in->queue_ahead : 0.0;
    } else {
        atbest = (P == best);
        dist_ticks = (P <= best) ? (double)((best - P) / step) : 0.0;
        qahead = dist_ticks == 0.0 ? in->queue_ahead : 0.0;
    }
    sp = in->spread_ticks; vel = in->velocity_pct_per_sec;
    tte = in->time_to_expiry_sec; hfr = in->hist_fill_rate;

    pt_fill_params_t fp = {
        .distance_ticks = dist_ticks,
        .at_best = atbest,
        .queue_ahead = qahead,
        .order_size = (double)in->order_size,
        .spread_ticks = sp,
        .velocity_pct_per_sec = vel,
        .time_to_expiry_sec = tte,
        .hist_fill_rate = hfr
    };
    double fpv = pt_fill_prob_default(&fp);
    if (fp_out) *fp_out = fpv;

    double edge;
    if (in->side == PT_SIDE_ASK)
        edge = ((double)in->fair - (double)P) / (double)PT_PRICE_SCALE;
    else
        edge = ((double)P - (double)in->fair) / (double)PT_PRICE_SCALE;
    return fpv * edge;
}

void pt_pricing_optimize(const pt_pricing_input_t *in, pt_pricing_output_t *out)
{
    if (in == NULL || out == NULL) return;
    int step = (in->ticks_per_step <= 0) ? 1 : in->ticks_per_step;
    out->limit_price = in->best;
    out->fill_prob = 0.0; out->ev_per_share = -INFINITY; out->est_ev_total = 0.0;
    out->at_best = 1;

    int inside = (in->max_ticks_inside <= 0) ? 0 : in->max_ticks_inside;
    double best_ev = -INFINITY;
    pt_price_t best_p = in->best;

    /* search best first, then away ticks, then inside ticks */
    double fp = 0.0;
    double ev = eval_(in, in->best, &fp);
    if (ev > best_ev) { best_ev = ev; best_p = in->best; }

    /* away ticks (out of market) */
    if (in->side == PT_SIDE_ASK) {
        for (int i = 1; i <= inside; i++) {
            pt_price_t P = in->best + (pt_price_t)i * step;
            double f; double e = eval_(in, P, &f);
            if (e > best_ev) { best_ev = e; best_p = P; }
        }
    } else {
        for (int i = 1; i <= inside; i++) {
            pt_price_t P = in->best - (pt_price_t)i * step;
            double f; double e = eval_(in, P, &f);
            if (e > best_ev) { best_ev = e; best_p = P; }
        }
    }

    out->limit_price = best_p;
    out->at_best = (best_p == in->best);
    (void)eval_(in, out->limit_price, &out->fill_prob);
    out->ev_per_share = best_ev;
    out->est_ev_total = best_ev * (double)in->order_size;
}