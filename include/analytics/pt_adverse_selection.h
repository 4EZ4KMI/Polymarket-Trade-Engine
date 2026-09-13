#ifndef PMT_PT_ADVERSE_SELECTION_H
#define PMT_PT_ADVERSE_SELECTION_H

#include "core/ptypes.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PT_ADV_HORIZONS 6
#define PT_ADV_MAX_RECORDS 512

/* Horizons in nanoseconds */
#define PT_ADV_H_10MS  10000000ULL
#define PT_ADV_H_50MS  50000000ULL
#define PT_ADV_H_100MS 100000000ULL
#define PT_ADV_H_500MS 500000000ULL
#define PT_ADV_H_1S   1000000000ULL
#define PT_ADV_H_5S   5000000000ULL

typedef struct {
    uint64_t       fill_id;
    pt_nsec_t      fill_time_ns;
    pt_market_id_t market_id;
    int            strategy;
    int            is_yes;
    int            side;
    pt_price_t     fill_price;
    pt_size_t      fill_qty;
    
    /* Price movement measured at each horizon (bps) */
    double         movement_bps[PT_ADV_HORIZONS];
    int            horizon_measured[PT_ADV_HORIZONS];
    int            complete;
} pt_adverse_record_t;

typedef struct {
    pt_adverse_record_t records[PT_ADV_MAX_RECORDS];
    size_t              count;
    uint64_t            next_fill_id;
    
    /* Aggregates in bps */
    double              avg_adv_bps_10ms;
    double              avg_adv_bps_50ms;
    double              avg_adv_bps_100ms;
    double              avg_adv_bps_500ms;
    double              avg_adv_bps_1s;
    double              avg_adv_bps_5s;
    double              overall_avg_adv_bps;
} pt_adverse_tracker_t;

void pt_adverse_tracker_init(pt_adverse_tracker_t *t);

/* Record a virtual fill event to track adverse selection across future price ticks */
uint64_t pt_adverse_record_fill(pt_adverse_tracker_t *t,
                                pt_nsec_t fill_time_ns,
                                pt_market_id_t market_id,
                                int strategy,
                                int is_yes,
                                int side,
                                pt_price_t fill_price,
                                pt_size_t fill_qty);

/* Process a new mid/trade price tick and update maturing horizon windows */
void pt_adverse_tracker_on_price(pt_adverse_tracker_t *t,
                                 pt_market_id_t market_id,
                                 int is_yes,
                                 pt_price_t current_mid_price,
                                 pt_nsec_t now);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_ADVERSE_SELECTION_H */
