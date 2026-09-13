#include "util/pt_ring.h"
#include <stdlib.h>
#include <string.h>

int pt_ring_init(pt_ring_t *r, size_t slot_size, size_t capacity_pow2)
{
    if (r == NULL || slot_size == 0 || capacity_pow2 == 0)
        return -1;
    size_t cap = (size_t)1 << capacity_pow2;
    r->buf = (uint8_t *)malloc(slot_size * cap);
    if (r->buf == NULL)
        return -1;
    r->slot_size = slot_size;
    r->capacity  = cap;
    atomic_init(&r->head, 0);
    atomic_init(&r->tail, 0);
    atomic_init(&r->dropped, 0);
    return 0;
}

void pt_ring_destroy(pt_ring_t *r)
{
    if (r == NULL) return;
    free(r->buf);
    r->buf = NULL;
}

static inline size_t ring_next_(pt_ring_t *r, size_t idx)
{
    return (idx + 1) & (r->capacity - 1);
}

int pt_ring_push(pt_ring_t *r, const void *data)
{
    size_t head = atomic_load_explicit(&r->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);
    if (ring_next_(r, head) == tail) {
        atomic_fetch_add_explicit(&r->dropped, 1, memory_order_relaxed);
        return 0; /* full */
    }
    uint8_t *slot = r->buf + head * r->slot_size;
    memcpy(slot, data, r->slot_size);
    atomic_store_explicit(&r->head, ring_next_(r, head), memory_order_release);
    return 1;
}

int pt_ring_pop(pt_ring_t *r, void *dst)
{
    size_t tail = atomic_load_explicit(&r->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&r->head, memory_order_acquire);
    if (tail == head)
        return 0; /* empty */
    uint8_t *slot = r->buf + tail * r->slot_size;
    if (dst != NULL)
        memcpy(dst, slot, r->slot_size);
    atomic_store_explicit(&r->tail, ring_next_(r, tail), memory_order_release);
    return 1;
}

int pt_ring_peek(const pt_ring_t *r, void *dst)
{
    size_t tail = atomic_load_explicit(&r->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&r->head, memory_order_acquire);
    if (tail == head)
        return 0;
    if (dst != NULL)
        memcpy(dst, r->buf + tail * r->slot_size, r->slot_size);
    return 1;
}

size_t pt_ring_len(const pt_ring_t *r)
{
    size_t head = atomic_load_explicit(&r->head, memory_order_acquire);
    size_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);
    size_t len = head - tail;
    return len & (r->capacity - 1);
}

size_t pt_ring_free(const pt_ring_t *r)
{
    return r->capacity - 1 - pt_ring_len(r);
}

uint64_t pt_ring_dropped(const pt_ring_t *r)
{
    return atomic_load_explicit(&r->dropped, memory_order_relaxed);
}