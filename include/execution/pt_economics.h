#ifndef PMT_PT_ECONOMICS_H
#define PMT_PT_ECONOMICS_H

#include "core/ptypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double maker_fee_bps;       /* Maker fee in bps (negative = rebate) */
    double taker_fee_bps;       /* Taker fee in bps (e.g. 2.0 = 0.02%) */
    double maker_rebate_bps;    /* Specific maker rebate reward if applicable */
    double fixed_order_fee_usd; /* Fixed gas/settlement cost per order */
    double dynamic_fee_scale;   /* Dynamic scale factor based on implied probability */
} pt_fee_schedule_t;

typedef struct {
    double gross_edge;          /* Raw theoretical edge before any frictions */
    double fees;                /* Taker or maker fees (probability units) */
    double rebates;             /* Liquidity provision rewards */
    double spread_cost;         /* Half-spread or full spread paid */
    double slippage;            /* Modeled or realized market impact */
    double latency_cost;        /* Decay during in-flight network transit */
    double adverse_selection;   /* Price drift post-fill */
    double hedging_cost;        /* Expected / realized second-leg balancing cost */
    double net_edge;            /* gross_edge - fees + rebates - spread - slip - latency - adv - hedge */
} pt_edge_breakdown_t;

typedef struct {
    double theoretical_edge;    /* Initial edge at signal generation */
    double executable_edge;     /* Edge available after walking visible depth */
    double net_edge;            /* Net edge after theoretical friction buffer */
    double realized_edge;       /* Actual edge realized at fill settlement */
    double realized_pnl;        /* Realized dollar PnL */
} pt_opportunity_edge_t;

void pt_fee_schedule_default(pt_fee_schedule_t *s);

/* Calculate all economic cost components for a maker or taker trade */
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
                       pt_edge_breakdown_t *out);

/* Calculate fee amount in USD */
double pt_economics_fee_usd(const pt_fee_schedule_t *fees, int is_taker,
                            pt_price_t price_scaled, pt_size_t size);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_ECONOMICS_H */
