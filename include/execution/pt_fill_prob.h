#ifndef PMT_PT_FILL_PROB_H
#define PMT_PT_FILL_PROB_H

#ifdef __cplusplus
extern "C" {
#endif

/* Inputs to a fill-probability estimate. */
typedef struct {
    double distance_ticks;      /* ticks away from best (0 = at best) */
    int    at_best;             /* price == best */
    double queue_ahead;         /* shares ahead of us at this level */
    double order_size;
    double spread_ticks;
    double velocity_pct_per_sec;/* |book mid move| % per second */
    double time_to_expiry_sec;
    double hist_fill_rate;      /* 0..1 historical fill rate (0=unknown) */
} pt_fill_params_t;

/* Tuning for the rule-based model. */
typedef struct {
    double k_distance;   /* decay strength with distance */
    double k_queue;      /* queue sensitivity */
    double k_velocity;   /* velocity boost */
    double k_spread;     /* spread penalty */
    double k_expiry;     /* expiry decay */
    double hist_weight;  /* 0..1 weight given to hist_fill_rate anchor */
} pt_fill_rule_cfg_t;

typedef double (*pt_fill_fn_t)(const void *ctx, const pt_fill_params_t *p);

typedef struct {
    const void  *ctx;   /* opaque context (cfg or ML state) */
    pt_fill_fn_t fn;
} pt_fill_model_t;

double pt_fill_prob_rule_based(const void *ctx, const pt_fill_params_t *p);

/* bind a rule-based model to a cfg (passed as ctx) */
void pt_fill_rule_model(pt_fill_model_t *m, const pt_fill_rule_cfg_t *cfg);

double pt_fill_prob_default(const pt_fill_params_t *p); /* default tuned cfg */

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_FILL_PROB_H */