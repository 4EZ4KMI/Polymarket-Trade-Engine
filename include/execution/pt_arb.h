#ifndef PMT_PT_ARB_H
#define PMT_PT_ARB_H

#include "core/ptypes.h"
#include "orderbook/pt_book.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double taker_fee_bps;      /* e.g. 2.0 = 0.02% of notional per leg */
    double slippage_bps;       /* modeled slippage on crossing spread */
    double latency_buffer_pct; /* prob points 0..1 (e.g. 0.002) */
    double risk_buffer_pct;    /* prob points 0..1 */
    double min_edge_pct;       /* net edge must be >= this (0..1) */
    pt_size_t min_liquidity;   /* required executable depth per leg */
} pt_arb_cfg_t;

typedef struct {
    int       has_opportunity;
    double    gross_edge;          /* 1 - (best asks sum), prob units */
    double    net_edge;            /* after fees/slippage/buffers, prob units */
    pt_size_t max_executable_size; /* largest size still profitable */
    double    expected_profit;     /* net_edge * size (USD approx) */
    double    confidence;          /* 0..1 */
    int       levels_used_yes, levels_used_no;
    int64_t   avg_yes_scaled, avg_no_scaled;
} pt_arb_opp_t;

/* EVALUATE a YES/NO executable-arbitrage on the ASK sides of both books.
 * max_want = max shares we will consider per leg. */
int pt_arb_calc(const pt_book_t *yes, const pt_book_t *no,
                const pt_arb_cfg_t *cfg, pt_size_t max_want,
                pt_arb_opp_t *out);

/* net edge at a given size (prob units); returns 0 if size not executable. */
int pt_arb_net_at(const pt_book_t *yes, const pt_book_t *no,
                  const pt_arb_cfg_t *cfg, pt_size_t size,
                  double *net_edge, int64_t *avg_yes, int64_t *avg_no);

double pt_arb_gross_edge_level1(const pt_book_t *yes, const pt_book_t *no);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_ARB_H */