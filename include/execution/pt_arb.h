#ifndef PMT_PT_ARB_H
#define PMT_PT_ARB_H

#include "core/ptypes.h"
#include "orderbook/pt_book.h"
#include "execution/pt_economics.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    pt_fee_schedule_t fees;
    double            slippage_bps;       /* modeled slippage on crossing spread */
    double            latency_buffer_pct; /* prob points 0..1 (e.g. 0.001) */
    double            risk_buffer_pct;    /* prob points 0..1 */
    double            min_edge_pct;       /* net edge must be >= this (0..1) */
    pt_size_t         min_liquidity;      /* required executable depth per leg */
    double            max_unhedged_loss;  /* max dollar loss tolerated per arb */
} pt_arb_cfg_t;

typedef struct {
    int                 has_opportunity;
    double              raw_edge;               /* 1.0 - (best_yes_ask + best_no_ask) */
    double              executable_edge;        /* 1.0 - (vwap_yes + vwap_no) */
    double              net_edge;               /* after fees/slippage/latency/hedge buffers */
    pt_size_t           max_executable_size;    /* max volume fillable on both legs profitably */
    double              expected_net_profit;    /* net_edge * max_executable_size (USD) */
    double              fill_probability_leg_A; /* fill prob estimate for Leg YES */
    double              fill_probability_leg_B; /* fill prob estimate for Leg NO */
    double              joint_fill_probability; /* P(A)*P(B) */
    double              hedge_cost;             /* expected cost to cross opposite book if lag leg fails */
    double              worst_case_loss;        /* max drawdown if Leg A fills and Leg B cannot fill */
    double              confidence;             /* combined metric [0..1] */
    int                 levels_used_yes;
    int                 levels_used_no;
    int64_t             avg_yes_scaled;         /* VWAP for YES leg */
    int64_t             avg_no_scaled;          /* VWAP for NO leg */
    pt_edge_breakdown_t breakdown;
} pt_arb_opp_t;

/* Evaluate multi-level VWAP parity arbitrage on ASK sides of both books. */
int pt_arb_calc(const pt_book_t *yes, const pt_book_t *no,
                const pt_arb_cfg_t *cfg, pt_size_t max_want,
                pt_arb_opp_t *out);

/* Evaluate net edge at exact size. Returns 1 if size executable, 0 if insufficient liquidity. */
int pt_arb_net_at(const pt_book_t *yes, const pt_book_t *no,
                  const pt_arb_cfg_t *cfg, pt_size_t size,
                  double *net_edge, int64_t *avg_yes, int64_t *avg_no);

double pt_arb_gross_edge_level1(const pt_book_t *yes, const pt_book_t *no);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_ARB_H */
