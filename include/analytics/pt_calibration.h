#ifndef PMT_PT_CALIBRATION_H
#define PMT_PT_CALIBRATION_H

#include "core/ptypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PT_CALIB_NUM_BUCKETS 10
#define PT_CALIB_MAX_SAMPLES 2048

typedef struct {
    uint64_t signal_id;
    double   predicted_prob;   /* Model predicted probability [0..1] */
    double   market_prob;      /* Implied probability at signal time */
    double   raw_edge;         /* predicted_prob - market_prob */
    double   actual_outcome;   /* 1.0 if YES settled win, 0.0 if NO settled win */
    double   time_to_expiry_s;
    double   realized_pnl;
    int      settled;          /* 1 if outcome is known */
} pt_calib_sample_t;

typedef struct {
    double   bucket_min;
    double   bucket_max;
    uint64_t count;
    double   sum_predicted_prob;
    double   sum_actual_outcome;
    double   win_rate;
    double   total_pnl;
    double   avg_pnl;
    double   calibration_error; /* |avg_pred - actual_win_rate| */
} pt_prob_bucket_t;

typedef struct {
    pt_calib_sample_t samples[PT_CALIB_MAX_SAMPLES];
    uint64_t          sample_count;
    uint64_t          settled_count;
    
    /* Aggregate calibration metrics */
    double            brier_score;
    double            log_loss;
    double            expected_calibration_error; /* ECE */
    double            max_calibration_error;      /* MCE */
    
    pt_prob_bucket_t  buckets[PT_CALIB_NUM_BUCKETS];
} pt_calibration_tracker_t;

void pt_calibration_init(pt_calibration_tracker_t *c);

/* Record a new prediction signal */
uint64_t pt_calibration_record_signal(pt_calibration_tracker_t *c,
                                      uint64_t signal_id,
                                      double predicted_prob,
                                      double market_prob,
                                      double time_to_expiry_s);

/* Resolve a signal with its actual settlement outcome and realized PnL */
void pt_calibration_resolve(pt_calibration_tracker_t *c,
                            uint64_t signal_id,
                            double actual_outcome,
                            double realized_pnl);

/* Compute aggregate calibration statistics across all settled samples */
void pt_calibration_compute(pt_calibration_tracker_t *c);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_CALIBRATION_H */
