#include "risk/pt_risk.h"
#include <string.h>
#include <math.h>

void pt_risk_init(pt_risk_engine_t *r, const pt_risk_cfg_t *cfg)
{
    memset(r, 0, sizeof(*r));
    if (cfg) r->cfg = *cfg;
    if (r->cfg.max_orders_per_sec == 0) r->cfg.max_orders_per_sec = 50;
    if (r->cfg.max_order_size_shares == 0.0) r->cfg.max_order_size_shares = 1000.0;
}

void pt_risk_feed_touch_poly(pt_risk_engine_t *r, pt_nsec_t now)
{
    if (r) r->last_poly_update_t = now;
}

void pt_risk_feed_touch_binance(pt_risk_engine_t *r, pt_nsec_t now)
{
    if (r) r->last_binance_update_t = now;
}

void pt_risk_trip_kill(pt_risk_engine_t *r, const char *reason)
{
    if (!r) return;
    r->kill_switch_tripped = 1;
    r->kill_reason = reason ? reason : "manual";
}

void pt_risk_reset_kill(pt_risk_engine_t *r)
{
    if (!r) return;
    r->kill_switch_tripped = 0;
    r->kill_reason = NULL;
}

int pt_risk_is_tripped(const pt_risk_engine_t *r)
{
    return r ? r->kill_switch_tripped : 1;
}

int pt_risk_evaluate_signal(pt_risk_engine_t *r, const pt_signal_t *sig,
                            double market_current_exposure, pt_nsec_t now,
                            pt_size_t *size_approved)
{
    if (r == NULL || sig == NULL || size_approved == NULL)
        return PT_REJECT_INVALID_PRICE;
    *size_approved = 0;
    r->total_evaluated++;

    /* 1. Kill switch */
    if (r->kill_switch_tripped) {
        r->total_rejected++;
        return PT_REJECT_KILL_SWITCH_ACTIVE;
    }

    /* 2. Rate limiting (sliding 1s) */
    if (r->rate_window_start_t == 0 || now >= r->rate_window_start_t + 1000000000ULL) {
        r->rate_window_start_t = now;
        r->orders_in_current_sec = 0;
    }
    if (r->cfg.max_orders_per_sec > 0 && r->orders_in_current_sec >= r->cfg.max_orders_per_sec) {
        r->total_rejected++;
        return PT_REJECT_RATE_LIMIT_EXCEEDED;
    }

    /* 3. Daily loss */
    if (r->daily_pnl <= -fabs(r->cfg.max_daily_loss)) {
        pt_risk_trip_kill(r, "max_daily_loss_exceeded");
        r->total_rejected++;
        return PT_REJECT_DAILY_LOSS_EXCEEDED;
    }

    /* 4. Max drawdown */
    double dd = r->peak_pnl - r->daily_pnl;
    if (dd >= fabs(r->cfg.max_drawdown)) {
        pt_risk_trip_kill(r, "max_drawdown_exceeded");
        r->total_rejected++;
        return PT_REJECT_DRAWDOWN_EXCEEDED;
    }

    /* 5. Consecutive losses */
    if (r->cfg.max_consecutive_losses > 0 &&
        r->consecutive_losses >= r->cfg.max_consecutive_losses) {
        pt_risk_trip_kill(r, "consecutive_losses_exceeded");
        r->total_rejected++;
        return PT_REJECT_CONSECUTIVE_LOSSES;
    }

    /* 6. Stale feeds */
    if (r->cfg.max_polymarket_stale_ns > 0 && r->last_poly_update_t > 0) {
        if (now > r->last_poly_update_t + r->cfg.max_polymarket_stale_ns) {
            r->total_rejected++;
            return PT_REJECT_POLY_FEED_STALE;
        }
    }
    if (r->cfg.max_binance_stale_ns > 0 && r->last_binance_update_t > 0) {
        if (now > r->last_binance_update_t + r->cfg.max_binance_stale_ns) {
            r->total_rejected++;
            return PT_REJECT_BINANCE_FEED_STALE;
        }
    }

    /* 7. Spread */
    if (sig->poly_spread > r->cfg.max_spread_for_entry) {
        r->total_rejected++;
        return PT_REJECT_SPREAD_TOO_WIDE;
    }

    /* 8. Confidence floor */
    if (sig->confidence < r->cfg.min_confidence_floor) {
        r->total_rejected++;
        return PT_REJECT_LOW_CONFIDENCE;
    }

    /* 9. Position sizing */
    double notional_per_share = (double)sig->target_price / (double)PT_PRICE_SCALE;
    if (notional_per_share <= 0.001) {
        r->total_rejected++;
        return PT_REJECT_INVALID_PRICE;
    }

    double room_market = r->cfg.max_position_per_market - market_current_exposure;
    if (room_market <= 0.0) {
        r->total_rejected++;
        return PT_REJECT_POSITION_LIMIT;
    }

    double room_total = r->cfg.max_total_exposure - r->total_exposure;
    if (room_total <= 0.0) {
        r->total_rejected++;
        return PT_REJECT_TOTAL_EXPOSURE_LIMIT;
    }

    double min_room = (room_market < room_total) ? room_market : room_total;
    pt_size_t allowed_shares = (pt_size_t)(min_room / notional_per_share);
    if (r->cfg.max_order_size_shares > 0 && (double)allowed_shares > r->cfg.max_order_size_shares) {
        allowed_shares = (pt_size_t)r->cfg.max_order_size_shares;
    }
    if (allowed_shares == 0) {
        r->total_rejected++;
        return PT_REJECT_TOTAL_EXPOSURE_LIMIT;
    }

    pt_size_t target_sz = (sig->max_size < allowed_shares) ? sig->max_size : allowed_shares;
    if (r->cfg.max_order_size_shares > 0 && (double)target_sz > r->cfg.max_order_size_shares) {
        target_sz = (pt_size_t)r->cfg.max_order_size_shares;
    }

    *size_approved = target_sz;
    r->orders_in_current_sec++;
    return PT_REJECT_NONE;
}


