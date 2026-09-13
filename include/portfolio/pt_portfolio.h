#ifndef PMT_PT_PORTFOLIO_H
#define PMT_PT_PORTFOLIO_H

#include "core/ptypes.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct pt_strategy_stats_tracker_s pt_strategy_stats_tracker_t;
typedef struct pt_lifecycle_tracker_s pt_lifecycle_tracker_t;

#define PT_PORTFOLIO_MAX_POSITIONS 128

typedef struct {
    pt_market_id_t market_id;
    int            is_yes;
    int            strategy;            /* PT_STRAT_PARITY5M vs PT_STRAT_FLOW15M */
    uint64_t       signal_id;           /* Exact opportunity signal attribution */
    pt_size_t      shares;
    int64_t        cost_basis_scaled;   /* total scaled dollars paid */
    pt_price_t     current_bid_price;   /* for unrealized pnl */
    double         unrealized_pnl;
    double         realized_pnl;
    int            is_settled;          /* 1 if position has been settled */
} pt_position_t;

typedef struct {
    double        initial_cash;
    double        cash;
    double        realized_pnl;
    double        unrealized_pnl;
    double        total_exposure;       /* current notional in open positions */
    uint64_t      trades_count;
    uint64_t      wins_count;
    uint64_t      losses_count;
    int           position_count;
    pt_position_t positions[PT_PORTFOLIO_MAX_POSITIONS];
} pt_portfolio_t;

void pt_portfolio_init(pt_portfolio_t *p, double initial_cash);

/* Record an execution fill with strategy attribution */
void pt_portfolio_on_fill(pt_portfolio_t *p, pt_market_id_t market_id,
                          int is_yes, int side, pt_size_t shares,
                          pt_price_t price_scaled, int strategy);

void pt_portfolio_on_fill_ex(pt_portfolio_t *p, pt_market_id_t market_id,
                             int is_yes, int side, pt_size_t shares,
                             pt_price_t price_scaled, int strategy,
                             uint64_t signal_id);

/* Update mark-to-market valuations with current bids. */
void pt_portfolio_mark(pt_portfolio_t *p, pt_market_id_t market_id,
                       int is_yes, pt_price_t current_bid);

/* Total portfolio equity = cash + mark-to-market positions. */
double pt_portfolio_equity(const pt_portfolio_t *p);
double pt_portfolio_win_rate(const pt_portfolio_t *p);
double pt_portfolio_market_exposure(const pt_portfolio_t *p, pt_market_id_t market_id);

/* Settle all positions for a market at expiry: winning_is_yes = 1 (YES wins @ $1), 0 (NO wins @ $1)
   Isolates settlements by strategy and records outcomes directly to strategy stats and lifecycle */
void pt_portfolio_settle_market(pt_portfolio_t *p, pt_market_id_t market_id, int winning_is_yes,
                                pt_strategy_stats_tracker_t *stats, pt_lifecycle_tracker_t *lc);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_PORTFOLIO_H */
