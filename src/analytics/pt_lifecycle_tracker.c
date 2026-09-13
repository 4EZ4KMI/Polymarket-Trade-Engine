#include "analytics/pt_lifecycle_tracker.h"
#include "analytics/pt_adverse_selection.h"
#include <string.h>

void pt_lifecycle_tracker_init(pt_lifecycle_tracker_t *t)
{
    if (!t) return;
    memset(t, 0, sizeof(*t));
}

uint64_t pt_lifecycle_on_signal(pt_lifecycle_tracker_t *t,
                                uint64_t signal_id,
                                pt_nsec_t signal_time,
                                pt_market_id_t market_id,
                                int strategy,
                                int is_yes,
                                int side,
                                double theoretical_edge,
                                double executable_edge,
                                double expected_edge,
                                double fill_prob,
                                pt_size_t requested_size,
                                double expected_pnl)
{
    if (!t || t->count >= PT_LIFECYCLE_MAX_RECORDS) return 0;
    pt_opportunity_record_t *r = &t->records[t->count++];
    memset(r, 0, sizeof(*r));
    r->signal_id = signal_id;
    r->signal_time = signal_time;
    r->market_id = market_id;
    r->strategy = strategy;
    r->is_yes = is_yes;
    r->side = side;
    r->state = PT_LC_STATE_PENDING;
    r->arb_status = 1; /* PENDING */
    
    r->leg_a.is_yes = is_yes;
    r->leg_a.side = side;
    r->leg_a.requested_size = requested_size;
    
    if (strategy == PT_STRAT_PARITY5M) {
        r->leg_b.is_yes = 0; /* NO leg */
        r->leg_b.side = side;
        r->leg_b.requested_size = requested_size;
    }

    r->theoretical_edge = theoretical_edge;
    r->executable_edge = executable_edge;
    r->expected_edge = expected_edge;
    r->fill_probability = fill_prob;
    r->requested_size = requested_size;
    r->expected_pnl = expected_pnl;
    return signal_id;
}

void pt_lifecycle_on_order(pt_lifecycle_tracker_t *t, uint64_t signal_id,
                           pt_order_id_t oid, double quoted_edge)
{
    if (!t) return;
    for (uint64_t i = 0; i < t->count; i++) {
        if (t->records[i].signal_id == signal_id) {
            t->records[i].order_id = oid;
            t->records[i].leg_a.order_id = oid;
            t->records[i].quoted_edge = quoted_edge;
            t->records[i].arb_status = 1; /* PENDING */
            break;
        }
    }
}

void pt_lifecycle_on_arb_orders(pt_lifecycle_tracker_t *t, uint64_t signal_id,
                                pt_order_id_t yes_oid, pt_order_id_t no_oid,
                                double quoted_edge)
{
    if (!t) return;
    for (uint64_t i = 0; i < t->count; i++) {
        if (t->records[i].signal_id == signal_id) {
            t->records[i].order_id = yes_oid;
            t->records[i].leg_a.order_id = yes_oid;
            t->records[i].leg_b_order_id = no_oid;
            t->records[i].leg_b.order_id = no_oid;
            t->records[i].quoted_edge = quoted_edge;
            t->records[i].arb_status = 1; /* PENDING */
            break;
        }
    }
}

