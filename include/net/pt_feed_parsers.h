#ifndef PMT_PT_FEED_PARSERS_H
#define PMT_PT_FEED_PARSERS_H

#include "orderbook/pt_book.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Polymarket CLOB book stream parser --- */
typedef struct {
    char        asset_id[80];
    int         is_snapshot;
    int         side;          /* PT_SIDE_BID / PT_SIDE_ASK */
    pt_price_t  price;         /* scaled */
    pt_size_t   size;          /* shares */
    pt_seq_t    seq;
    pt_nsec_t   timestamp_ns;
} pt_poly_delta_t;

/* --- Polymarket CLOB trade parser --- */
typedef struct {
    char        asset_id[80];
    int         side;          /* PT_SIDE_BID / PT_SIDE_ASK */
    pt_price_t  price;         /* scaled */
    pt_size_t   size;          /* shares */
    pt_nsec_t   timestamp_ns;
} pt_poly_trade_t;

/* Parses a raw Polymarket book delta message into struct. Returns 1 if ok, 0 if skipped. */
int pt_parse_polymarket_book_msg(const char *msg, size_t len, pt_poly_delta_t *out);
int pt_parse_polymarket_trade_msg(const char *msg, size_t len, pt_poly_trade_t *out);

/* --- Binance BTC ticker/aggTrade parser --- */
typedef struct {
    double     price;
    double     qty;
    int        is_buyer_maker; /* 1 = sell aggressor, 0 = buy aggressor */
    pt_nsec_t  event_time_ns;
} pt_binance_trade_t;

/* Parses a raw Binance @trade or @aggTrade or @bookTicker JSON payload. */
int pt_parse_binance_trade(const char *msg, size_t len, pt_binance_trade_t *out);
int pt_parse_binance_bookticker(const char *msg, size_t len, double *bid, double *ask, pt_nsec_t *ts_ns);

/* --- Real Polymarket Discovery & Resolution messages --- */
typedef struct {
    uint64_t market_id;
    char     condition_id[68];
    char     slug[64];
    char     yes_token_id[72];
    char     no_token_id[72];
    double   strike;
    uint64_t start_time_ms;
    uint64_t end_time_ms;
} pt_market_discovery_msg_t;

typedef struct {
    uint64_t market_id;
    char     condition_id[68];
    int      winning_is_yes;
    double   resolution_price;
    uint64_t resolution_time_ms;
} pt_market_resolution_msg_t;

int pt_parse_market_discovery_msg(const char *msg, size_t len, pt_market_discovery_msg_t *out);
int pt_parse_market_resolution_msg(const char *msg, size_t len, pt_market_resolution_msg_t *out);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_FEED_PARSERS_H */
