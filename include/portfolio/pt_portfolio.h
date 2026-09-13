#ifndef PMT_PT_PORTFOLIO_H
#define PMT_PT_PORTFOLIO_H

#include "core/ptypes.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PT_PORTFOLIO_MAX_POSITIONS 128

typedef struct {
    pt_market_id_t market_id;
    int            is_yes;
    pt_size_t      shares;
    int64_t        cost_basis_scaled;   /* total scaled dollars paid */
    pt_price_t     current_bid_price;   /* for unrealized pnl */
    double         unrealized_pnl;
    double         realized_pnl;
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

/* Record an execution fill. */
void pt_portfolio_on_fill(pt_portfolio_t *p, pt_market_id_t market_id,
                          int is_yes, int side, pt_size_t shares,
                          pt_price_t price_scaled);

/* Update mark-to-market valuations with current bids. */
void pt_portfolio_mark(pt_portfolio_t *p, pt_market_id_t market_id,
                       int is_yes, pt_price_t current_bid);

/* Total portfolio equity = cash + mark-to-market positions. */
double pt_portfolio_equity(const pt_portfolio_t *p);
double pt_portfolio_win_rate(const pt_portfolio_t *p);
double pt_portfolio_market_exposure(const pt_portfolio_t *p, pt_market_id_t market_id);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_PORTFOLIO_H */