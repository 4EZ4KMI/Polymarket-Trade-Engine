#ifndef PMT_PT_LIFECYCLE_TRACKER_H
#define PMT_PT_LIFECYCLE_TRACKER_H

#include "core/ptypes.h"
#include "execution/pt_order.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PT_LIFECYCLE_MAX_RECORDS 2048

typedef struct {
    uint64_t       signal_id;
    pt_nsec_t      signal_time;
    pt_market_id_t market_id;
    int            strategy;
    int            is_yes;
    int            side;
    
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
    double         fees;
    double         rebates;
    double         slippage;
    
    /* PnL */
    double         expected_pnl;
    double         realized_pnl;
    double         pnl_difference;   /* realized_pnl - expected_pnl */
    
    pt_order_id_t  order_id;
    uint64_t       fill_id;
    pt_size_t      requested_size;
    pt_size_t      filled_size;
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

/* Forward declaration */
typedef struct pt_adverse_tracker_s pt_adverse_tracker_t;

/* Track fill event */
void pt_lifecycle_on_fill(pt_lifecycle_tracker_t *t, uint64_t signal_id,
                          pt_size_t fill_qty, pt_price_t fill_price,
                          double filled_edge, double latency_ms,
                          double fees, double rebates, double slippage,
                          uint64_t fill_id);

/* Finalize opportunity trade record */
void pt_lifecycle_on_complete(pt_lifecycle_tracker_t *t, uint64_t signal_id,
                              double realized_pnl, double hedge_cost,
                              double realized_edge);

/* Recompute averages using measured per-fill adverse selection trajectories */
void pt_lifecycle_compute_stats(pt_lifecycle_tracker_t *t, const pt_adverse_tracker_t *adv);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_LIFECYCLE_TRACKER_H */
