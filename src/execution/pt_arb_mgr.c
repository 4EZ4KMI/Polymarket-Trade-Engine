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
    if (arb) m->arb_cfg = *arb;
    if (mgr) {
        m->cfg = *mgr;
    } else {
        m->cfg.max_loss_per_trade = 10.0;
        m->cfg.requote_tolerance = 0.005;
        m->cfg.allow_requote = 1;
        m->cfg.max_unhedged_time_ms = 500.0;
        m->cfg.max_unhedged_size = 500;
        m->cfg.hedge_timeout_ms = 1000.0;
    }
    m->yes_book = yes;
    m->no_book  = no;
    m->next_arb_id = 1;
}

void pt_arb_mgr_set_books(pt_arb_mgr_t *m, const pt_book_t *yes,
                          const pt_book_t *no)
{
    if (m) {
        m->yes_book = yes;
        m->no_book = no;
    }
}

pt_flow_id_t pt_arb_mgr_next_id(pt_arb_mgr_t *m)
{
    return m ? m->next_arb_id++ : 1;
}

int pt_arb_mgr_open(pt_arb_mgr_t *m, const pt_arb_opp_t *opp, pt_arb_op_t *op_out,
                    pt_order_id_t yes_oid, pt_nsec_t now)
{
    if (m == NULL || opp == NULL || !opp->has_opportunity)
        return -1;
    int free_slot = -1;
    for (int i = 0; i < PT_ARBMGR_MAX_OPS; i++) {
        if (m->ops[i].state == PT_ARB_ST_IDLE ||
            m->ops[i].state == PT_ARB_ST_BOTH_FILLED ||
            m->ops[i].state == PT_ARB_ST_HEDGED ||
            m->ops[i].state == PT_ARB_ST_CANCELLED ||
            m->ops[i].state == PT_ARB_ST_EXPIRED ||
            m->ops[i].state == PT_ARB_ST_FAILED ||
            m->ops[i].arb_id == 0) {
            free_slot = i;
            break;
        }
    }
    if (free_slot < 0) return -1;

    pt_arb_op_t *op = &m->ops[free_slot];
    memset(op, 0, sizeof(*op));
    op->arb_id = pt_arb_mgr_next_id(m);
    op->state = PT_ARB_ST_LEG_A_PENDING;
    op->target_size = opp->max_executable_size;
    op->yes_limit = (pt_price_t)opp->avg_yes_scaled;
    op->no_limit  = (pt_price_t)opp->avg_no_scaled;
    op->yes_oid = yes_oid;
    op->no_oid  = 0;
    op->designed_net_edge = opp->net_edge;
    op->opened_t = now;
    op->last_update_t = now;
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

static pt_price_t cur_ask_(const pt_book_t *b)
{
    pt_price_t p = 0;
    if (b && pt_book_best_ask(b, &p, NULL) == 0)
        return p;
    return 0;
}

static pt_price_t cur_bid_(const pt_book_t *b)
{
    pt_price_t p = 0;
    if (b && pt_book_best_bid(b, &p, NULL) == 0)
        return p;
    return 0;
}
void pt_arb_mgr_on_fill(pt_arb_mgr_t *m, int op_index, int is_yes,
                        pt_size_t filled_delta, pt_price_t fill_price,
                        pt_nsec_t now, pt_arb_action_t *act)
{
    clear_act_(act);
    if (m == NULL || act == NULL) return;
    if (op_index < 0 || op_index >= PT_ARBMGR_MAX_OPS) return;

    pt_arb_op_t *op = &m->ops[op_index];
    if (op->arb_id == 0) return;

    if (is_yes) {
        op->yes_filled += filled_delta;
        op->yes_notional += (int64_t)filled_delta * (int64_t)fill_price;
        if (op->leg_a_fill_t == 0) op->leg_a_fill_t = now;
    } else {
        op->no_filled += filled_delta;
        op->no_notional += (int64_t)filled_delta * (int64_t)fill_price;
    }
    op->last_update_t = now;

    pt_size_t excess_yes, excess_no, rem_yes, rem_no;
    pt_arb_mgr_exposure(m, op_index, &excess_yes, &excess_no, &rem_yes, &rem_no);

    if (op->yes_filled >= op->target_size && op->no_filled >= op->target_size) {
        op->state = PT_ARB_ST_BOTH_FILLED;
        act->action = PT_ARB_ACT_COMPLETE;
        act->op_index = op_index;

        double ay = (double)op->yes_notional / (double)op->target_size;
        double an = (double)op->no_notional / (double)op->target_size;
        double fees = pt_economics_fee_usd(&m->arb_cfg.fees, 1, (pt_price_t)ay, op->target_size) +
                      pt_economics_fee_usd(&m->arb_cfg.fees, 1, (pt_price_t)an, op->target_size);
        act->realized_pnl = (1.0 - (ay + an) / (double)PT_PRICE_SCALE) * (double)op->target_size - fees;
        op->realized_pnl = act->realized_pnl;
        return;
    }

    int leading_yes = (op->yes_filled > op->no_filled);
    int has_excess = (excess_yes > 0 || excess_no > 0);
    pt_price_t lag_ask = leading_yes ? cur_ask_(m->no_book) : cur_ask_(m->yes_book);
    pt_price_t lead_bid = leading_yes ? cur_bid_(m->yes_book) : cur_bid_(m->no_book);

    double net_now = 0.0;
    pt_size_t eval = (op->target_size > 1) ? op->target_size : 1;
    if (!pt_arb_net_at(m->yes_book, m->no_book, &m->arb_cfg, eval, &net_now, NULL, NULL))
        net_now = -1.0;

    if (has_excess) {
        pt_size_t hedge_sz = leading_yes ? excess_yes : excess_no;
        double designed_lag = leading_yes ? (double)op->no_limit : (double)op->yes_limit;
        double proj_loss = (double)hedge_sz * ((double)lag_ask - designed_lag) / (double)PT_PRICE_SCALE;
        proj_loss += pt_economics_fee_usd(&m->arb_cfg.fees, 1, lag_ask, hedge_sz);

        act->projected_loss = (proj_loss > 0) ? proj_loss : 0.0;
        act->max_loss_allowed = m->cfg.max_loss_per_trade;
        act->over_risk = (act->projected_loss >= m->cfg.max_loss_per_trade);

        if (act->over_risk) {
            op->state = PT_ARB_ST_HEDGE_REQUIRED;
            act->action = PT_ARB_ACT_MARKET_HEDGE;
            act->leg = leading_yes ? 0 : 1;
            act->size = hedge_sz;
            act->price = lead_bid;
            act->op_index = op_index;
            return;
        }

        if (net_now > m->arb_cfg.min_edge_pct && m->cfg.allow_requote) {
            op->state = leading_yes ? PT_ARB_ST_LEG_B_PENDING : PT_ARB_ST_LEG_A_PENDING;
            set_submit_(act, leading_yes ? 1 : 0, hedge_sz, lag_ask, net_now, op_index);
            return;
        } else {
            op->state = PT_ARB_ST_HEDGE_REQUIRED;
            act->action = PT_ARB_ACT_MARKET_HEDGE;
            act->leg = leading_yes ? 0 : 1;
            act->size = hedge_sz;
            act->price = lead_bid;
            act->op_index = op_index;
            return;
        }
    }

    op->state = PT_ARB_ST_LEG_A_PARTIAL;
    act->action = PT_ARB_ACT_NONE;
    act->remaining_edge = net_now;
    act->op_index = op_index;
}



void pt_arb_mgr_on_cancel(pt_arb_mgr_t *m, int op_index, int is_yes,
                          pt_nsec_t now, pt_arb_action_t *act)
{
    clear_act_(act);
    if (m == NULL || act == NULL) return;
    if (op_index < 0 || op_index >= PT_ARBMGR_MAX_OPS) return;

    pt_arb_op_t *op = &m->ops[op_index];
    pt_size_t excess_yes, excess_no, rem_yes, rem_no;
    pt_arb_mgr_exposure(m, op_index, &excess_yes, &excess_no, &rem_yes, &rem_no);
    (void)rem_yes; (void)rem_no;

    if (op->yes_filled == 0 && op->no_filled == 0) {
        op->state = PT_ARB_ST_CANCELLED;
        act->action = PT_ARB_ACT_ABORT;
        act->op_index = op_index;
        return;
    }

    if (excess_yes > 0) {
        op->state = PT_ARB_ST_HEDGE_REQUIRED;
        act->action = PT_ARB_ACT_MARKET_HEDGE;
        act->leg = 0; /* dump YES */
        act->size = excess_yes;
        act->price = cur_bid_(m->yes_book);
        act->op_index = op_index;
    } else if (excess_no > 0) {
        op->state = PT_ARB_ST_HEDGE_REQUIRED;
        act->action = PT_ARB_ACT_MARKET_HEDGE;
        act->leg = 1; /* dump NO */
        act->size = excess_no;
        act->price = cur_bid_(m->no_book);
        act->op_index = op_index;
    } else {
        op->state = PT_ARB_ST_CANCELLED;
        act->action = PT_ARB_ACT_NONE;
        act->op_index = op_index;
    }
}

void pt_arb_mgr_tick(pt_arb_mgr_t *m, pt_nsec_t now, pt_arb_action_t *act_out, int *num_acts)
{
    if (!m || !act_out || !num_acts) return;
    *num_acts = 0;

    pt_nsec_t unhedged_ns = (pt_nsec_t)(m->cfg.max_unhedged_time_ms * 1000000.0);

    for (int i = 0; i < PT_ARBMGR_MAX_OPS; i++) {
        pt_arb_op_t *op = &m->ops[i];
        if (op->state == PT_ARB_ST_IDLE || op->state == PT_ARB_ST_BOTH_FILLED ||
            op->state == PT_ARB_ST_HEDGED || op->state == PT_ARB_ST_CANCELLED ||
            op->state == PT_ARB_ST_EXPIRED || op->state == PT_ARB_ST_FAILED)
            continue;

        pt_size_t ey, en, ry, rn;
        pt_arb_mgr_exposure(m, i, &ey, &en, &ry, &rn);

        if ((ey > 0 || en > 0) && op->leg_a_fill_t > 0) {
            if (now - op->leg_a_fill_t >= unhedged_ns) {
                pt_arb_action_t *a = &act_out[(*num_acts)++];
                clear_act_(a);
                a->action = PT_ARB_ACT_MARKET_HEDGE;
                a->op_index = i;
                if (ey > 0) {
                    a->leg = 0;
                    a->size = ey;
                    a->price = cur_bid_(m->yes_book);
                } else {
                    a->leg = 1;
                    a->size = en;
                    a->price = cur_bid_(m->no_book);
                }
                op->state = PT_ARB_ST_HEDGE_REQUIRED;
                op->last_update_t = now;
            }
        }
    }
}
