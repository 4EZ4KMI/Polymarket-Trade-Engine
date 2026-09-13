#ifndef PMT_PT_FLOW_SKEW_H
#define PMT_PT_FLOW_SKEW_H

#include "core/ptypes.h"
#include "features/pt_features.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double min_btc_momentum_pct;  /* threshold for 1s/2s BTC return (e.g. 0.05%) */
    double min_btc_velocity;      /* price units/sec */
    double min_polymarket_imbalance; /* L1/L3 imbalance (e.g. 0.15) */
    double max_polymarket_spread; /* max allowed spread in prob units (e.g. 0.04) */
    double min_expected_edge;     /* prob units (e.g. 0.015) */
    double min_confidence;        /* 0..1 */
    pt_size_t default_order_size;
    double time_to_expiry_min_sec;/* do not trade closer than N sec to expiry */
    double time_to_expiry_max_sec;
} pt_flow_skew_cfg_t;

typedef struct {
    int         has_signal;
    pt_dir_t    dir;              /* BUY (YES) or SELL (equivalent to BUY NO) */
    int         is_yes;           /* 1=YES, 0=NO */
    pt_price_t  target_price;     /* limit price suggestion (scaled) */
    double      expected_edge;    /* prob units */
    double      expected_profit;
    double      confidence;
    double      fill_probability;
    pt_size_t   max_size;
    double      risk_score;       /* 0=safe, 1=high risk */
    /* feature snapshot */
    double      btc_mom_1s, btc_mom_5s;
    double      poly_imb_l3;
    double      poly_spread;
} pt_flow_signal_t;

/* Pure-function signal evaluation over current features. */
int pt_flow_skew_eval(const pt_flow_skew_cfg_t *cfg,
                      const pt_market_features_t *poly_yes,
                      const pt_market_features_t *poly_no,
                      const pt_btc_window_t *btc_windows, /* [PT_BTC_NTF] */
                      double btc_mid,
                      double time_to_expiry_sec,
                      pt_flow_signal_t *out);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_FLOW_SKEW_H */