void pt_lifecycle_on_fill_ex(pt_lifecycle_tracker_t *t, uint64_t signal_id,
                             pt_order_id_t order_id, int is_yes,
                             pt_size_t fill_qty, pt_price_t fill_price,
                             double filled_edge, double latency_ms,
                             double fees, double rebates, double slippage,
                             uint64_t fill_id)
{
    if (!t || fill_qty == 0) return;

    for (uint64_t i = 0; i < t->count; i++) {
        pt_opportunity_record_t *r = &t->records[i];
        int matches = 0;
        if (signal_id > 0 && r->signal_id == signal_id) {
            matches = 1;
        } else if (order_id > 0 && (r->leg_a.order_id == order_id || r->leg_b.order_id == order_id)) {
            matches = 1;
        }
        if (!matches) continue;

        pt_lc_leg_t *target = &r->leg_a;
        if (r->strategy == PT_STRAT_PARITY5M) {
            if (order_id > 0 && r->leg_b.order_id == order_id) {
                target = &r->leg_b;
            } else if (is_yes == 0 && r->leg_b.requested_size > 0 && (order_id == 0 || r->leg_a.order_id != order_id)) {
                target = &r->leg_b;
            }
        }

        target->filled_size += fill_qty;
        target->fill_count++;
        target->cum_cost_scaled += (int64_t)fill_qty * (int64_t)fill_price;
        if (target->filled_size > 0) {
            target->avg_fill_price = (pt_price_t)(target->cum_cost_scaled / target->filled_size);
        }
        target->fees += fees;
        target->rebates += rebates;
        target->slippage += slippage;
        target->latency_ms = latency_ms;
        target->last_fill_id = fill_id;

        /* Update State Machine */
        if (r->strategy == PT_STRAT_PARITY5M) {
            if (r->leg_a.filled_size >= r->leg_a.requested_size &&
                r->leg_b.filled_size >= r->leg_b.requested_size &&
                r->leg_a.requested_size > 0 && r->leg_b.requested_size > 0) {
                r->state = PT_LC_STATE_BOTH_LEGS_FILLED;
                r->arb_status = 2; /* BOTH_FILLED */
            } else if ((r->leg_a.filled_size >= r->leg_a.requested_size && r->leg_b.filled_size == 0) ||
                       (r->leg_b.filled_size >= r->leg_b.requested_size && r->leg_a.filled_size == 0)) {
                r->state = PT_LC_STATE_ONE_LEG_FILLED;
                r->arb_status = 1;
            } else {
                r->state = PT_LC_STATE_PARTIAL;
                r->arb_status = 1;
            }
        } else {
            if (r->leg_a.filled_size >= r->leg_a.requested_size && r->leg_a.requested_size > 0) {
                r->state = PT_LC_STATE_ONE_LEG_FILLED;
            } else {
                r->state = PT_LC_STATE_PARTIAL;
            }
        }

        /* Update Legacy / Convenience fields */
        r->filled_size = r->leg_a.filled_size;
        r->leg_b_filled_size = r->leg_b.filled_size;
        if (target == &r->leg_a) {
            r->fill_id = fill_id;
        } else {
            r->leg_b_fill_id = fill_id;
        }
        r->filled_edge = filled_edge;
        r->latency_ms = latency_ms;
        r->fees = r->leg_a.fees + r->leg_b.fees;
        r->rebates = r->leg_a.rebates + r->leg_b.rebates;
        r->slippage = r->leg_a.slippage + r->leg_b.slippage;

        pt_size_t total_req = r->leg_a.requested_size + r->leg_b.requested_size;
        pt_size_t total_fill = r->leg_a.filled_size + r->leg_b.filled_size;
        if (total_req > 0) {
            r->fill_ratio = (double)total_fill / (double)total_req;
        }
        break;
    }
}

void pt_lifecycle_on_fill(pt_lifecycle_tracker_t *t, uint64_t signal_id,
                          pt_size_t fill_qty, pt_price_t fill_price,
                          double filled_edge, double latency_ms,
                          double fees, double rebates, double slippage,
                          uint64_t fill_id)
{
    pt_lifecycle_on_fill_ex(t, signal_id, 0, -1, fill_qty, fill_price,
                            filled_edge, latency_ms, fees, rebates, slippage, fill_id);
}

void pt_lifecycle_on_hedge(pt_lifecycle_tracker_t *t, uint64_t signal_id, double hedge_cost)
{
    if (!t) return;
    for (uint64_t i = 0; i < t->count; i++) {
        pt_opportunity_record_t *r = &t->records[i];
        if (r->signal_id == signal_id) {
            r->hedge_cost = hedge_cost;
            r->hedge_executed = 1;
            r->state = PT_LC_STATE_HEDGED;
            r->arb_status = 3; /* HEDGED */
            break;
        }
    }
}

