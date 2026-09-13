#ifndef PMT_PT_ORDER_H
#define PMT_PT_ORDER_H

#include "core/ptypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PT_OSTATE_CREATED   = 0,
    PT_OSTATE_PENDING_SUBMIT,
    PT_OSTATE_LIVE,      /* resting on book */
    PT_OSTATE_PARTIAL,   /* some but not all filled */
    PT_OSTATE_FILLED,
    PT_OSTATE_PENDING_CANCEL,
    PT_OSTATE_CANCELLED,
    PT_OSTATE_STALE_PENDING_CANCEL,
    PT_OSTATE_REJECTED,
    PT_OSTATE_EXPIRED,
    PT_OSTATE_UNKNOWN    /* api lost track -> reconcile needed */
} pt_order_state_t;

typedef enum {
    PT_OTYPE_LIMIT = 0,
    PT_OTYPE_MARKET,  /* allowed only when explicitly enabled */
    PT_OTYPE_IOC,
    PT_OTYPE_FOK
} pt_order_type_t;

typedef enum {
    PT_STRAT_PARITY5M = 0,
    PT_STRAT_FLOW15M,
    PT_STRAT_UNKNOWN
} pt_strategy_t;

/* a live or resting order (fixed-size struct, no heap) */
typedef struct {
    pt_order_id_t    id;
    uint64_t         signal_id;
    pt_market_id_t   market_id;
    pt_token_id_t    token_id;
    int              is_yes;        /* which outcome token */
    int              side;          /* PT_SIDE_BID / PT_SIDE_ASK */
    pt_order_type_t  type;
    pt_price_t       price;         /* limit price (scaled) */
    pt_size_t        original_size;
    pt_size_t        remaining_size;
    pt_size_t        filled_size;
    int64_t          cum_cost_scaled; /* sum(fill_price * fill_size) */
    pt_price_t       avg_fill_price;
    pt_order_state_t state;
    int              mode;          /* PT_MODE_BACKTEST/PAPER/LIVE */
    int              strategy;
    pt_flow_id_t     arb_id;        /* linkage to arb operation (0 = standalone) */
    
    /* Realistic Queue & Latency Breakdown */
    pt_size_t        queue_ahead;   /* current remaining shares ahead in matching engine */
    pt_size_t        initial_queue; /* shares ahead at arrival time */
    pt_nsec_t        created_t;     /* engine decision timestamp */
    pt_nsec_t        submit_t;      /* network departure timestamp */
    pt_nsec_t        arrival_t;     /* matching engine entry timestamp */
    pt_nsec_t        ack_t;         /* engine received ack timestamp */
    pt_nsec_t        cancel_req_t;  /* cancel request initiated */
    pt_nsec_t        cancel_ack_t;  /* cancel confirmed */
    pt_nsec_t        last_update_t;
    
    /* Cost & Edge Decomposition */
    double           fee_paid;
    double           rebate_earned;
    double           slippage_cost;
    double           adverse_selection;
} pt_order_t;

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_ORDER_H */