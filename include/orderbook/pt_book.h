#ifndef PMT_PT_BOOK_H
#define PMT_PT_BOOK_H

#include "core/ptypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PT_BOOK_MAX_LEVELS 64

typedef struct {
    pt_price_t price;   /* scaled integer */
    pt_size_t  size;    /* shares */
} pt_level_t;

/* One side of an L2 book, levels kept ascending by price. */
typedef struct {
    int         count;
    pt_level_t  levels[PT_BOOK_MAX_LEVELS];
} pt_side_book_t;

typedef struct {
    pt_side_book_t bids;  /* ascending by price (best bid = last) */
    pt_side_book_t asks;  /* ascending by price (best ask = first) */
    pt_seq_t  seq;    /* last applied delta seq */
    pt_seq_t  last_trade_seq;
    /* last trade */
    pt_price_t last_trade_price;
    pt_size_t  last_trade_size;
    int        last_trade_side; /* 0=unknown 1=buy 2=sell */
} pt_book_t;

void pt_book_init(pt_book_t *b);

/* Apply a single level update.
 * side: PT_SIDE_BID / PT_SIDE_ASK
 * amount:
 *    is_delta==false => absolute size (>=0); 0 removes the level.
 *    is_delta==true  => signed delta added to current size; <=0 removes.
 * Returns 0 on success, -1 on invalid args / level overflow. */
int pt_book_update(pt_book_t *b, int side, pt_price_t price, int64_t amount,
                   int is_delta, pt_seq_t seq);

/* Replace an entire side with a snapshot (ascending levels). */
int pt_book_snapshot(pt_book_t *b, int side, const pt_level_t *levels,
                     int n, pt_seq_t seq);

/* --- queries (integer domain) --- */
int   pt_book_best_bid(const pt_book_t *b, pt_price_t *price, pt_size_t *size);
int   pt_book_best_ask(const pt_book_t *b, pt_price_t *price, pt_size_t *size);
int   pt_book_mid(const pt_book_t *b, double *mid);

/* cumulative size across the best N levels of a side. 0 => all levels. */
pt_size_t pt_book_depth_volume(const pt_book_t *b, int side, int levels);
/* sum(price_i * size_i) across best N levels (scaled) */
int64_t   pt_book_depth_weighted(const pt_book_t *b, int side, int levels);

/* imbalance = (bid - ask)/(bid + ask) across best N levels, in [-1, +1].
 * Returns NaN if no liquidity. */
double    pt_book_imbalance(const pt_book_t *b, int levels);

/* Walk the ask (or bid) side to fill `want` shares.
 * Fills max_fillable (<= want), returns average exec price (scaled int)
 * and fills_prices (optional, sized array of up to count levels used). */
pt_size_t pt_book_walk(const pt_book_t *b, int side, pt_size_t want,
                       int64_t *avg_price_scaled, int *levels_used);

void pt_book_record_trade(pt_book_t *b, pt_price_t price, pt_size_t size,
                          int side, pt_seq_t seq);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_BOOK_H */