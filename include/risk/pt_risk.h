#ifndef PMT_PT_RISK_H
#define PMT_PT_RISK_H

#include "core/ptypes.h"
#include "strategies/pt_signal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double    max_position_per_market;   /* USD */
    double    max_total_exposure;        /* USD */
    double    max_daily_loss;            /* USD */
    double    max_drawdown;              /* USD from peak */
    pt_nsec_t max_polymarket_stale_ns;   /* max allowed time since last poly book update */
    pt_nsec_t max_binance_stale_ns;      /* max allowed time since last btc tick */
    double    max_spread_for_entry;      /* prob points */
    int       max_consecutive_losses;    /* halts if exceeded */
    double    min_confidence_floor;      /* signals below this rejected */
} pt_risk_cfg_t;

typedef enum {
    PT_REJECT_NONE = 0,
    PT_REJECT_KILL_SWITCH_ACTIVE,
    PT_REJECT_DAILY_LOSS_EXCEEDED,
    PT_REJECT_DRAWDOWN_EXCEEDED,
    PT_REJECT_CONSECUTIVE_LOSSES,
    PT_REJECT_POSITION_LIMIT,
    PT_REJECT_TOTAL_EXPOSURE_LIMIT,
    PT_REJECT_POLY_FEED_STALE,
    PT_REJECT_BINANCE_FEED_STALE,
    PT_REJECT_SPREAD_TOO_WIDE,
    PT_REJECT_LOW_CONFIDENCE,
    PT_REJECT_INVALID_PRICE
} pt_risk_reject_t;

typedef struct {
    pt_risk_cfg_t  cfg;
    int            kill_switch_tripped;
    const char    *kill_reason;
    /* state */
    double         total_exposure;       /* current open exposure USD */
    double         daily_pnl;            /* realized PnL today */
    double         peak_pnl;             /* for drawdown */
    int            consecutive_losses;
    pt_nsec_t      last_poly_update_t;
    pt_nsec_t      last_binance_update_t;
    uint64_t       total_evaluated;
    uint64_t       total_rejected;
} pt_risk_engine_t;

void pt_risk_init(pt_risk_engine_t *r, const pt_risk_cfg_t *cfg);

void pt_risk_feed_touch_poly(pt_risk_engine_t *r, pt_nsec_t now);
void pt_risk_feed_touch_binance(pt_risk_engine_t *r, pt_nsec_t now);

/* Evaluate a signal. Returns PT_REJECT_NONE (0) on approval, else reject reason.
 * *size_approved may be clipped to fit position limits. */
int pt_risk_evaluate_signal(pt_risk_engine_t *r, const pt_signal_t *sig,
                            double market_current_exposure, pt_nsec_t now,
                            pt_size_t *size_approved);

/* Record fill & PnL */
void pt_risk_on_fill(pt_risk_engine_t *r, double notional_usd);
void pt_risk_on_trade_pnl(pt_risk_engine_t *r, double realized_pnl);

/* Manual / auto trip */
void pt_risk_trip_kill(pt_risk_engine_t *r, const char *reason);
void pt_risk_reset_kill(pt_risk_engine_t *r);
int  pt_risk_is_tripped(const pt_risk_engine_t *r);

const char *pt_risk_reject_str(int reject_code);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_RISK_H */