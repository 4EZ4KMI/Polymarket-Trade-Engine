#ifndef PMT_PT_TELEMETRY_H
#define PMT_PT_TELEMETRY_H

#include "core/ptypes.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PT_LAT_BUCKETS 32

typedef struct {
    uint64_t count;
    uint64_t min_ns;
    uint64_t max_ns;
    uint64_t sum_ns;
    uint64_t buckets[PT_LAT_BUCKETS];
} pt_latency_hist_t;

typedef struct {
    /* latencies for each pipeline phase */
    pt_latency_hist_t stages[PT_LATC_NUM];
    pt_latency_hist_t e2e_latency;     /* exchange_event -> fill */
    pt_latency_hist_t loop_tick_time;  /* trading loop tick processing time */
    /* throughput & event counters */
    uint64_t poly_events_total;
    uint64_t btc_events_total;
    uint64_t signals_generated;
    uint64_t orders_submitted;
    uint64_t orders_filled;
    uint64_t orders_cancelled;
    uint64_t ring_drops_total;
} pt_telemetry_t;

void pt_telemetry_init(pt_telemetry_t *t);
void pt_telemetry_record_sample(pt_latency_hist_t *h, pt_nsec_t dt_ns);
double pt_telemetry_percentile_us(const pt_latency_hist_t *h, double pct);

/* Format a telemetry summary into a JSON string buffer for Dashboard API */
size_t pt_telemetry_json(const pt_telemetry_t *t, char *buf, size_t max_len);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_TELEMETRY_H */