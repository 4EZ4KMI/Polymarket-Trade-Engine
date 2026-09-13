#include "util/pt_winbuf.h"
#include <stdlib.h>
#include <string.h>

int pt_winbuf_init(pt_winbuf_t *w, size_t cap)
{
    if (w == NULL || cap == 0) return -1;
    w->t = (pt_nsec_t *)malloc(sizeof(pt_nsec_t) * cap);
    w->v = (double *)malloc(sizeof(double) * cap);
    if (w->t == NULL || w->v == NULL) {
        free(w->t); free(w->v);
        return -1;
    }
    w->cap = cap; w->head = 0; w->count = 0;
    return 0;
}

void pt_winbuf_destroy(pt_winbuf_t *w)
{
    if (w == NULL) return;
    free(w->t); free(w->v);
    w->t = NULL; w->v = NULL; w->cap = 0;
}

void pt_winbuf_reset(pt_winbuf_t *w)
{
    w->head = 0; w->count = 0;
}

void pt_winbuf_add(pt_winbuf_t *w, pt_nsec_t t, double v)
{
    if (w->count > 0) {
        size_t latest = (w->head + w->count - 1) % w->cap;
        if (t < w->t[latest]) t = w->t[latest]; /* enforce monotonic */
        if (t == w->t[latest]) {
            w->v[latest] = v;          /* replace equal-timestamp sample */
            return;
        }
    }
    if (w->count == w->cap) {
        w->t[w->head] = t;
        w->v[w->head] = v;
        w->head = (w->head + 1) % w->cap;
        return;
    }
    w->t[w->head] = t;
    w->v[w->head] = v;
    w->head = (w->head + 1) % w->cap;
    w->count++;
}

int pt_winbuf_agg(const pt_winbuf_t *w, pt_nsec_t now, pt_nsec_t dur,
                  pt_winagg_t *out)
{
    if (w == NULL || out == NULL || dur == 0) return 0;
    pt_nsec_t lo = (now > dur) ? (now - dur) : 0;
    double sum = 0.0, mn = 0.0, mx = 0.0;
    double first = 0.0, last = 0.0;
    pt_nsec_t first_t = 0, last_t = 0;
    size_t n = 0;

    /* iterate newest -> oldest */
    for (size_t k = 0; k < w->count; k++) {
        size_t idx = (w->head + w->count - 1 - k) % w->cap;
        pt_nsec_t tt = w->t[idx];
        if (tt > now) continue;              /* future sample */
        if (tt <= lo) break;                 /* before window & older */
        double v = w->v[idx];
        if (n == 0) { mn = mx = v; }
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += v;
        if (n == 0) { last = v; last_t = tt; }
        first = v; first_t = tt; /* oldest so far */
        n++;
    }
    if (n == 0) return 0;
    out->sum = sum; out->min = mn; out->max = mx;
    out->first = first; out->last = last;
    out->first_t = first_t; out->last_t = last_t;
    out->count = n; out->ok = 1;
    return 1;
}