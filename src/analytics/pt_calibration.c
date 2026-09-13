#include "analytics/pt_calibration.h"
#include <string.h>
#include <math.h>

void pt_calibration_init(pt_calibration_tracker_t *c)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
    for (int i = 0; i < PT_CALIB_NUM_BUCKETS; i++) {
        c->buckets[i].bucket_min = 0.50 + (double)i * 0.05;
        c->buckets[i].bucket_max = 0.50 + (double)(i + 1) * 0.05;
    }
}

uint64_t pt_calibration_record_signal(pt_calibration_tracker_t *c,
                                      uint64_t signal_id,
                                      double predicted_prob,
                                      double market_prob,
                                      double time_to_expiry_s)
{
    if (!c) return 0;
    if (c->sample_count >= PT_CALIB_MAX_SAMPLES) return 0;

    pt_calib_sample_t *s = &c->samples[c->sample_count++];
    memset(s, 0, sizeof(*s));
    s->signal_id = signal_id;
    s->predicted_prob = predicted_prob;
    s->market_prob = market_prob;
    s->raw_edge = predicted_prob - market_prob;
    s->time_to_expiry_s = time_to_expiry_s;
    s->settled = 0;
    return signal_id;
}

void pt_calibration_resolve(pt_calibration_tracker_t *c,
                            uint64_t signal_id,
                            double actual_outcome,
                            double realized_pnl)
{
    if (!c) return;
    for (uint64_t i = 0; i < c->sample_count; i++) {
        if (c->samples[i].signal_id == signal_id) {
            if (!c->samples[i].settled) {
                c->samples[i].actual_outcome = actual_outcome;
                c->samples[i].realized_pnl = realized_pnl;
                c->samples[i].settled = 1;
                c->settled_count++;
            }
            break;
        }
    }
}

void pt_calibration_compute(pt_calibration_tracker_t *c)
{
    if (!c || c->settled_count == 0) return;

    /* Reset buckets */
    for (int i = 0; i < PT_CALIB_NUM_BUCKETS; i++) {
        c->buckets[i].count = 0;
        c->buckets[i].sum_predicted_prob = 0.0;
        c->buckets[i].sum_actual_outcome = 0.0;
        c->buckets[i].win_rate = 0.0;
        c->buckets[i].total_pnl = 0.0;
        c->buckets[i].avg_pnl = 0.0;
        c->buckets[i].calibration_error = 0.0;
    }

    double brier_sum = 0.0;
    double log_loss_sum = 0.0;
    const double eps = 1e-12;

    for (uint64_t i = 0; i < c->sample_count; i++) {
        const pt_calib_sample_t *s = &c->samples[i];
        if (!s->settled) continue;

        double p = s->predicted_prob;
        if (p < eps) p = eps;
        if (p > 1.0 - eps) p = 1.0 - eps;
        double o = s->actual_outcome;

        /* Brier score */
        brier_sum += (p - o) * (p - o);

        /* Log loss */
        log_loss_sum -= (o * log(p) + (1.0 - o) * log(1.0 - p));

        /* Bucket assignment based on max(p, 1-p) for directional confidence */
        double conf = (p >= 0.5) ? p : (1.0 - p);
        int bidx = (int)((conf - 0.50) / 0.05);
        if (bidx < 0) bidx = 0;
        if (bidx >= PT_CALIB_NUM_BUCKETS) bidx = PT_CALIB_NUM_BUCKETS - 1;

        pt_prob_bucket_t *b = &c->buckets[bidx];
        b->count++;
        b->sum_predicted_prob += conf;
        double won = (p >= 0.5 && o >= 0.5) || (p < 0.5 && o < 0.5) ? 1.0 : 0.0;
        b->sum_actual_outcome += won;
        b->total_pnl += s->realized_pnl;
    }

    c->brier_score = brier_sum / (double)c->settled_count;
    c->log_loss = log_loss_sum / (double)c->settled_count;

    double weighted_ece = 0.0;
    double max_ce = 0.0;

    for (int i = 0; i < PT_CALIB_NUM_BUCKETS; i++) {
        pt_prob_bucket_t *b = &c->buckets[i];
        if (b->count > 0) {
            double avg_pred = b->sum_predicted_prob / (double)b->count;
            b->win_rate = b->sum_actual_outcome / (double)b->count;
            b->avg_pnl = b->total_pnl / (double)b->count;
            b->calibration_error = fabs(avg_pred - b->win_rate);
            weighted_ece += b->calibration_error * ((double)b->count / (double)c->settled_count);
            if (b->calibration_error > max_ce) {
                max_ce = b->calibration_error;
            }
        }
    }

    c->expected_calibration_error = weighted_ece;
    c->max_calibration_error = max_ce;
}
