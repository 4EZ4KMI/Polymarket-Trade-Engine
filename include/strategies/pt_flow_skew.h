#ifndef PMT_PT_FLOW_SKEW_H
#define PMT_PT_FLOW_SKEW_H

#include "core/ptypes.h"
#include "features/pt_features.h"
#include "strategies/pt_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    pt_model_cfg_t model;
    double         min_btc_momentum_pct;  /* threshold for 1s/2s BTC return */
    double         min_btc_velocity;      /* price units/sec */
    double         min_polymarket_imbalance; /* L1/L3 imbalance */
    double         max_polymarket_spread; /* max allowed spread in prob units */
    double         min_expected_edge;     /* executable edge threshold */
    double         min_confidence;        /* 0..1 */
    pt_size_t      default_order_size;
    double         time_to_expiry_min_sec;/* do not trade closer than N sec to expiry */
    double         time_to_expiry_max_sec;
    double         taker_fee_bps;
    double         slippage_bps;
} pt_flow_skew_cfg_t;

typedef struct {
    int                 has_signal;
    pt_dir_t            dir;              /* BUY YES or BUY NO */
    int                 is_yes;           /* 1=YES, 0=NO */
    pt_price_t          target_price;     /* limit price suggestion (scaled) */
    double              model_probability;/* P(YES) predicted by model */
    double              market_probability;/* Mid-implied probability */
    double              raw_edge;         /* model_prob - market_prob */
    double              executable_edge;  /* raw_edge - costs */
    double              expected_profit;
    double              confidence;
    double              fill_probability;
    pt_size_t           max_size;
    double              risk_score;       /* 0=safe, 1=high risk */
    pt_feature_vector_t features;         /* Complete captured feature vector */
} pt_flow_signal_t;

/* Pure-function signal evaluation over feature vector & model */
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
