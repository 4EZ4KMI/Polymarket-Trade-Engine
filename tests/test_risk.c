#include "test_harness.h"
#include "risk/pt_risk.h"

PT_T(risk_daily_loss_trip)
{
    pt_risk_cfg_t cfg = {
        .max_position_per_market = 1000.0,
        .max_total_exposure      = 5000.0,
        .max_daily_loss          = 50.0,
        .max_drawdown            = 100.0,
        .max_polymarket_stale_ns = 0,
        .max_binance_stale_ns    = 0,
        .max_spread_for_entry    = 0.05,
        .max_consecutive_losses  = 3,
        .min_confidence_floor    = 0.50
    };
    pt_risk_engine_t r;
    pt_risk_init(&r, &cfg);

    pt_signal_t sig = {
        .signal_id = 1,
        .target_price = 500,
        .max_size = 100,
        .confidence = 0.80,
        .poly_spread = 0.02
    };

    pt_size_t approved = 0;
    PT_ASSERT(pt_risk_evaluate_signal(&r, &sig, 0.0, 1000ULL, &approved) == PT_REJECT_NONE);
    PT_ASSERT(approved == 100);

    /* Loss occurs: -$60 */
    pt_risk_on_trade_pnl(&r, -60.0);
    PT_ASSERT(pt_risk_evaluate_signal(&r, &sig, 0.0, 2000ULL, &approved) == PT_REJECT_DAILY_LOSS_EXCEEDED);
    PT_ASSERT(pt_risk_is_tripped(&r) == 1);
}

PT_T(risk_consecutive_losses_trip)
{
    pt_risk_cfg_t cfg = {
        .max_position_per_market = 1000.0,
        .max_total_exposure      = 5000.0,
        .max_daily_loss          = 1000.0,
        .max_drawdown            = 1000.0,
        .max_polymarket_stale_ns = 0,
        .max_binance_stale_ns    = 0,
        .max_spread_for_entry    = 0.05,
        .max_consecutive_losses  = 3,
        .min_confidence_floor    = 0.50
    };
    pt_risk_engine_t r;
    pt_risk_init(&r, &cfg);

    pt_risk_on_trade_pnl(&r, -1.0);
    pt_risk_on_trade_pnl(&r, -1.0);
    PT_ASSERT(pt_risk_is_tripped(&r) == 0);

    pt_risk_on_trade_pnl(&r, -1.0); /* 3rd consecutive loss */
    pt_signal_t sig = { .target_price = 500, .max_size = 50, .confidence = 0.7, .poly_spread = 0.01 };
    pt_size_t approved = 0;
    PT_ASSERT(pt_risk_evaluate_signal(&r, &sig, 0.0, 1000ULL, &approved) == PT_REJECT_CONSECUTIVE_LOSSES);
}

PT_T(risk_stale_feeds)
{
    pt_risk_cfg_t cfg = {
        .max_position_per_market = 1000.0,
        .max_total_exposure      = 5000.0,
        .max_daily_loss          = 1000.0,
        .max_drawdown            = 1000.0,
        .max_polymarket_stale_ns = 1000000000ULL, /* 1 second */
        .max_binance_stale_ns    = 1000000000ULL,
        .max_spread_for_entry    = 0.05,
        .max_consecutive_losses  = 0,
        .min_confidence_floor    = 0.50
    };
    pt_risk_engine_t r;
    pt_risk_init(&r, &cfg);
    pt_risk_feed_touch_poly(&r, 1000000000ULL);
    pt_risk_feed_touch_binance(&r, 1000000000ULL);

    pt_signal_t sig = { .target_price = 500, .max_size = 50, .confidence = 0.7, .poly_spread = 0.01 };
    pt_size_t approved = 0;

    /* Current time 5 seconds later without feed touch */
    PT_ASSERT(pt_risk_evaluate_signal(&r, &sig, 0.0, 6000000000ULL, &approved) == PT_REJECT_POLY_FEED_STALE);
}