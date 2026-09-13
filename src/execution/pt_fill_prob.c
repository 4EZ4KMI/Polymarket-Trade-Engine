#include "execution/pt_fill_prob.h"
#include <math.h>
#include <stddef.h>

static double clamp01_(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }

double pt_fill_prob_rule_based(const void *ctx, const pt_fill_params_t *p)
{
    const pt_fill_rule_cfg_t *c =
        (const pt_fill_rule_cfg_t *)(ctx ? ctx : (void *)p); /* cfg may be NULL */
    double kd = (ctx) ? c->k_distance : 0.8;
    double kq = (ctx) ? c->k_queue    : 1.0;
    double kv = (ctx) ? c->k_velocity : 0.15;
    double ks = (ctx) ? c->k_spread   : 0.02;
    double ke = (ctx) ? c->k_expiry   : 0.05;
    double hw = (ctx) ? c->hist_weight: 0.10;

    if (p == NULL || p->time_to_expiry_sec <= 0.0)
        return 0.0;

    double d = p->distance_ticks;
    double q = p->queue_ahead;
    double s = p->order_size;
    double v = fabs(p->velocity_pct_per_sec);
    double sp = p->spread_ticks;
    double tte = p->time_to_expiry_sec;

    /* distance: 1 at best, decays with squared distance */
    double df = 1.0 / (1.0 + kd * d * d);
    /* queue share: our size / (our size + queue ahead) at that level */
    double qf = (s + q > 1e-12) ? (s / (s + q)) : 1.0;
    qf = pow(qf, kq);
    /* velocity: none if book frozen, rises as market moves */
    double vf = 1.0 + kv * v;
    /* wide spread lowers crossing probability */
    double sf = 1.0 / (1.0 + ks * sp);
    /* closer to expiry, less certainty of resting time => dampens */
    double ef = 0.6 + 0.4 * exp(-ke * tte);

    double p_base = clamp01_(df * qf * vf * sf * ef);

    /* blend historical fill rate as an anchor when available */
    if (hw > 0.0) {
        double h = p->hist_fill_rate;
        if (h > 0.0 && h <= 1.0)
            p_base = (1.0 - hw) * p_base + hw * h;
    }
    return clamp01_(p_base);
}

void pt_fill_rule_model(pt_fill_model_t *m, const pt_fill_rule_cfg_t *cfg)
{
    if (m == NULL) return;
    m->ctx = (const void *)cfg;
    m->fn  = pt_fill_prob_rule_based;
}

double pt_fill_prob_default(const pt_fill_params_t *p)
{
    return pt_fill_prob_rule_based(NULL, p);
}