void pt_lifecycle_on_complete(pt_lifecycle_tracker_t *t, uint64_t signal_id,
                              double realized_pnl, double hedge_cost,
                              double realized_edge)
{
    if (!t) return;
    for (uint64_t i = 0; i < t->count; i++) {
        pt_opportunity_record_t *r = &t->records[i];
        if (r->signal_id == signal_id && !r->completed) {
            r->realized_pnl = realized_pnl;
            if (hedge_cost > 0.0 || r->hedge_cost == 0.0) {
                r->hedge_cost = hedge_cost;
                if (hedge_cost > 0.0) r->hedge_executed = 1;
            }
            r->realized_edge = realized_edge;
            r->pnl_difference = realized_pnl - r->expected_pnl;
            r->completed = 1;
            r->state = PT_LC_STATE_SETTLED;
            r->arb_status = 4; /* SETTLED */
            t->completed_count++;
            break;
        }
    }
}

void pt_lifecycle_on_complete_by_market(pt_lifecycle_tracker_t *t, pt_market_id_t market_id,
                                        int strategy, double realized_pnl,
                                        double hedge_cost, double realized_edge)
{
    if (!t) return;
    for (uint64_t i = 0; i < t->count; i++) {
        pt_opportunity_record_t *r = &t->records[i];
        if (r->market_id == market_id && r->strategy == strategy && !r->completed) {
            pt_lifecycle_on_complete(t, r->signal_id, realized_pnl, hedge_cost, realized_edge);
            break;
        }
    }
}

void pt_lifecycle_compute_stats(pt_lifecycle_tracker_t *t, const pt_adverse_tracker_t *adv)
{
    if (!t || t->completed_count == 0) return;
    double sum_theo = 0.0, sum_exec = 0.0, sum_real = 0.0;
    double sum_adv = 0.0, sum_slip = 0.0;
    double sum_exp_pnl = 0.0, sum_real_pnl = 0.0;

    for (uint64_t i = 0; i < t->count; i++) {
        pt_opportunity_record_t *r = &t->records[i];
        if (!r->completed) continue;
        if (adv && r->fill_id > 0) {
            r->adverse_selection = pt_adverse_get_fill_bps(adv, r->fill_id);
        }
        sum_theo += r->theoretical_edge;
        sum_exec += r->executable_edge;
        sum_real += r->realized_edge;
        sum_adv  += r->adverse_selection;
        sum_slip += r->slippage;
        sum_exp_pnl += r->expected_pnl;
        sum_real_pnl += r->realized_pnl;
    }

    double n = (double)t->completed_count;
    t->avg_theoretical_edge = sum_theo / n;
    t->avg_executable_edge = sum_exec / n;
    t->avg_realized_edge = sum_real / n;
    t->avg_adverse_selection = sum_adv / n;
    t->avg_slippage = sum_slip / n;
    t->total_expected_pnl = sum_exp_pnl;
    t->total_realized_pnl = sum_real_pnl;
}

const char *pt_lc_state_to_str(pt_lc_state_t st)
{
    switch (st) {
        case PT_LC_STATE_PENDING: return "PENDING";
        case PT_LC_STATE_PARTIAL: return "PARTIAL";
        case PT_LC_STATE_ONE_LEG_FILLED: return "ONE_LEG_FILLED";
        case PT_LC_STATE_BOTH_LEGS_FILLED: return "BOTH_LEGS_FILLED";
        case PT_LC_STATE_HEDGED: return "HEDGED";
        case PT_LC_STATE_SETTLED: return "SETTLED";
        case PT_LC_STATE_CANCELLED: return "CANCELLED";
        case PT_LC_STATE_EXPIRED_UNRESOLVED: return "EXPIRED_UNRESOLVED";
        default: return "UNKNOWN";
    }
}
