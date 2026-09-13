#ifndef PMT_PT_FEED_PARSERS_H
#define PMT_PT_FEED_PARSERS_H

#include "orderbook/pt_book.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PT_POLY_MAX_SNAP_LEVELS 128
#define PT_POLY_MAX_PRICE_CHANGES 128

/* --- Polymarket CLOB native book snapshot --- */
typedef struct {
    char        asset_id[80];
    char        market[80];
    char        hash[80];
    pt_nsec_t   timestamp_ns;
    int         bid_count;
    pt_level_t  bids[PT_POLY_MAX_SNAP_LEVELS];
    int         ask_count;
    pt_level_t  asks[PT_POLY_MAX_SNAP_LEVELS];
} pt_poly_book_snap_t;

/* --- Polymarket CLOB native price_change delta --- */
typedef struct {
    char        asset_id[80];
    int         side;          /* PT_SIDE_BID / PT_SIDE_ASK */
    pt_price_t  price;         /* scaled */
    pt_size_t   size;          /* shares (0 = level removal) */
    pt_price_t  best_bid;
    pt_price_t  best_ask;
} pt_poly_price_change_entry_t;

typedef struct {
    char                         market[80];
    pt_nsec_t                    timestamp_ns;
    int                          count;
    pt_poly_price_change_entry_t changes[PT_POLY_MAX_PRICE_CHANGES];
} pt_poly_price_changes_t;

/* --- Polymarket CLOB legacy single delta struct (for backwards compatibility) --- */
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
    char        market[80];
    int         side;          /* PT_SIDE_BID / PT_SIDE_ASK */
    pt_price_t  price;         /* scaled */
    pt_size_t   size;          /* shares */
    pt_nsec_t   timestamp_ns;  /* venue timestamp */
} pt_poly_trade_t;

/* --- Binance BTC ticker/aggTrade parser --- */
typedef struct {
    double     price;
    double     qty;
    int        is_buyer_maker; /* 1 = sell aggressor, 0 = buy aggressor */
    pt_nsec_t  event_time_ns;
} pt_binance_trade_t;

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
    int      duration_sec;
    int      eligible_for_strategy_a;
    int      eligible_for_strategy_b;
} pt_market_discovery_msg_t;

typedef struct {
    uint64_t market_id;
    char     condition_id[68];
    char     winning_outcome[16];   /* "YES", "NO" */
    char     winning_asset_id[72];  /* Token ID of winning outcome */
    int      has_winning_outcome;
    int      winning_is_yes;        /* 1 = YES, 0 = NO, -1 = unknown */
    double   resolution_price;
    uint64_t resolution_time_ms;
} pt_market_resolution_msg_t;

/* Parsing functions */
int pt_parse_polymarket_book_snap(const char *msg, size_t len, pt_poly_book_snap_t *out);
int pt_parse_polymarket_price_changes(const char *msg, size_t len, pt_poly_price_changes_t *out);
int pt_parse_polymarket_book_msg(const char *msg, size_t len, pt_poly_delta_t *out);
int pt_parse_polymarket_trade_msg(const char *msg, size_t len, pt_poly_trade_t *out);
int pt_parse_binance_trade(const char *msg, size_t len, pt_binance_trade_t *out);
int pt_parse_binance_bookticker(const char *msg, size_t len, double *bid, double *ask, pt_nsec_t *ts_ns);
int pt_parse_market_discovery_msg(const char *msg, size_t len, pt_market_discovery_msg_t *out);
int pt_parse_market_resolution_msg(const char *msg, size_t len, pt_market_resolution_msg_t *out);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_FEED_PARSERS_H */
