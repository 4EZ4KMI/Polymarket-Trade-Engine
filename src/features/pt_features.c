#include "features/pt_features.h"
#include <math.h>
#include <string.h>

const pt_nsec_t PT_FLOW_TFNS[PT_FLOW_NTF] = {
    100000000ULL, 250000000ULL, 500000000ULL,
    1000000000ULL, 2000000000ULL, 5000000000ULL
};
const pt_nsec_t PT_BTC_TFNS[PT_BTC_NTF] = {
    10000000ULL, 50000000ULL, 100000000ULL, 250000000ULL, 500000000ULL,
    1000000000ULL, 2000000000ULL, 5000000000ULL, 10000000000ULL
};

int pt_flow_init(pt_flow_t *f, size_t cap)
{
    if (pt_winbuf_init(&f->buy, cap) ||
        pt_winbuf_init(&f->sell, cap) ||
        pt_winbuf_init(&f->cnt, cap)) {
        pt_winbuf_destroy(&f->buy); pt_winbuf_destroy(&f->sell); pt_winbuf_destroy(&f->cnt);
        return -1;
    }
    return 0;
}

void pt_flow_destroy(pt_flow_t *f)
{
    pt_winbuf_destroy(&f->buy); pt_winbuf_destroy(&f->sell); pt_winbuf_destroy(&f->cnt);
}

void pt_flow_add_trade(pt_flow_t *f, pt_nsec_t t, int is_buy, double size)
{
    if (is_buy) {
        pt_winbuf_add(&f->buy, t, size);
        pt_winbuf_add(&f->cnt, t, 1.0);
    } else {
        pt_winbuf_add(&f->sell, t, size);
        pt_winbuf_add(&f->cnt, t, 1.0);
    }
}

void pt_flow_snapshot(const pt_flow_t *f, pt_nsec_t now, pt_flow_window_t *out)
{
    for (int i = 0; i < PT_FLOW_NTF; i++) {
        pt_winagg_t ab, as, ac;
        pt_flow_window_t *o = &out[i];
        memset(o, 0, sizeof(*o));
        int hb = pt_winbuf_agg(&f->buy, now, PT_FLOW_TFNS[i], &ab);
        int hs = pt_winbuf_agg(&f->sell, now, PT_FLOW_TFNS[i], &as);
        int hc = pt_winbuf_agg(&f->cnt, now, PT_FLOW_TFNS[i], &ac);
        o->buy_vol  = hb ? ab.sum : 0.0;
        o->sell_vol = hs ? as.sum : 0.0;
        o->count    = hc ? ac.sum : 0;
        if (o->sell_vol > 1e-12)
            o->buy_sell_ratio = o->buy_vol / o->sell_vol;
        else
            o->buy_sell_ratio = (o->buy_vol > 1e-12) ? INFINITY : 0.0;
        double dur_sec = (double)PT_FLOW_TFNS[i] / 1e9;
        o->rate_per_sec = (double)o->count / dur_sec;
    }
}

int pt_btc_init(pt_btc_t *b, size_t cap)
{
    return pt_winbuf_init(&b->mid, cap);
}

void pt_btc_destroy(pt_btc_t *b)
{
    pt_winbuf_destroy(&b->mid);
}

void pt_btc_add(pt_btc_t *b, pt_nsec_t t, double mid)
{
    pt_winbuf_add(&b->mid, t, mid);
}

void pt_btc_snapshot(const pt_btc_t *b, pt_nsec_t now, pt_btc_window_t *out,
                     double *last_mid, int *have_data)
{
    pt_winagg_t all;
    int h = pt_winbuf_agg(&b->mid, now, UINT64_MAX, &all);
    if (h && last_mid)   *last_mid = all.last;
    if (h && have_data)   *have_data = 1;
    if (!h && have_data)  *have_data = 0;

    for (int i = 0; i < PT_BTC_NTF; i++) {
        pt_btc_window_t *o = &out[i];
        memset(o, 0, sizeof(*o));
        pt_winagg_t wa;
        if (!pt_winbuf_agg(&b->mid, now, PT_BTC_TFNS[i], &wa) || wa.count < 2) {
            o->count = wa.count;
            continue;
        }
        double base = wa.first;
        if (base == 0.0) base = 1e-9;
        o->return_pct   = (wa.last - wa.first) / base * 100.0;
        o->max_move_pct = (wa.max - wa.min) / base * 100.0;
        double dt = (double)(wa.last_t - wa.first_t) / 1e9;
        o->velocity = (dt > 0) ? fabs(wa.last - wa.first) / dt : 0.0;
        o->count = wa.count;
    }
}

void pt_book_features_compute(const pt_book_t *b, pt_book_features_t *out)
{
    memset(out, 0, sizeof(*out));
    out->have_book = 0;
    if (b == NULL) return;
    pt_price_t bp = 0, ap = 0;
    if (pt_book_best_bid(b, &bp, NULL) || pt_book_best_ask(b, &ap, NULL))
        return; /* no book yet */
    out->have_book = 1;
    out->best_bid = bp; out->best_ask = ap;
    out->spread = ((double)ap - (double)bp) / (double)PT_PRICE_SCALE;
    double mid = ((double)bp + (double)ap) / 2.0;
    out->mid = mid / (double)PT_PRICE_SCALE;

    out->bid_vol_L1 = pt_book_depth_volume(b, PT_SIDE_BID, 1);
    out->ask_vol_L1 = pt_book_depth_volume(b, PT_SIDE_ASK, 1);
    out->bid_vol_L3 = pt_book_depth_volume(b, PT_SIDE_BID, 3);
    out->ask_vol_L3 = pt_book_depth_volume(b, PT_SIDE_ASK, 3);
    out->bid_vol_L5 = pt_book_depth_volume(b, PT_SIDE_BID, 5);
    out->ask_vol_L5 = pt_book_depth_volume(b, PT_SIDE_ASK, 5);
    out->bid_vol_L10 = pt_book_depth_volume(b, PT_SIDE_BID, 10);
    out->ask_vol_L10 = pt_book_depth_volume(b, PT_SIDE_ASK, 10);

    out->imb_L1 = pt_book_imbalance(b, 1);
    out->imb_L3 = pt_book_imbalance(b, 3);
    out->imb_L5 = pt_book_imbalance(b, 5);
    out->imb_L10 = pt_book_imbalance(b, 10);

    /* microprice = (bid*ask_qty + ask*bid_qty)/(bid_qty+ask_qty) using L1 */
    double den = (double)out->bid_vol_L1 + (double)out->ask_vol_L1;
    if (den > 1e-12)
        out->microprice = ((double)bp * (double)out->ask_vol_L1 +
                           (double)ap * (double)out->bid_vol_L1) / den /
                          (double)PT_PRICE_SCALE;
    else
        out->microprice = out->mid;

    /* distance-weighted imbalance: weight = 1/level_index across up to 10 */
    const pt_side_book_t *bd = &b->bids, *as = &b->asks;
    double wb = 0.0, wa = 0.0;
    int nb = (bd->count < 10) ? bd->count : 10;
    int na = (as->count < 10) ? as->count : 10;
    for (int i = 0; i < nb; i++)
        wb += (double)bd->levels[bd->count - 1 - i].size / (double)(i + 1);
    for (int i = 0; i < na; i++)
        wa += (double)as->levels[i].size / (double)(i + 1);
    out->wimb = (wb + wa > 1e-12) ? (wb - wa) / (wb + wa) : NAN;
}