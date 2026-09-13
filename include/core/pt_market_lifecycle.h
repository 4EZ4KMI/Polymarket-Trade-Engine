#ifndef PMT_PT_MARKET_LIFECYCLE_H
#define PMT_PT_MARKET_LIFECYCLE_H

#include "core/ptypes.h"
#include "portfolio/pt_portfolio.h"
#include "analytics/pt_strategy_stats.h"
#include "analytics/pt_lifecycle_tracker.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    pt_market_id_t  market_id;
    char            symbol[32];
    double          strike_price;
    pt_nsec_t       open_time_ns;
    pt_nsec_t       expiry_time_ns;
    pt_lifecycle_t  state;
    double          stop_before_expiry_sec; /* cutoff buffer in seconds */
    int             resolved_winning_outcome; /* 1 = YES, 0 = NO */
    double          resolution_price;
} pt_market_info_t;

void pt_market_info_init(pt_market_info_t *m, pt_market_id_t market_id,
                         const char *symbol, double strike,
                         pt_nsec_t open_t, pt_nsec_t expiry_t,
                         double stop_cutoff_sec);

/* Check if new orders are allowed right now */
int  pt_market_can_trade(const pt_market_info_t *m, pt_nsec_t now);

/* Get seconds remaining to expiry */
double pt_market_time_to_expiry_sec(const pt_market_info_t *m, pt_nsec_t now);

/* Advance lifecycle state and settle positions upon resolution */
int  pt_market_lifecycle_tick(pt_market_info_t *m, double current_btc_price,
                              pt_nsec_t now, pt_portfolio_t *portfolio,
                              pt_strategy_stats_tracker_t *stats,
                              pt_lifecycle_tracker_t *lc);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_MARKET_LIFECYCLE_H */
