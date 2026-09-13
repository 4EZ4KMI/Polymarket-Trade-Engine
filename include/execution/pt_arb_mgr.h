#ifndef PMT_PT_ARB_MGR_H
#define PMT_PT_ARB_MGR_H

#include "core/ptypes.h"
#include "orderbook/pt_book.h"
#include "execution/pt_arb.h"
#include "execution/pt_order.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PT_ARBMGR_MAX_OPS 64

typedef enum {
    PT_ARB_ST_IDLE = 0,
    PT_ARB_ST_SIGNAL_DETECTED,
    PT_ARB_ST_LEG_A_PENDING,
    PT_ARB_ST_LEG_A_PARTIAL,
    PT_ARB_ST_LEG_A_FILLED,
    PT_ARB_ST_LEG_B_PENDING,
    PT_ARB_ST_LEG_B_PARTIAL,
    PT_ARB_ST_BOTH_FILLED,
    PT_ARB_ST_HEDGE_REQUIRED,
    PT_ARB_ST_HEDGED,
    PT_ARB_ST_CANCELLED,
    PT_ARB_ST_EXPIRED,
    PT_ARB_ST_FAILED
} pt_arb_state_t;

typedef enum {
    PT_ARB_ACT_NONE = 0,
    PT_ARB_ACT_SUBMIT,
    PT_ARB_ACT_CANCEL,
    PT_ARB_ACT_REQUOTE,
    PT_ARB_ACT_MARKET_HEDGE, /* Emergency cross-spread market hedge */
    PT_ARB_ACT_COMPLETE,
    PT_ARB_ACT_ABORT
} pt_arb_act_t;

/* Per-leg committed view of an arb operation */
typedef struct {
    pt_flow_id_t   arb_id;
    pt_arb_state_t state;
    pt_size_t      target_size;         /* intended size per leg */
    pt_price_t     yes_limit, no_limit;
    pt_order_id_t  yes_oid, no_oid;
    pt_size_t      yes_filled, no_filled;
    int64_t        yes_notional;        /* sum price*size (scaled) */
    int64_t        no_notional;
    double         designed_net_edge;   /* at detection */
    pt_nsec_t      opened_t;
    pt_nsec_t      leg_a_fill_t;        /* timestamp when Leg A first filled */
    pt_nsec_t      last_update_t;
    double         unhedged_exposure_usd;
    double         realized_pnl;
} pt_arb_op_t;

typedef struct {
    double    max_loss_per_trade;       /* USD */
    double    requote_tolerance;        /* prob points */
    int       allow_requote;            /* 1 = may requote unfilled legs */
    double    max_unhedged_time_ms;     /* Max time to stay unhedged before emergency sweep */
    pt_size_t max_unhedged_size;        /* Max unhedged shares */
    double    hedge_timeout_ms;         /* Timeout for resting hedge limit order */
} pt_arb_mgr_cfg_t;

typedef struct {
    pt_arb_mgr_cfg_t cfg;
    pt_arb_cfg_t     arb_cfg;
    const pt_book_t *yes_book;
    const pt_book_t *no_book;
    pt_arb_op_t      ops[PT_ARBMGR_MAX_OPS];
    pt_flow_id_t     next_arb_id;
} pt_arb_mgr_t;

typedef struct {
    int           action;               /* pt_arb_act_t */
    int           leg;                  /* 0=yes, 1=no */
    pt_size_t     size;                 /* for submit/hedge */
    pt_price_t    price;
    pt_order_id_t new_oid;
    double        realized_pnl;         /* estimated realized pnl on completion */
    double        realized_pnl_pending; /* intermediate realized estimate */
    double        remaining_edge;
    double        max_loss_allowed;
    double        projected_loss;
    int           over_risk;
    int           op_index;
} pt_arb_action_t;

void pt_arb_mgr_init(pt_arb_mgr_t *m, const pt_arb_cfg_t *arb,
                     const pt_arb_mgr_cfg_t *mgr, const pt_book_t *yes,
                     const pt_book_t *no);

void pt_arb_mgr_set_books(pt_arb_mgr_t *m, const pt_book_t *yes,
                          const pt_book_t *no);

pt_flow_id_t pt_arb_mgr_next_id(pt_arb_mgr_t *m);

/* Open an arb operation from an opportunity */
int pt_arb_mgr_open(pt_arb_mgr_t *m, const pt_arb_opp_t *opp, pt_arb_op_t *op_out,
                    pt_order_id_t yes_oid, pt_nsec_t now);

/* Handle fill on a leg */
void pt_arb_mgr_on_fill(pt_arb_mgr_t *m, int op_index, int is_yes,
                        pt_size_t filled_delta, pt_price_t fill_price,
                        pt_nsec_t now, pt_arb_action_t *act);

/* Handle cancel on a leg */
void pt_arb_mgr_on_cancel(pt_arb_mgr_t *m, int op_index, int is_yes,
                          pt_nsec_t now, pt_arb_action_t *act);

/* Periodic manager tick to enforce unhedged timeouts and emergency hedges */
void pt_arb_mgr_tick(pt_arb_mgr_t *m, pt_nsec_t now, pt_arb_action_t *act_out, int *num_acts);

/* Given an op, compute exposure */
void pt_arb_mgr_exposure(const pt_arb_mgr_t *m, int op_index,
                         pt_size_t *excess_yes, pt_size_t *excess_no,
                         pt_size_t *remaining_yes, pt_size_t *remaining_no);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_ARB_MGR_H */
