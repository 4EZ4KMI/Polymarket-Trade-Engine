#ifndef PMT_PT_EVENT_LOG_H
#define PMT_PT_EVENT_LOG_H

#include "core/ptypes.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Binary event types stored sequentially in an mmap file. */
typedef enum {
    PT_EV_BOOK_DELTA = 1,
    PT_EV_BOOK_SNAP,
    PT_EV_TRADE_TICK,
    PT_EV_BTC_TICK,
    PT_EV_SIGNAL,
    PT_EV_ORDER_SUBMIT,
    PT_EV_ORDER_FILL,
    PT_EV_ORDER_CANCEL,
    PT_EV_RISK_REJECT,
    PT_EV_KILL_SWITCH
} pt_ev_type_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t    ev_type;          /* pt_ev_type_t */
    pt_nsec_t  ts;               /* local monotonic receive ns */
    pt_seq_t   seq;
    uint64_t   id1, id2;         /* market_id, token_id, order_id */
    int64_t    val1, val2;       /* price, size, pnl */
    double     dval1, dval2;     /* edge, confidence */
} pt_raw_event_t;
#pragma pack(pop)

typedef struct {
    int      fd;
    void    *mmap_base;
    size_t   capacity_bytes;
    size_t   write_offset;
    char     filepath[256];
} pt_mmap_log_t;

/* open/create an mmap ring-log file of `capacity_bytes` (e.g. 64MB) */
int  pt_mmap_log_open(pt_mmap_log_t *l, const char *path, size_t capacity_bytes);
void pt_mmap_log_close(pt_mmap_log_t *l);
/* appends raw event; returns 0 ok, -1 on log full/error */
int  pt_mmap_log_append(pt_mmap_log_t *l, const pt_raw_event_t *ev);
void pt_mmap_log_flush(pt_mmap_log_t *l);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_EVENT_LOG_H */