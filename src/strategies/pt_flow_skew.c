#include "strategies/pt_flow_skew.h"
#include "execution/pt_fill_prob.h"
#include <math.h>
#include <string.h>

static double clamp01_(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }

int pt_flow_skew_eval(const pt_flow_skew_cfg_t *cfg,
                      const pt_market_features_t *poly_yes,
                      const pt_market_features_t *poly_no,
                      const pt_btc_window_t *btc_windows,
                      double btc_mid,
                      double time_to_expiry_sec,
                      pt_flow_signal_t *out)
{
    if (out == NULL) return -1;
    memset(out, 0, sizeof(*out));
    if (cfg == NULL || poly_yes == NULL || btc_windows == NULL)
        return -1;

    /* expiry guard */
    if (time_to_expiry_sec < cfg->time_to_expiry_min_sec ||
        time_to_expiry_sec > cfg->time_to_expiry_max_sec)
        return 0;

    const pt_book_features_t *bk = &poly_yes->book;
    if (!bk->have_book || bk->spread > cfg->max_polymarket_spread)
        return 0; /* wide or empty market */

    /* short-term BTC return: index 5 = 1s, index 7 = 5s in PT_BTC_TFNS */
    double ret_1s = btc_windows[5].return_pct;
    double ret_5s = btc_windows[7].return_pct;
    double vel_1s = btc_windows[5].velocity;

    out->btc_mom_1s = ret_1s; out->btc_mom_5s = ret_5s;
    out->poly_imb_l3 = bk->imb_L3; out->poly_spread = bk->spread;

    /* direction detection: BTC micro-move */
    int bullish = (ret_1s >= cfg->min_btc_momentum_pct &&
                   vel_1s >= cfg->min_btc_velocity &&
                   bk->imb_L3 >= -0.05); /* Polymarket not already slammed bid */
    int bearish = (ret_1s <= -cfg->min_btc_momentum_pct &&
                   vel_1s >= cfg->min_btc_velocity &&
                   bk->imb_L3 <= 0.05);

    if (!bullish && !bearish)
        return 0;

    int is_yes = bullish ? 1 : 0;
    pt_price_t target_ask = bullish ? bk->best_ask : (poly_no ? poly_no->book.best_ask : 0);
    if (target_ask == 0)
        return 0;

    /* expected edge scales with momentum strength minus spread friction */
    double mom_mag = fabs(ret_1s);
    double raw_edge = mom_mag * 0.05; /* calibration: 0.10% btc move -> 0.005 prob edge */
    double net_edge = raw_edge - (bk->spread / 2.0);

    if (net_edge < cfg->min_expected_edge)
        return 0;

    /* fill probability estimate */
    pt_fill_params_t fp = {
        .distance_ticks = 0.0,
        .at_best = 1,
        .queue_ahead = (double)bk->ask_vol_L1,
        .order_size = (double)cfg->default_order_size,
        .spread_ticks = bk->spread * (double)PT_PRICE_SCALE,
        .velocity_pct_per_sec = vel_1s,
        .time_to_expiry_sec = time_to_expiry_sec,
        .hist_fill_rate = 0.0
    };
    double fill_p = pt_fill_prob_default(&fp);

    /* confidence combines momentum magnitude, L3 agreement, and queue size */
    double mom_conf = clamp01_(mom_mag / (cfg->min_btc_momentum_pct * 3.0));
    double imb_conf = clamp01_((fabs(bk->imb_L3) + 0.5) / 1.5);
    double conf = clamp01_(mom_conf * 0.6 + imb_conf * 0.4);

    if (conf < cfg->min_confidence)
        return 0;

    out->has_signal = 1;
    out->dir = PT_DIR_BUY;
    out->is_yes = is_yes;
    out->target_price = target_ask;
    out->expected_edge = net_edge;
    out->max_size = cfg->default_order_size;
    out->expected_profit = net_edge * (double)out->max_size;
    out->confidence = conf;
    out->fill_probability = fill_p;
    /* risk score: 0 (safe) to 1 (risky) */
    out->risk_score = clamp01_(bk->spread * 10.0 + (1.0 - fill_p) * 0.5);
    return 1;
}