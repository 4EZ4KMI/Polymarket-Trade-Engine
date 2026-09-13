#ifndef PMT_PT_SIGNAL_H
#define PMT_PT_SIGNAL_H

#include "core/ptypes.h"
#include "strategies/pt_flow_skew.h"
#include "execution/pt_arb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t        signal_id;
    int             strategy;        /* PT_STRAT_PARITY5M / PT_STRAT_FLOW15M */
    pt_market_id_t  market_id;
    pt_nsec_t       timestamp;
    pt_dir_t        direction;       /* BUY */
    int             is_yes;          /* 1=YES, 0=NO outcome token */
    pt_price_t      target_price;    /* scaled */
    double          expected_edge;   /* prob units */
    double          expected_profit; /* USD */
    double          confidence;      /* 0..1 */
    double          fill_probability;/* 0..1 */
    pt_size_t       max_size;
    double          time_to_expiry;  /* seconds */
    double          risk_score;      /* 0..1 */
    /* feature snapshot */
    double          btc_mom_1s;
    double          poly_imb_l3;
    double          poly_spread;
} pt_signal_t;

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_SIGNAL_H */