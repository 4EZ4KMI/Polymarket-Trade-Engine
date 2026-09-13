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

    /* Expiry guard */
    if (time_to_expiry_sec < cfg->time_to_expiry_min_sec ||
        time_to_expiry_sec > cfg->time_to_expiry_max_sec)
        return 0;

    const pt_book_features_t *bk = &poly_yes->book;
    if (!bk->have_book || bk->spread > cfg->max_polymarket_spread)
        return 0;

    /* Construct statistical feature vector */
    pt_feature_vector_t *fv = &out->features;
    fv->btc_ret_10ms  = btc_windows[0].return_pct;
    fv->btc_ret_50ms  = btc_windows[1].return_pct;
    fv->btc_ret_100ms = btc_windows[2].return_pct;
    fv->btc_ret_500ms = btc_windows[4].return_pct;
    fv->btc_ret_1s    = btc_windows[5].return_pct;
    fv->btc_ret_5s    = btc_windows[7].return_pct;
    fv->btc_ret_10s   = btc_windows[8].return_pct;

    fv->btc_velocity    = btc_windows[5].velocity;
    fv->btc_realized_vol= btc_windows[7].max_move_pct;
    fv->yes_bid         = bk->best_bid;
    fv->yes_ask         = bk->best_ask;
    fv->spread          = bk->spread;
    fv->imb_l1          = bk->imb_L1;
    fv->imb_l3          = bk->imb_L3;
    fv->imb_l5          = bk->imb_L5;
    fv->imb_l10         = bk->imb_L10;
    fv->microprice      = bk->microprice;
    fv->depth_l1_shares = bk->ask_vol_L1;
    fv->time_to_expiry_sec = time_to_expiry_sec;

    if (poly_no && poly_no->book.have_book) {
        fv->no_bid = poly_no->book.best_bid;
        fv->no_ask = poly_no->book.best_ask;
    }

    /* Compute Model Probability for YES outcome */
    double pred_yes_prob = pt_model_predict_prob(&cfg->model, fv);
    double mkt_yes_prob = bk->mid;
    out->model_probability = pred_yes_prob;
    out->market_probability = mkt_yes_prob;

    double edge_yes = pred_yes_prob - ((double)bk->best_ask / (double)PT_PRICE_SCALE);
    double pred_no_prob = 1.0 - pred_yes_prob;
    double no_ask_prob = (poly_no && poly_no->book.have_book)
        ? ((double)poly_no->book.best_ask / (double)PT_PRICE_SCALE)
        : (1.0 - ((double)bk->best_bid / (double)PT_PRICE_SCALE));
    double edge_no = pred_no_prob - no_ask_prob;

    int is_yes = 1;
    double best_raw_edge = 0.0;
    pt_price_t target_ask = 0;

    if (edge_yes > edge_no && edge_yes > 0.0) {
        is_yes = 1;
        best_raw_edge = pred_yes_prob - mkt_yes_prob;
        target_ask = bk->best_ask;
    } else if (edge_no > 0.0) {
        is_yes = 0;
        best_raw_edge = pred_no_prob - (1.0 - mkt_yes_prob);
        target_ask = (poly_no && poly_no->book.have_book)
            ? poly_no->book.best_ask
            : (pt_price_t)((1.0 - (double)bk->best_bid / (double)PT_PRICE_SCALE) * (double)PT_PRICE_SCALE);
    } else {
        return 0; /* No statistical directional edge */
    }

    /* Deduct execution frictions (taker fees, slippage, half-spread) */
    double fee_rate = cfg->taker_fee_bps / 10000.0;
    double slip_rate = cfg->slippage_bps / 10000.0;
    double exec_costs = (bk->spread / 2.0) + fee_rate + slip_rate;
    double exec_edge = best_raw_edge - exec_costs;

    if (exec_edge < cfg->min_expected_edge)
        return 0;

    /* Fill probability estimation */
    pt_fill_params_t fp = {
        .distance_ticks = 0.0,
        .at_best = 1,
        .queue_ahead = (double)bk->ask_vol_L1,
        .order_size = (double)cfg->default_order_size,
        .spread_ticks = bk->spread * (double)PT_PRICE_SCALE,
        .velocity_pct_per_sec = fv->btc_velocity,
        .time_to_expiry_sec = time_to_expiry_sec,
        .hist_fill_rate = 0.0
    };
    double fill_p = pt_fill_prob_default(&fp);

    /* Confidence calculation */
    double mom_conf = clamp01_(fabs(fv->btc_ret_1s) / (cfg->min_btc_momentum_pct > 0 ? cfg->min_btc_momentum_pct * 3.0 : 0.001));
    double imb_conf = clamp01_((fabs(bk->imb_L3) + 0.5) / 1.5);
    double conf = clamp01_(mom_conf * 0.6 + imb_conf * 0.4);

    if (conf < cfg->min_confidence)
        return 0;

    out->has_signal = 1;
    out->dir = PT_DIR_BUY;
    out->is_yes = is_yes;
    out->target_price = target_ask;
    out->raw_edge = best_raw_edge;
    out->executable_edge = exec_edge;
    out->max_size = cfg->default_order_size;
    out->expected_profit = exec_edge * (double)out->max_size;
    out->confidence = conf;
    out->fill_probability = fill_p;
    out->risk_score = clamp01_(bk->spread * 10.0 + (1.0 - fill_p) * 0.5);

    return 1;
}
