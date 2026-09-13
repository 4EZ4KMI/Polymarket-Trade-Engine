#include "telemetry/pt_telemetry.h"
#include <stdio.h>
#include <string.h>

/* power-of-two microsecond bucket boundaries: 0, 1us, 2us, 4us, 8us, ..., 32ms */
static inline int bucket_for_(pt_nsec_t ns)
{
    uint64_t us = ns / 1000ULL;
    if (us == 0) return 0;
    int b = 64 - __builtin_clzll(us);
    return (b >= PT_LAT_BUCKETS) ? (PT_LAT_BUCKETS - 1) : b;
}

void pt_telemetry_init(pt_telemetry_t *t)
{
    memset(t, 0, sizeof(*t));
    for (int i = 0; i < PT_LATC_NUM; i++) t->stages[i].min_ns = UINT64_MAX;
    t->e2e_latency.min_ns    = UINT64_MAX;
    t->loop_tick_time.min_ns = UINT64_MAX;
}

void pt_telemetry_record_sample(pt_latency_hist_t *h, pt_nsec_t dt_ns)
{
    if (!h) return;
    h->count++;
    h->sum_ns += dt_ns;
    if (dt_ns < h->min_ns) h->min_ns = dt_ns;
    if (dt_ns > h->max_ns) h->max_ns = dt_ns;
    h->buckets[bucket_for_(dt_ns)]++;
}

double pt_telemetry_percentile_us(const pt_latency_hist_t *h, double pct)
{
    if (!h || h->count == 0) return 0.0;
    uint64_t target = (uint64_t)((double)h->count * (pct / 100.0) + 0.5);
    uint64_t cum = 0;
    for (int b = 0; b < PT_LAT_BUCKETS; b++) {
        cum += h->buckets[b];
        if (cum >= target) {
            return (double)(1ULL << b); /* approx bucket value in us */
        }
    }
    return (double)(h->max_ns / 1000.0);
}

size_t pt_telemetry_json(const pt_telemetry_t *t, char *buf, size_t max_len)
{
    if (!t || !buf || max_len == 0) return 0;
    double p50 = pt_telemetry_percentile_us(&t->e2e_latency, 50.0);
    double p90 = pt_telemetry_percentile_us(&t->e2e_latency, 90.0);
    double p99 = pt_telemetry_percentile_us(&t->e2e_latency, 99.0);
    double avg_us = t->e2e_latency.count > 0
        ? (double)(t->e2e_latency.sum_ns / t->e2e_latency.count) / 1000.0
        : 0.0;

    return (size_t)snprintf(buf, max_len,
        "{"
        "\"poly_events\":%llu,"
        "\"btc_events\":%llu,"
        "\"signals\":%llu,"
        "\"orders_submitted\":%llu,"
        "\"orders_filled\":%llu,"
        "\"orders_cancelled\":%llu,"
        "\"ring_drops\":%llu,"
        "\"latency_e2e_avg_us\":%.2f,"
        "\"latency_e2e_p50_us\":%.2f,"
        "\"latency_e2e_p90_us\":%.2f,"
        "\"latency_e2e_p99_us\":%.2f,"
        "\"latency_e2e_max_us\":%.2f"
        "}",
        (unsigned long long)t->poly_events_total,
        (unsigned long long)t->btc_events_total,
        (unsigned long long)t->signals_generated,
        (unsigned long long)t->orders_submitted,
        (unsigned long long)t->orders_filled,
        (unsigned long long)t->orders_cancelled,
        (unsigned long long)t->ring_drops_total,
        avg_us, p50, p90, p99,
        (double)(t->e2e_latency.max_ns / 1000.0));
}