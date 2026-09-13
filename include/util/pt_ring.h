#ifndef PMT_PT_RING_H
#define PMT_PT_RING_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Lock-free single-producer / single-consumer ring buffer.
 *
 * Allocations are fixed at init. Slots are fixed-size bytes so that
 * events of different kinds can coexist in one channel.
 *
 * Memory ordering:
 *   - producer publishes the payload (release on head advance)
 *   - consumer observes the payload (acquire on tail read)
 */

typedef struct {
    size_t    slot_size;   /* bytes per slot */
    size_t    capacity;    /* number of slots (power of two) */
    uint8_t  *buf;
    _Atomic size_t head;   /* producer index */
    _Atomic size_t tail;   /* consumer index */
    _Atomic uint64_t dropped; /* overflow counter */
} pt_ring_t;

int  pt_ring_init(pt_ring_t *r, size_t slot_size, size_t capacity_pow2);
void pt_ring_destroy(pt_ring_t *r);

/* returns 1 on success, 0 on full/overflow (drops counter incremented) */
int pt_ring_push(pt_ring_t *r, const void *data);
/* returns 1 and copies into dst on success, 0 when empty */
int pt_ring_pop(pt_ring_t *r, void *dst);
/* non-consuming peek of head element; returns 1 if present */
int pt_ring_peek(const pt_ring_t *r, void *dst);

size_t pt_ring_len(const pt_ring_t *r);
size_t pt_ring_free(const pt_ring_t *r);
uint64_t pt_ring_dropped(const pt_ring_t *r);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_RING_H */