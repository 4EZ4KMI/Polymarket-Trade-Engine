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
            t->records[i].leg_b_order_id = no_oid;
            t->records[i].quoted_edge = quoted_edge;
            t->records[i].arb_status = 1; /* PENDING */
            break;
        }
    }
}

void pt_lifecycle_on_fill(pt_lifecycle_tracker_t *t, uint64_t signal_id,
                          pt_size_t fill_qty, pt_price_t fill_price,
                          double filled_edge, double latency_ms,
                          double fees, double rebates, double slippage,
                          uint64_t fill_id)
{
    if (!t) return;
    (void)fill_price;
    for (uint64_t i = 0; i < t->count; i++) {
        pt_opportunity_record_t *r = &t->records[i];
        if (r->signal_id == signal_id) {
            if (r->fill_id == 0) {
                r->fill_id = fill_id;
                r->filled_size += fill_qty;
            } else {
                r->leg_b_fill_id = fill_id;
                r->leg_b_filled_size += fill_qty;
                r->arb_status = 2; /* BOTH_FILLED */
            }
            r->filled_edge = filled_edge;
            r->latency_ms = latency_ms;
            r->fees += fees;
            r->rebates += rebates;
            r->slippage += slippage;
            r->adverse_selection = 0.0; /* Trajectory measured as post-fill price deltas mature */
            if (r->requested_size > 0) {
                r->fill_ratio = (double)r->filled_size / (double)r->requested_size;
            }
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
            r->hedge_cost = hedge_cost;
            r->realized_edge = realized_edge;
            r->pnl_difference = realized_pnl - r->expected_pnl;
            r->completed = 1;
            t->completed_count++;
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
