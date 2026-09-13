#include "execution/pt_arb_mgr.h"
#include <string.h>

static void clear_act_(pt_arb_action_t *a)
{
    if (!a) return;
    memset(a, 0, sizeof(*a));
    a->action = PT_ARB_ACT_NONE;
}

void pt_arb_mgr_init(pt_arb_mgr_t *m, const pt_arb_cfg_t *arb,
                     const pt_arb_mgr_cfg_t *mgr, const pt_book_t *yes,
                     const pt_book_t *no)
{
    memset(m, 0, sizeof(*m));
    if (arb)  m->arb_cfg  = *arb;
    if (mgr)  m->cfg      = *mgr;
    m->yes_book = yes;
    m->no_book  = no;
    m->next_arb_id = 1;
}

void pt_arb_mgr_set_books(pt_arb_mgr_t *m, const pt_book_t *yes,
                          const pt_book_t *no)
{
    m->yes_book = yes; m->no_book = no;
}

pt_flow_id_t pt_arb_mgr_next_id(pt_arb_mgr_t *m)
{
    return m->next_arb_id++;
}

int pt_arb_mgr_open(pt_arb_mgr_t *m, const pt_arb_opp_t *opp, pt_arb_op_t *op_out,
                    pt_order_id_t yes_oid, pt_nsec_t now)
{
    if (m == NULL || opp == NULL || !opp->has_opportunity)
        return -1;
    int free_slot = -1;
    for (int i = 0; i < PT_ARBMGR_MAX_OPS; i++) {
        if (m->ops[i].state == PT_ARB_ST_COMPLETED ||
            m->ops[i].state == PT_ARB_ST_ABORTED || m->ops[i].arb_id == 0) {
            free_slot = i; break;
        }
    }
    if (free_slot < 0)
        return -1;

    pt_arb_op_t *op = &m->ops[free_slot];
    memset(op, 0, sizeof(*op));
    op->arb_id = pt_arb_mgr_next_id(m);
    op->state = PT_ARB_ST_SUBMITTING_Y;
    op->target_size = opp->max_executable_size;
    op->yes_limit = (pt_price_t)opp->avg_yes_scaled;
    op->no_limit  = (pt_price_t)opp->avg_no_scaled;
    op->yes_oid = yes_oid;
    op->no_oid  = 0;
    op->designed_net_edge = opp->net_edge;
    op->opened_t = now;
    if (op_out) *op_out = *op;
    return free_slot;
}

void pt_arb_mgr_exposure(const pt_arb_mgr_t *m, int op_index,
                         pt_size_t *excess_yes, pt_size_t *excess_no,
                         pt_size_t *remaining_yes, pt_size_t *remaining_no)
{
    pt_size_t ey = 0, en = 0, ry = 0, rn = 0;
    if (m && op_index >= 0 && op_index < PT_ARBMGR_MAX_OPS) {
        const pt_arb_op_t *op = &m->ops[op_index];
        pt_size_t common = (op->yes_filled < op->no_filled) ? op->yes_filled : op->no_filled;
        ey = op->yes_filled - common;
        en = op->no_filled  - common;
        ry = (op->target_size > op->yes_filled) ? op->target_size - op->yes_filled : 0;
        rn = (op->target_size > op->no_filled)  ? op->target_size - op->no_filled  : 0;
    }
    if (excess_yes)    *excess_yes    = ey;
    if (excess_no)     *excess_no     = en;
    if (remaining_yes) *remaining_yes = ry;
    if (remaining_no)  *remaining_no  = rn;
}

static void set_submit_(pt_arb_action_t *a, int leg, pt_size_t size,
                        pt_price_t price, double remaining_edge, int opidx)
{
    a->action = PT_ARB_ACT_SUBMIT; a->leg = leg; a->size = size;
    a->price = price; a->remaining_edge = remaining_edge; a->op_index = opidx;
}

static void set_cancel_(pt_arb_action_t *a, int leg, double remaining_edge,
                        int opidx)
{
    a->action = PT_ARB_ACT_CANCEL; a->leg = leg;
    a->remaining_edge = remaining_edge; a->op_index = opidx;
}

static pt_price_t cur_ask_(const pt_book_t *b)
{
    pt_price_t p = 0;
    if (b && pt_book_best_ask(b, &p, NULL) == 0)
        return p;
    return 0;
}

