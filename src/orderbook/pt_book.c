#include "orderbook/pt_book.h"
#include <math.h>
#include <string.h>

void pt_book_init(pt_book_t *b)
{
    memset(b, 0, sizeof(*b));
}

/* find insertion index for ascending price; sets exact=1 if present */
static int side_find(const pt_side_book_t *s, pt_price_t price, int *exact)
{
    int lo = 0, hi = s->count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (s->levels[mid].price < price) lo = mid + 1;
        else                              hi = mid;
    }
    *exact = (lo < s->count && s->levels[lo].price == price);
    return lo;
}

/* apply update to one side; returns 0 ok, -1 overflow */
static int side_apply(pt_side_book_t *s, pt_price_t price, int64_t amount,
                      int is_delta)
{
    int exact, idx = side_find(s, price, &exact);
    if (exact) {
        pt_level_t *lv = &s->levels[idx];
        int64_t cur = (int64_t)lv->size;
        int64_t nxt = is_delta ? (cur + amount) : amount;
        if (nxt <= 0) {
            memmove(&s->levels[idx], &s->levels[idx + 1],
                    (size_t)(s->count - idx - 1) * sizeof(pt_level_t));
            s->count--;
        } else {
            lv->size = (pt_size_t)nxt;
        }
        return 0;
    }
    if (amount <= 0)
        return 0; /* deleting a nonexistent level: no-op */
    if (s->count >= PT_BOOK_MAX_LEVELS)
        return -1;
    memmove(&s->levels[idx + 1], &s->levels[idx],
            (size_t)(s->count - idx) * sizeof(pt_level_t));
    s->levels[idx].price = price;
    s->levels[idx].size  = (pt_size_t)amount;
    s->count++;
    return 0;
}

int pt_book_update(pt_book_t *b, int side, pt_price_t price, int64_t amount,
                   int is_delta, pt_seq_t seq)
{
    int rc;
    if (b == NULL || price < 0)
        return -1;
    if (side == PT_SIDE_BID)
        rc = side_apply(&b->bids, price, amount, is_delta);
    else if (side == PT_SIDE_ASK)
        rc = side_apply(&b->asks, price, amount, is_delta);
    else
        return -1;
    if (rc == 0 && seq > b->seq)
        b->seq = seq;
    return rc;
}

int pt_book_snapshot(pt_book_t *b, int side, const pt_level_t *levels, int n,
                     pt_seq_t seq)
{
    if (b == NULL || n < 0 || n > PT_BOOK_MAX_LEVELS)
        return -1;
    pt_side_book_t *s = (side == PT_SIDE_BID) ? &b->bids : &b->asks;
    s->count = 0;
    if (levels != NULL) {
        for (int i = 0; i < n; i++) {
            if (levels[i].size == 0)
                continue;
            s->levels[s->count++] = levels[i];
        }
    }
    if (seq > b->seq)
        b->seq = seq;
    return 0;
}

int pt_book_best_bid(const pt_book_t *b, pt_price_t *price, pt_size_t *size)
{
    if (b->bids.count == 0)
        return -1;
    const pt_level_t *lv = &b->bids.levels[b->bids.count - 1];
    if (price) *price = lv->price;
    if (size)  *size  = lv->size;
    return 0;
}

int pt_book_best_ask(const pt_book_t *b, pt_price_t *price, pt_size_t *size)
{
    if (b->asks.count == 0)
        return -1;
    const pt_level_t *lv = &b->asks.levels[0];
    if (price) *price = lv->price;
    if (size)  *size  = lv->size;
    return 0;
}

int pt_book_mid(const pt_book_t *b, double *mid)
{
    pt_price_t bp, ap;
    if (pt_book_best_bid(b, &bp, NULL) || pt_book_best_ask(b, &ap, NULL))
        return -1;
    *mid = ((double)bp + (double)ap) / 2.0 / (double)PT_PRICE_SCALE;
    return 0;
}

pt_size_t pt_book_depth_volume(const pt_book_t *b, int side, int levels)
{
    const pt_side_book_t *s = (side == PT_SIDE_BID) ? &b->bids : &b->asks;
    int n = (levels <= 0 || levels > s->count) ? s->count : levels;
    pt_size_t total = 0;
    if (side == PT_SIDE_ASK) {
        for (int i = 0; i < n; i++) total += s->levels[i].size;
    } else {
        for (int i = s->count - 1; i >= s->count - n; i--) total += s->levels[i].size;
    }
    return total;
}

int64_t pt_book_depth_weighted(const pt_book_t *b, int side, int levels)
{
    const pt_side_book_t *s = (side == PT_SIDE_BID) ? &b->bids : &b->asks;
    int n = (levels <= 0 || levels > s->count) ? s->count : levels;
    int64_t w = 0;
    if (side == PT_SIDE_ASK) {
        for (int i = 0; i < n; i++)
            w += (int64_t)s->levels[i].price * (int64_t)s->levels[i].size;
    } else {
        for (int i = s->count - 1; i >= s->count - n; i--)
            w += (int64_t)s->levels[i].price * (int64_t)s->levels[i].size;
    }
    return w;
}

double pt_book_imbalance(const pt_book_t *b, int levels)
{
    pt_size_t bid = pt_book_depth_volume(b, PT_SIDE_BID, levels);
    pt_size_t ask = pt_book_depth_volume(b, PT_SIDE_ASK, levels);
    if (bid + ask == 0)
        return NAN;
    return ((double)bid - (double)ask) / ((double)bid + (double)ask);
}

pt_size_t pt_book_walk(const pt_book_t *b, int side, pt_size_t want,
                       int64_t *avg_price_scaled, int *levels_used)
{
    const pt_side_book_t *s = (side == PT_SIDE_BID) ? &b->bids : &b->asks;
    pt_size_t filled = 0;
    int64_t cost = 0;
    int used = 0;

    if (side == PT_SIDE_ASK) {
        for (int i = 0; i < s->count && filled < want; i++) {
            const pt_level_t *lv = &s->levels[i];
            pt_size_t take = (lv->size < want - filled) ? lv->size : (want - filled);
            cost += (int64_t)lv->price * (int64_t)take;
            filled += take;
            used++;
        }
    } else {
        for (int i = s->count - 1; i >= 0 && filled < want; i--) {
            const pt_level_t *lv = &s->levels[i];
            pt_size_t take = (lv->size < want - filled) ? lv->size : (want - filled);
            cost += (int64_t)lv->price * (int64_t)take;
            filled += take;
            used++;
        }
    }
    if (avg_price_scaled) *avg_price_scaled = (filled > 0) ? (cost / filled) : 0;
    if (levels_used)      *levels_used      = used;
    return filled;
}

void pt_book_record_trade(pt_book_t *b, pt_price_t price, pt_size_t size,
                          int side, pt_seq_t seq)
{
    b->last_trade_price = price;
    b->last_trade_size  = size;
    b->last_trade_side  = side;
    if (seq > b->last_trade_seq)
        b->last_trade_seq = seq;
}