void pt_risk_on_fill(pt_risk_engine_t *r, double notional_usd)
{
    if (r) r->total_exposure += notional_usd;
}

void pt_risk_on_trade_pnl(pt_risk_engine_t *r, double realized_pnl)
{
    if (!r) return;
    r->daily_pnl += realized_pnl;
    if (r->daily_pnl > r->peak_pnl)
        r->peak_pnl = r->daily_pnl;

    if (realized_pnl < 0.0)
        r->consecutive_losses++;
    else if (realized_pnl > 0.0)
        r->consecutive_losses = 0;
}

const char *pt_risk_reject_str(int reject_code)
{
    switch ((pt_risk_reject_t)reject_code) {
    case PT_REJECT_NONE:                 return "approved";
    case PT_REJECT_KILL_SWITCH_ACTIVE:   return "kill_switch_active";
    case PT_REJECT_DAILY_LOSS_EXCEEDED:  return "daily_loss_exceeded";
    case PT_REJECT_DRAWDOWN_EXCEEDED:    return "drawdown_exceeded";
    case PT_REJECT_CONSECUTIVE_LOSSES:   return "consecutive_losses_exceeded";
    case PT_REJECT_POSITION_LIMIT:       return "market_position_limit";
    case PT_REJECT_TOTAL_EXPOSURE_LIMIT: return "total_exposure_limit";
    case PT_REJECT_MAX_ORDER_SIZE:       return "max_order_size_exceeded";
    case PT_REJECT_POLY_FEED_STALE:      return "poly_feed_stale";
    case PT_REJECT_BINANCE_FEED_STALE:   return "binance_feed_stale";
    case PT_REJECT_SPREAD_TOO_WIDE:      return "spread_too_wide";
    case PT_REJECT_LOW_CONFIDENCE:       return "confidence_below_floor";
    case PT_REJECT_RATE_LIMIT_EXCEEDED:  return "rate_limit_orders_per_sec";
    case PT_REJECT_LATENCY_TOO_HIGH:     return "latency_threshold_exceeded";
    case PT_REJECT_INVALID_PRICE:        return "invalid_price";
    }
    return "unknown_reject";
}