void pt_arb_mgr_on_fill(pt_arb_mgr_t *m, int op_index, int is_yes,
                        pt_size_t filled_delta, pt_price_t fill_price,
                        pt_nsec_t now, pt_arb_action_t *act)
{
    clear_act_(act);
    if (m == NULL || act == NULL) return;
    if (op_index < 0 || op_index >= PT_ARBMGR_MAX_OPS) {
        act->action = PT_ARB_ACT_ABORT; return;
    }
    pt_arb_op_t *op = &m->ops[op_index];
    if (op->state == PT_ARB_ST_COMPLETED || op->state == PT_ARB_ST_ABORTED)
        return;

    /* 1. update the filled leg */
    if (is_yes) {
        op->yes_filled += filled_delta;
        op->yes_notional += (int64_t)fill_price * (int64_t)filled_delta;
    } else {
        op->no_filled += filled_delta;
        op->no_notional += (int64_t)fill_price * (int64_t)filled_delta;
    }

    /* 2. exposure / remaining */
    pt_size_t excess_yes, excess_no, rem_yes, rem_no;
    pt_arb_mgr_exposure(m, op_index, &excess_yes, &excess_no, &rem_yes, &rem_no);
    (void)rem_yes; (void)rem_no;

    if (op->yes_filled >= op->target_size && op->no_filled >= op->target_size) {
        op->state = PT_ARB_ST_COMPLETED;
        act->action = PT_ARB_ACT_COMPLETE; act->op_index = op_index;
        double ay = op->yes_filled ? (double)op->yes_notional / op->yes_filled : 0.0;
        double an = op->no_filled  ? (double)op->no_notional  / op->no_filled  : 0.0;
        double fees = ((ay + an) / (double)PT_PRICE_SCALE) *
                      (m->arb_cfg.taker_fee_bps / 10000.0);
        act->realized_pnl = (1.0 - (ay + an) / (double)PT_PRICE_SCALE - fees) *
                            (double)op->target_size;
        return;
    }

    int leading_yes = (op->yes_filled > op->no_filled);
    int has_excess = (excess_yes > 0 || excess_no > 0);
    pt_price_t lag_price = leading_yes ? cur_ask_(m->no_book) : cur_ask_(m->yes_book);

    double net_now = 0.0;
    pt_size_t eval = (op->target_size > 1) ? op->target_size : 1;
    if (!pt_arb_net_at(m->yes_book, m->no_book, &m->arb_cfg, eval, &net_now, NULL, NULL))
        net_now = -1.0;

    if (has_excess && net_now <= m->arb_cfg.min_edge_pct) {
        /* hedge the naked surplus; lagging leg fill now costs more */
        op->state = PT_ARB_ST_HEDGING;
        set_cancel_(act, leading_yes ? 1 : 0, net_now, op_index);
        double hedge_sz = (double)(leading_yes ? excess_yes : excess_no);
        double designed = leading_yes ? (double)op->no_limit : (double)op->yes_limit;
        double proj = hedge_sz * ((double)lag_price - designed) / (double)PT_PRICE_SCALE;
        proj += hedge_sz * (double)lag_price / (double)PT_PRICE_SCALE *
                (m->arb_cfg.taker_fee_bps / 10000.0);
        act->projected_loss = (proj > 0) ? proj : 0.0;
        act->max_loss_allowed = m->cfg.max_loss_per_trade;
        act->over_risk = (act->projected_loss >= m->cfg.max_loss_per_trade);
        if (act->over_risk) {
            op->state = PT_ARB_ST_ABORTED;
            act->action = PT_ARB_ACT_ABORT;
        }
        return;
    }

    if (has_excess && net_now > m->arb_cfg.min_edge_pct) {
        op->state = PT_ARB_ST_HEDGING;
        pt_size_t hedge = leading_yes ? excess_yes : excess_no;
        if (m->cfg.allow_requote) {
            set_submit_(act, leading_yes ? 1 : 0, hedge, lag_price, net_now, op_index);
            act->realized_pnl_pending = 0.0;
        } else {
            set_cancel_(act, leading_yes ? 1 : 0, net_now, op_index);
        }
        return;
    }

    if (net_now <= m->arb_cfg.min_edge_pct) {
        op->state = PT_ARB_ST_ABORTED;
        act->action = PT_ARB_ACT_ABORT;
        return;
    }
    op->state = PT_ARB_ST_PARTIAL;
    act->action = PT_ARB_ACT_NONE;
    act->remaining_edge = net_now;
    act->op_index = op_index;
    act->realized_pnl_pending = (double)op->yes_filled * (double)op->no_filled == 0
        ? 0.0 : 0.0;
}

void pt_arb_mgr_on_cancel(pt_arb_mgr_t *m, int op_index, int is_yes,
                          pt_nsec_t now, pt_arb_action_t *act)
{
    clear_act_(act);
    if (m == NULL || act == NULL) { act = NULL; return; }
    if (op_index < 0 || op_index >= PT_ARBMGR_MAX_OPS) {
        act->action = PT_ARB_ACT_ABORT; return;
    }
    pt_arb_op_t *op = &m->ops[op_index];
    pt_size_t excess_yes, excess_no, rem_yes, rem_no;
    pt_arb_mgr_exposure(m, op_index, &excess_yes, &excess_no, &rem_yes, &rem_no);
    (void)rem_yes; (void)rem_no;
    if (op->yes_filled == 0 && op->no_filled == 0) {
        op->state = PT_ARB_ST_ABORTED;
        act->action = PT_ARB_ACT_ABORT;
        return;
    }
    if (excess_yes > 0) {
        op->state = PT_ARB_ST_HEDGING;
        set_submit_(act, 1, excess_yes, cur_ask_(m->no_book), -1.0, op_index);
    } else if (excess_no > 0) {
        op->state = PT_ARB_ST_HEDGING;
        set_submit_(act, 0, excess_no, cur_ask_(m->yes_book), -1.0, op_index);
    } else {
        act->action = PT_ARB_ACT_NONE;
        act->op_index = op_index;
    }
}