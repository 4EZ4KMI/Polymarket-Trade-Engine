#ifndef PMT_PT_WINBUF_H
#define PMT_PT_WINBUF_H

#include "core/ptypes.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed-capacity circular buffer of (timestamp, value) samples.
 * Samples are assumed appended in monotonic (or equal) timestamp order. */
typedef struct {
    pt_nsec_t *t;
    double    *v;
    size_t     cap;
    size_t     head;   /* next write slot */
    size_t     count;  /* valid samples */
} pt_winbuf_t;

int  pt_winbuf_init(pt_winbuf_t *w, size_t cap);
void pt_winbuf_destroy(pt_winbuf_t *w);
void pt_winbuf_reset(pt_winbuf_t *w);
/* newer or equal timestamps overwrite the sample at the same time to keep
 * monotonicity; otherwise append. drops oldest when full. */
void pt_winbuf_add(pt_winbuf_t *w, pt_nsec_t t, double v);

typedef struct {
    double   sum, min, max;
    double   first, last;   /* oldest / newest within window */
    pt_nsec_t first_t, last_t;
    size_t   count;
    int      ok;
} pt_winagg_t;

/* aggregate samples with last_t in (now-dur, now]. returns 1 if any. */
int pt_winbuf_agg(const pt_winbuf_t *w, pt_nsec_t now, pt_nsec_t dur,
                  pt_winagg_t *out);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_WINBUF_H */