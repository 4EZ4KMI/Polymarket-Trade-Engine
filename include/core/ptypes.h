#ifndef PMT_PTYPES_H
#define PMT_PTYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Core scalar types.                                                 */
/*                                                                     */
/*  Hot-path arithmetic must stay in integer domain.                   */
/*                                                                     */
/*  price:  scaled by PT_PRICE_SCALE (1e3 => 0.001 resolution).        */
/*          e.g. 0.470 -> 470.                                         */
/*  size:   integer shares (share = 1 USDC notional on Polymarket).    */
/* ------------------------------------------------------------------ */

#define PT_PRICE_SCALE 1000LL
typedef int64_t  pt_price_t;   /* scaled price */
typedef uint64_t pt_size_t;    /* shares / liquidity volume */

typedef uint64_t pt_seq_t;     /* monotonic event sequence number */
typedef uint64_t pt_msec_t;    /* wall-clock milliseconds */
typedef uint64_t pt_nsec_t;    /* nanosecond timestamps (realtime & monotonic) */

/* market / order / signal opaque ids (kept as integers for cheap hashing) */
typedef uint64_t pt_token_id_t;
typedef uint64_t pt_market_id_t;
typedef uint64_t pt_order_id_t;
typedef uint64_t pt_flow_id_t;   /* execution / flow handle */

typedef enum {
    PT_SIDE_BID = 1,
    PT_SIDE_ASK = 2
} pt_side_t;

typedef enum {
    PT_DIR_NONE = 0,
    PT_DIR_BUY  = 1,   /* buy YES or NO outcome -> long probability */
    PT_DIR_SELL = 2
} pt_dir_t;

typedef enum {
    PT_LIFECYCLE_DISCOVERED = 0,
    PT_LIFECYCLE_SUBSCRIBED,
    PT_LIFECYCLE_ACTIVE,
    PT_LIFECYCLE_EXPIRING,
    PT_LIFECYCLE_EXPIRED,
    PT_LIFECYCLE_RESOLVED
} pt_lifecycle_t;

typedef enum {
    PT_MODE_UNSET   = 0,
    PT_MODE_BACKTEST,
    PT_MODE_PAPER,
    PT_MODE_LIVE
} pt_mode_t;

/* latency breakdown points */
typedef enum {
    PT_LATC_EXCHANGE_EVENT = 0,  /* exchange_event_time */
    PT_LATC_RECEIVE,             /* receive_time          */
    PT_LATC_PROCESS,             /* processing_time       */
    PT_LATC_SIGNAL,              /* signal_time           */
    PT_LATC_SUBMIT,              /* order_submit_time     */
    PT_LATC_ACK,                 /* order_ack_time        */
    PT_LATC_FILL,                /* fill_time             */
    PT_LATC_NUM
} pt_latency_point_t;

#ifdef __cplusplus
}
#endif
#endif /* PMT_PTYPES_H */