#ifndef PMT_PT_FEATURES_H
#define PMT_PT_FEATURES_H

#include "core/ptypes.h"
#include "orderbook/pt_book.h"
#include "util/pt_winbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- trade-flow timeframes ---------------------------------------- */
#define PT_FLOW_NTF 6
extern const pt_nsec_t PT_FLOW_TFNS[PT_FLOW_NTF]; /* 100ms..5s */

typedef struct {
    double buy_vol, sell_vol;
    size_t count;
    double buy_sell_ratio; /* inf-friendly: buy/sell; 0 if no sell vol */
    double rate_per_sec;
} pt_flow_window_t;

typedef struct {
    pt_winbuf_t buy;     /* buy volume samples */
    pt_winbuf_t sell;    /* sell volume samples */
    pt_winbuf_t cnt;     /* trade count samples */
} pt_flow_t;

int  pt_flow_init(pt_flow_t *f, size_t cap);
void pt_flow_destroy(pt_flow_t *f);
void pt_flow_add_trade(pt_flow_t *f, pt_nsec_t t, int is_buy, double size);
void pt_flow_snapshot(const pt_flow_t *f, pt_nsec_t now,
                      pt_flow_window_t *out /*[PT_FLOW_NTF]*/);

/* ---- BTC momentum timeframes: 10ms..10s --------------------------- */
#define PT_BTC_NTF 9
extern const pt_nsec_t PT_BTC_TFNS[PT_BTC_NTF];

typedef struct {
    double return_pct;  /* (last-first)/first * 100; 0 if window empty */
    double max_move_pct;/* (max-min)/first * 100  (short-term vol proxy) */
    double velocity;    /* |last-first| per second (price units/sec) */
    size_t count;
} pt_btc_window_t;

typedef struct {
    pt_winbuf_t mid;    /* BTC mid price samples */
} pt_btc_t;

int  pt_btc_init(pt_btc_t *b, size_t cap);
void pt_btc_destroy(pt_btc_t *b);
void pt_btc_add(pt_btc_t *b, pt_nsec_t t, double mid);
void pt_btc_snapshot(const pt_btc_t *b, pt_nsec_t now,
                     pt_btc_window_t *out /*[PT_BTC_NTF]*/,
                     double *last_mid, int *have_data);

/* ---- order book derived features ---------------------------------- */
typedef struct {
    int      have_book;
    pt_price_t best_bid, best_ask;
    double   spread;      /* (ask-bid) in probability points */
    double   mid;         /* probability */
    double   microprice;
    pt_size_t bid_vol_L1, ask_vol_L1;
    pt_size_t bid_vol_L3, ask_vol_L3;
    pt_size_t bid_vol_L5, ask_vol_L5;
    pt_size_t bid_vol_L10, ask_vol_L10;
    double   imb_L1, imb_L3, imb_L5, imb_L10;
    double   wimb;        /* distance-weighted imbalance */
} pt_book_features_t;

void pt_book_features_compute(const pt_book_t *b, pt_book_features_t *out);

/* Combined snapshot for a market. */
typedef struct {
    pt_book_features_t  book;
    pt_flow_window_t    flow[PT_FLOW_NTF];
    /* last trade */
    pt_price_t last_trade_price;
    pt_size_t  last_trade_size;
    int        last_trade_side;
} pt_market_features_t;

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_FEATURES_H */