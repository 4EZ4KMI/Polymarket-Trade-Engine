#include "execution/pt_economics.h"
#include <string.h>
#include <math.h>

void pt_fee_schedule_default(pt_fee_schedule_t *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->maker_fee_bps = 0.0;       /* 0 bps for maker on Polymarket */
    s->taker_fee_bps = 2.0;       /* 2 bps standard baseline */
    s->maker_rebate_bps = 0.5;    /* 0.5 bps maker rebate incentive */
    s->fixed_order_fee_usd = 0.0;
    s->dynamic_fee_scale = 1.0;
}

void pt_economics_calc(const pt_fee_schedule_t *fees,
                       int is_taker,
                       double raw_prob_edge,
                       pt_price_t price_scaled,
                       pt_size_t size,
                       double spread_prob,
                       double slippage_bps,
                       double latency_buffer,
                       double adverse_sel_bps,
                       double hedge_buffer,
                       pt_edge_breakdown_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->gross_edge = raw_prob_edge;

    double p = (double)price_scaled / (double)PT_PRICE_SCALE;
    if (p <= 0.0) p = 0.5;

    pt_fee_schedule_t default_fees;
    if (!fees) {
        pt_fee_schedule_default(&default_fees);
        fees = &default_fees;
    }

    if (is_taker) {
        out->fees = p * (fees->taker_fee_bps / 10000.0) * fees->dynamic_fee_scale;
        out->rebates = 0.0;
        out->spread_cost = spread_prob > 0.0 ? (spread_prob / 2.0) : 0.0;
    } else {
        out->fees = fees->maker_fee_bps > 0 ? (p * (fees->maker_fee_bps / 10000.0)) : 0.0;
        out->rebates = p * (fees->maker_rebate_bps / 10000.0);
        out->spread_cost = 0.0; /* Maker captures spread */
    }

    if (fees->fixed_order_fee_usd > 0.0 && size > 0) {
        out->fees += fees->fixed_order_fee_usd / (double)size;
    }

    out->slippage = p * (slippage_bps / 10000.0);
    out->latency_cost = latency_buffer;
    out->adverse_selection = p * (adverse_sel_bps / 10000.0);
    out->hedging_cost = hedge_buffer;

    out->net_edge = out->gross_edge - out->fees + out->rebates -
                    out->spread_cost - out->slippage - out->latency_cost -
                    out->adverse_selection - out->hedging_cost;
}

double pt_economics_fee_usd(const pt_fee_schedule_t *fees, int is_taker,
                            pt_price_t price_scaled, pt_size_t size)
{
    if (size == 0) return 0.0;
    double notional = ((double)price_scaled / (double)PT_PRICE_SCALE) * (double)size;
    pt_fee_schedule_t default_fees;
    if (!fees) {
        pt_fee_schedule_default(&default_fees);
        fees = &default_fees;
    }
    if (is_taker) {
        return notional * (fees->taker_fee_bps / 10000.0) + fees->fixed_order_fee_usd;
    } else {
        return notional * (fees->maker_fee_bps / 10000.0) - notional * (fees->maker_rebate_bps / 10000.0);
    }
}
