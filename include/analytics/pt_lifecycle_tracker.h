#ifndef PMT_PT_LIFECYCLE_TRACKER_H
#define PMT_PT_LIFECYCLE_TRACKER_H

#include "core/ptypes.h"
#include "execution/pt_order.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PT_LIFECYCLE_MAX_RECORDS 2048

typedef enum {
    PT_LC_STATE_PENDING = 0,
    PT_LC_STATE_PARTIAL,
    PT_LC_STATE_ONE_LEG_FILLED,
    PT_LC_STATE_BOTH_LEGS_FILLED,
    PT_LC_STATE_HEDGED,
    PT_LC_STATE_SETTLED,
    PT_LC_STATE_CANCELLED,
    PT_LC_STATE_EXPIRED_UNRESOLVED
} pt_lc_state_t;

typedef struct {
    pt_order_id_t  order_id;
    int            is_yes;
    int            side;
    pt_size_t      requested_size;
    pt_size_t      filled_size;
    uint32_t       fill_count;
    int64_t        cum_cost_scaled;
    pt_price_t     avg_fill_price;
    double         fees;
    double         rebates;
    double         slippage;
    double         latency_ms;
    uint64_t       last_fill_id;
} pt_lc_leg_t;

typedef struct {
    uint64_t       signal_id;
    pt_nsec_t      signal_time;
    pt_market_id_t market_id;
    int            strategy;
    int            is_yes;
    int            side;
    pt_lc_state_t  state;
    
    pt_lc_leg_t    leg_a; /* For Strat A: YES leg. For Strat B: primary leg */
    pt_lc_leg_t    leg_b; /* For Strat A: NO leg */
    
    /* Edges at each lifecycle transition */
    double         theoretical_edge; /* Initial model/gross edge */
    double         executable_edge;  /* After visible book walking */
    double         quoted_edge;      /* At order submit price */
    double         expected_edge;    /* After theoretical fees & buffers */
    double         filled_edge;      /* At matching fill price */
    double         realized_edge;    /* Post-settlement/hedging actual outcome */
    
    /* Execution metrics */
    double         latency_ms;
    double         fill_probability;
    double         fill_ratio;
    double         adverse_selection;
    double         hedge_cost;
    int            hedge_executed;
    double         fees;
    double         rebates;
    double         slippage;
    
    /* PnL */
    double         expected_pnl;
    double         realized_pnl;
    double         pnl_difference;   /* realized_pnl - expected_pnl */
    
    pt_order_id_t  order_id;
    pt_order_id_t  leg_b_order_id;
    uint64_t       fill_id;
    uint64_t       leg_b_fill_id;
    pt_size_t      requested_size;
    pt_size_t      filled_size;
    pt_size_t      leg_b_filled_size;
    int            arb_status;
    int            completed;
} pt_opportunity_record_t;

typedef struct pt_lifecycle_tracker_s {
    pt_opportunity_record_t records[PT_LIFECYCLE_MAX_RECORDS];
    uint64_t                count;
    uint64_t                completed_count;
    
    /* Aggregates */
    double                  avg_theoretical_edge;
    double                  avg_executable_edge;
    double                  avg_realized_edge;
    double                  avg_adverse_selection;
    double                  avg_slippage;
    double                  total_expected_pnl;
    double                  total_realized_pnl;
} pt_lifecycle_tracker_t;

void pt_lifecycle_tracker_init(pt_lifecycle_tracker_t *t);

/* Track new signal generation */
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
                                double expected_pnl);

/* Track order submission */
void pt_lifecycle_on_order(pt_lifecycle_tracker_t *t, uint64_t signal_id,
                           pt_order_id_t oid, double quoted_edge);

/* Track dual-leg arbitrage order submissions (YES + NO) */
void pt_lifecycle_on_arb_orders(pt_lifecycle_tracker_t *t, uint64_t signal_id,
                                pt_order_id_t yes_oid, pt_order_id_t no_oid,
                                double quoted_edge);

/* Forward declaration */
typedef struct pt_adverse_tracker_s pt_adverse_tracker_t;

/* Track fill event */
void pt_lifecycle_on_fill(pt_lifecycle_tracker_t *t, uint64_t signal_id,
                          pt_size_t fill_qty, pt_price_t fill_price,
                          double filled_edge, double latency_ms,
                          double fees, double rebates, double slippage,
                          uint64_t fill_id);

void pt_lifecycle_on_fill_ex(pt_lifecycle_tracker_t *t, uint64_t signal_id,
                             pt_order_id_t order_id, int is_yes,
                             pt_size_t fill_qty, pt_price_t fill_price,
                             double filled_edge, double latency_ms,
                             double fees, double rebates, double slippage,
                             uint64_t fill_id);

/* Record executed hedge economics */
void pt_lifecycle_on_hedge(pt_lifecycle_tracker_t *t, uint64_t signal_id, double hedge_cost);

/* Finalize opportunity trade record */
void pt_lifecycle_on_complete(pt_lifecycle_tracker_t *t, uint64_t signal_id,
                              double realized_pnl, double hedge_cost,
                              double realized_edge);

void pt_lifecycle_on_complete_by_market(pt_lifecycle_tracker_t *t, pt_market_id_t market_id,
                                        int strategy, double realized_pnl,
                                        double hedge_cost, double realized_edge);

/* Recompute averages using measured per-fill adverse selection trajectories */
void pt_lifecycle_compute_stats(pt_lifecycle_tracker_t *t, const pt_adverse_tracker_t *adv);

const char *pt_lc_state_to_str(pt_lc_state_t st);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_LIFECYCLE_TRACKER_H */
