#ifndef PMT_PT_CLOCK_H
#define PMT_PT_CLOCK_H

#include "core/ptypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* fast realtime (epoch ns) and monotonic (ns) reads. */
pt_nsec_t pt_clock_realtime_ns(void);
pt_nsec_t pt_clock_mono_ns(void);

#include <time.h>
/* convert realtime ns -> broken-down for logging */
struct tm *pt_clock_utc(const pt_nsec_t realtime_ns, struct tm *out);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_CLOCK_H */