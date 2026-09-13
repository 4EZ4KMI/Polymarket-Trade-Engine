#include "util/pt_clock.h"
#include <time.h>

pt_nsec_t pt_clock_realtime_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

pt_nsec_t pt_clock_mono_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

struct tm *pt_clock_utc(const pt_nsec_t realtime_ns, struct tm *out)
{
    time_t secs = (time_t)(realtime_ns / 1000000000ULL);
    if (gmtime_r(&secs, out) == NULL) {
        out->tm_year = 0;
        out->tm_mon  = 0;
        out->tm_mday = 0;
        out->tm_hour = 0;
        out->tm_min  = 0;
        out->tm_sec  = 0;
    }
    return out;
}