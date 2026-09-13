#include "test_harness.h"
#include "strategies/pt_model.h"
#include "analytics/pt_calibration.h"
#include <string.h>

PT_T(model_predictions)
{
    pt_feature_vector_t fv;
    memset(&fv, 0, sizeof(fv));
    fv.yes_bid = 490;
    fv.yes_ask = 510;
    fv.btc_ret_1s = 0.0020; /* +0.20% BTC rally */
    fv.btc_velocity = 50.0;
    fv.imb_l3 = 0.40;
    fv.time_to_expiry_sec = 600.0;

    /* Baseline Heuristic */
    pt_model_cfg_t cfg_heur = { .type = PT_MODEL_HEURISTIC };
    double p_heur = pt_model_predict_prob(&cfg_heur, &fv);
    PT_ASSERT(p_heur > 0.50); /* Bullish */

    /* Logistic */
    pt_model_cfg_t cfg_log = {
        .type = PT_MODEL_LOGISTIC,
        .w0 = 0.0,
        .w_ret1s = 2.0,
        .w_ret5s = 1.0,
        .w_imb = 0.5,
        .w_velocity = 0.1,
        .w_expiry = 0.0
    };
    double p_log = pt_model_predict_prob(&cfg_log, &fv);
    PT_ASSERT(p_log > 0.50 && p_log <= 1.0);
}

PT_T(calibration_framework_metrics)
{
    pt_calibration_tracker_t calib;
    pt_calibration_init(&calib);

    /* Record 4 signals and resolve them */
    pt_calibration_record_signal(&calib, 1, 0.70, 0.50, 300.0);
    pt_calibration_resolve(&calib, 1, 1.0, 15.0); /* Win */

    pt_calibration_record_signal(&calib, 2, 0.70, 0.50, 300.0);
    pt_calibration_resolve(&calib, 2, 1.0, 15.0); /* Win */

    pt_calibration_record_signal(&calib, 3, 0.70, 0.50, 300.0);
    pt_calibration_resolve(&calib, 3, 0.0, -10.0); /* Loss */

    pt_calibration_record_signal(&calib, 4, 0.70, 0.50, 300.0);
    pt_calibration_resolve(&calib, 4, 1.0, 15.0); /* Win */

    pt_calibration_compute(&calib);

    PT_ASSERT(calib.settled_count == 4);
    PT_ASSERT(calib.brier_score > 0.0 && calib.brier_score < 0.5);
    PT_ASSERT(calib.log_loss > 0.0);
    /* 3 wins out of 4 -> actual win rate = 75%, predicted ~70% -> ECE ~ 5% */
    PT_ASSERT(calib.expected_calibration_error < 0.15);
}
