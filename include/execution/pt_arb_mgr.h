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
    PT_ARB_ST_DETECTED = 0,
    PT_ARB_ST_SUBMITTING_Y,
    PT_ARB_ST_SUBMITTING_N,
    PT_ARB_ST_PARTIAL,
    PT_ARB_ST_HEDGING,
    PT_ARB_ST_COMPLETED,
    PT_ARB_ST_ABORTED
} pt_arb_state_t;

typedef enum {
    PT_ARB_ACT_NONE = 0,
    PT_ARB_ACT_SUBMIT,
    PT_ARB_ACT_CANCEL,
    PT_ARB_ACT_REQUOTE,
    PT_ARB_ACT_FINALIZE_HEDGE, /* close/re-balance naked exposure */
    PT_ARB_ACT_COMPLETE,
    PT_ARB_ACT_ABORT
} pt_arb_act_t;

/* per-leg committed view of an arb operation */
typedef struct {
    pt_flow_id_t  arb_id;
    pt_arb_state_t state;
    pt_size_t     target_size;      /* intended size per leg */
    pt_price_t    yes_limit, no_limit;
    pt_order_id_t yes_oid, no_oid;
    pt_size_t     yes_filled, no_filled;
    int64_t       yes_notional;     /* sum price*size (scaled) */
    int64_t       no_notional;
    double        designed_net_edge; /* at detection */
    pt_nsec_t     opened_t;
} pt_arb_op_t;

typedef struct {
    double max_loss_per_trade;     /* usd */ 
    double requote_tolerance;      /* prob points */
    int    allow_requote;          /* 1 = may requote unfilled legs */
} pt_arb_mgr_cfg_t;

typedef struct {
    pt_arb_mgr_cfg_t cfg;
    pt_arb_cfg_t     arb_cfg;
    const pt_book_t *yes_book;     /* external live books (swappable) */
    const pt_book_t *no_book;
    pt_arb_op_t      ops[PT_ARBMGR_MAX_OPS];
    pt_flow_id_t     next_arb_id;
} pt_arb_mgr_t;

typedef struct {
    int         action;             /* pt_arb_action_t */
    int         leg;                /* 0=yes, 1=no */
    pt_size_t   size;               /* for submit/hedge */
    pt_price_t  price;
    pt_order_id_t new_oid;
    double      realized_pnl;       /* estimated realized pnl on completion */
    double      realized_pnl_pending;/* intermediate realized estimate */
    double      remaining_edge;
    double      max_loss_allowed;   /* current allowed loss */
    double      projected_loss;     /* worst case if we must hedge now */
    int         over_risk;          /* 1 if projected loss exceeds allowance */
    int         op_index;
} pt_arb_action_t;

void pt_arb_mgr_init(pt_arb_mgr_t *m, const pt_arb_cfg_t *arb,
                     const pt_arb_mgr_cfg_t *mgr, const pt_book_t *yes,
                     const pt_book_t *no);

void pt_arb_mgr_set_books(pt_arb_mgr_t *m, const pt_book_t *yes,
                          const pt_book_t *no);

pt_flow_id_t pt_arb_mgr_next_id(pt_arb_mgr_t *m);

/* Create a new arb op from a validated opportunity; sets state SUBMITTING_Y.
 * Returns op index or -1. Places logics to caller via out_leg_yes/no. */
int pt_arb_mgr_open(pt_arb_mgr_t *m, const pt_arb_opp_t *opp, pt_arb_op_t *op_out,
                    pt_order_id_t yes_oid, pt_nsec_t now);

/* Handle a fill on a leg of an op. Deterministic, no I/O: computes the
 * next action (cancel/requote/hedge/finalize) and updates the op. */
void pt_arb_mgr_on_fill(pt_arb_mgr_t *m, int op_index, int is_yes,
                        pt_size_t filled_delta, pt_price_t fill_price,
                        pt_nsec_t now, pt_arb_action_t *act);

/* Handle an order state change (cancelled/rejected) on a leg. */
void pt_arb_mgr_on_cancel(pt_arb_mgr_t *m, int op_index, int is_yes,
                          pt_nsec_t now, pt_arb_action_t *act);

/* Given an op, compute exposure / hedge sizing right now. */
void pt_arb_mgr_exposure(const pt_arb_mgr_t *m, int op_index,
                         pt_size_t *excess_yes, pt_size_t *excess_no,
                         pt_size_t *remaining_yes, pt_size_t *remaining_no);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_ARB_MGR_H */