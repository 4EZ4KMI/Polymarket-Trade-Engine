#ifndef PMT_PT_BROKER_H
#define PMT_PT_BROKER_H

#include "core/ptypes.h"
#include "orderbook/pt_book.h"
#include "execution/pt_order.h"
#include "portfolio/pt_portfolio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PT_BROKER_MAX_ORDERS 256

typedef struct {
    double latency_submit_ack_ms;
    double latency_ack_fill_ms;
    double slippage_bps;
    int    live_trading_enabled; /* HARD-LOCKED to 0 in paper mode */
} pt_broker_cfg_t;

typedef struct {
    pt_order_t   orders[PT_BROKER_MAX_ORDERS];
    pt_size_t    queue_ahead[PT_BROKER_MAX_ORDERS]; /* shares in front of us at submit */
    pt_nsec_t    scheduled_fill_t[PT_BROKER_MAX_ORDERS];
    pt_broker_cfg_t cfg;
    pt_order_id_t   next_oid;
    pt_portfolio_t *portfolio;
} pt_broker_t;

void pt_broker_init(pt_broker_t *b, const pt_broker_cfg_t *cfg, pt_portfolio_t *port);

/* Submit an order; returns order_id or 0 on error */
pt_order_id_t pt_broker_submit(pt_broker_t *b, pt_market_id_t market_id,
                               int is_yes, int side, pt_price_t price,
                               pt_size_t size, const pt_book_t *current_book,
                               int strategy, pt_nsec_t now);

int  pt_broker_cancel(pt_broker_t *b, pt_order_id_t oid, pt_nsec_t now);

/* Broker tick: evaluate resting orders against current book/trades and fire fills */
typedef void (*pt_broker_fill_cb_t)(pt_order_id_t oid, pt_market_id_t market_id,
                                    int is_yes, int side, pt_size_t filled_shares,
                                    pt_price_t fill_price, double pnl,
                                    void *ud);

int  pt_broker_tick(pt_broker_t *b, pt_market_id_t market_id,
                    const pt_book_t *yes_book, const pt_book_t *no_book,
                    pt_nsec_t now, pt_broker_fill_cb_t cb, void *ud);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_BROKER_H */