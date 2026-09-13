#include "strategies/pt_model.h"
#include <math.h>

static double clamp01_(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }
static double sigmoid_(double z) { return 1.0 / (1.0 + exp(-z)); }

double pt_model_predict_prob(const pt_model_cfg_t *cfg,
                             const pt_feature_vector_t *f)
{
    if (!f) return 0.5;
    pt_model_type_t mtype = cfg ? cfg->type : PT_MODEL_HEURISTIC;

    double mid_market_prob = 0.5;
    if (f->yes_bid > 0 && f->yes_ask > 0) {
        mid_market_prob = ((double)f->yes_bid + (double)f->yes_ask) / (2.0 * (double)PT_PRICE_SCALE);
    }

    if (mtype == PT_MODEL_LOGISTIC && cfg) {
        /* Logistic logit regression over standardized inputs */
        double z = cfg->w0 +
                   cfg->w_ret1s    * (f->btc_ret_1s * 100.0) +
                   cfg->w_ret5s    * (f->btc_ret_5s * 100.0) +
                   cfg->w_imb      * f->imb_l3 +
                   cfg->w_velocity * (f->btc_velocity / 100.0) +
                   cfg->w_expiry   * (f->time_to_expiry_sec / 900.0);
        return clamp01_(sigmoid_(z));
    }
    else if (mtype == PT_MODEL_EMPIRICAL) {
        /* Empirical non-parametric adjustment: shift market mid based on momentum sign and volume */
        double mom = f->btc_ret_1s;
        double sign = (mom > 0) ? 1.0 : (mom < 0 ? -1.0 : 0.0);
        double delta = sign * fmin(fabs(mom) * 25.0, 0.15) * (1.0 + 0.5 * f->imb_l3);
        return clamp01_(mid_market_prob + delta);
    }
    else {
        /* PT_MODEL_HEURISTIC (Baseline) */
        double mom_1s = f->btc_ret_1s;
        double raw_delta = mom_1s * 25.0; /* 0.10% btc move -> 0.025 prob delta */
        if (raw_delta > 0.15) raw_delta = 0.15;
        if (raw_delta < -0.15) raw_delta = -0.15;
        return clamp01_(mid_market_prob + raw_delta);
    }
}
