#ifndef PMT_PT_ORDER_H
#define PMT_PT_ORDER_H

#include "core/ptypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PT_OSTATE_CREATED   = 0,
    PT_OSTATE_LIVE,      /* resting on book */
    PT_OSTATE_PARTIAL,   /* some but not all filled */
    PT_OSTATE_FILLED,
    PT_OSTATE_CANCELLED,
    PT_OSTATE_STALE_PENDING_CANCEL,
    PT_OSTATE_REJECTED,
    PT_OSTATE_UNKNOWN    /* api lost track -> reconcile needed */
} pt_order_state_t;

typedef enum {
    PT_OTYPE_LIMIT = 0,
    PT_OTYPE_MARKET  /* allowed only when explicitly enabled */
} pt_order_type_t;

typedef enum {
    PT_STRAT_PARITY5M = 0,
    PT_STRAT_FLOW15M,
    PT_STRAT_UNKNOWN
} pt_strategy_t;

/* a live or resting order (fixed-size struct, no heap) */
typedef struct {
    pt_order_id_t   id;
    int             is_yes;        /* which outcome token */
    int             side;          /* PT_SIDE_BID / PT_SIDE_ASK */
    pt_price_t      price;         /* limit price (scaled) */
    pt_size_t       original_size;
    pt_size_t       filled_size;
    pt_order_state_t state;
    int             mode;          /* PT_MODE_BACKTEST/PAPER/LIVE */
    int             strategy;
    pt_flow_id_t    arb_id;        /* linkage to arb operation (0 = standalone) */
    pt_nsec_t       submit_t;
    pt_nsec_t       ack_t;
    pt_nsec_t       last_update_t;
} pt_order_t;

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_ORDER_H */