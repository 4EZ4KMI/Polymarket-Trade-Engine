#ifndef PMT_PT_PRICING_H
#define PMT_PT_PRICING_H

#include "core/ptypes.h"
#include "execution/pt_fill_prob.h"

#ifdef __cplusplus
extern "C" {
#endif

/* OrderPricingEngine: pick a limit price that maximizes expected value
 * per order = fill_prob * (fair - price) per share, subject to max payable.
 * Works in scaled integer price space. */

typedef struct {
    pt_price_t best;        /* best price on the side we want (best ask for buys) */
    pt_price_t fair;        /* fair value estimate (mid / microprice + edge) */
    pt_price_t mid;
    int        side;        /* PT_SIDE_BID / PT_SIDE_ASK (order side) */
    int        ticks_per_step;   /* step size in ticks (1 tick = 1 scaled unit) */
    pt_size_t  order_size;
    double     queue_ahead;
    double     spread_ticks;
    double     velocity_pct_per_sec;
    double     time_to_expiry_sec;
    double     hist_fill_rate;
    int        max_ticks_inside; /* how far inside best we may price */
    double     tick_value;       /* usd per tick per share (for EV display) */
} pt_pricing_input_t;

typedef struct {
    pt_price_t limit_price;         /* chosen (scaled) */
    double     fill_prob;
    double     ev_per_share;        /* expected value per share (prob units) */
    double     est_ev_total;        /* ev_per_share * order_size */
    int        at_best;
} pt_pricing_output_t;

void pt_pricing_optimize(const pt_pricing_input_t *in, pt_pricing_output_t *out);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_PRICING_H */