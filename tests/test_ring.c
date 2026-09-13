#include "test_harness.h"
#include "util/pt_ring.h"
#include <string.h>

PT_T(ring_push_pop)
{
    pt_ring_t r;
    size_t sz = sizeof(uint64_t);
    PT_ASSERT(pt_ring_init(&r, sz, 4) == 0); /* 16 slots */
    uint64_t in[10];
    for (int i = 0; i < 10; i++) {
        in[i] = (uint64_t)i * 7;
        PT_ASSERT(pt_ring_push(&r, &in[i]) == 1);
    }
    PT_ASSERT(pt_ring_len(&r) == 10);
    for (int i = 0; i < 10; i++) {
        uint64_t out = 0;
        PT_ASSERT(pt_ring_pop(&r, &out) == 1);
        PT_ASSERT(out == in[i]);
    }
    uint64_t x = 0;
    PT_ASSERT(pt_ring_pop(&r, &x) == 0); /* empty */
    pt_ring_destroy(&r);
}

PT_T(ring_overflow_drops)
{
    pt_ring_t r;
    size_t sz = sizeof(uint64_t);
    PT_ASSERT(pt_ring_init(&r, sz, 3) == 0); /* 8 slots -> capacity-1 = 7 usable */
    uint64_t v = 0;
    for (int i = 0; i < 20; i++) {
        pt_ring_push(&r, &v);
    }
    PT_ASSERT(pt_ring_len(&r) == 7);
    PT_ASSERT(pt_ring_dropped(&r) == 13);
    pt_ring_destroy(&r);
}