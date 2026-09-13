#ifndef PMT_PT_MODEL_H
#define PMT_PT_MODEL_H

#include "core/ptypes.h"
#include "features/pt_features.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PT_MODEL_HEURISTIC = 1, /* Baseline momentum heuristic */
    PT_MODEL_EMPIRICAL  = 2, /* Non-parametric empirical quantile lookup */
    PT_MODEL_LOGISTIC   = 3  /* Parametric logistic regression */
} pt_model_type_t;

/* Rich statistical feature vector combining BTC and Polymarket micro-signals */
typedef struct {
    /* BTC returns across timescales */
    double btc_ret_10ms;
    double btc_ret_50ms;
    double btc_ret_100ms;
    double btc_ret_500ms;
    double btc_ret_1s;
    double btc_ret_5s;
    double btc_ret_10s;
    
    /* BTC dynamics */
    double btc_velocity;
    double btc_acceleration;
    double btc_realized_vol;
    double btc_trade_imbalance;
    double btc_buy_vol;
    double btc_sell_vol;
    
    /* Polymarket state */
    pt_price_t yes_bid, yes_ask;
    pt_price_t no_bid,  no_ask;
    double     spread;
    double     imb_l1, imb_l3, imb_l5, imb_l10;
    double     microprice;
    pt_size_t  depth_l1_shares;
    double     poly_trade_flow_ratio;
    
    /* Time & expiry */
    double     time_to_expiry_sec;
    double     seconds_since_open;
} pt_feature_vector_t;

typedef struct {
    pt_model_type_t type;
    /* Logistic model weights: w0 + w_ret1s*ret_1s + w_ret5s*ret_5s + w_imb*imb_l3 + w_vol*vol */
    double w0;
    double w_ret1s;
    double w_ret5s;
    double w_imb;
    double w_velocity;
    double w_expiry;
} pt_model_cfg_t;

/* Compute predicted probability for YES winning outcome */
double pt_model_predict_prob(const pt_model_cfg_t *cfg,
                             const pt_feature_vector_t *features);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_MODEL_H */
