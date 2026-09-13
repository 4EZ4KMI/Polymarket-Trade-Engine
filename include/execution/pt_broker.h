#ifndef PMT_PT_BROKER_H
#define PMT_PT_BROKER_H

#include "core/ptypes.h"
#include "orderbook/pt_book.h"
#include "execution/pt_order.h"
#include "execution/pt_sim_queue.h"
#include "portfolio/pt_portfolio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PT_BROKER_MAX_ORDERS PT_SIM_MAX_ORDERS

typedef struct {
    double           latency_submit_ack_ms;
    double           latency_ack_fill_ms;
    double           latency_cancel_ms;
    pt_queue_model_t queue_model;
    double           slippage_bps;
    double           adverse_selection_bps;
    int              live_trading_enabled; /* HARD-LOCKED to 0 in paper mode */
} pt_broker_cfg_t;

typedef struct {
    pt_sim_queue_t   sim_queue;
    pt_broker_cfg_t  cfg;
    pt_portfolio_t  *portfolio;
} pt_broker_t;

void pt_broker_init(pt_broker_t *b, const pt_broker_cfg_t *cfg, pt_portfolio_t *port);

/* Submit limit/market order; returns order_id or 0 on error */
pt_order_id_t pt_broker_submit(pt_broker_t *b, pt_market_id_t market_id,
                               int is_yes, int side, pt_price_t price,
                               pt_size_t size, const pt_book_t *current_book,
                               int strategy, pt_nsec_t now);

pt_order_id_t pt_broker_submit_ex(pt_broker_t *b, pt_market_id_t market_id,
                                  int is_yes, int side, pt_order_type_t type,
                                  pt_price_t price, pt_size_t size,
                                  const pt_book_t *current_book,
                                  int strategy, uint64_t signal_id,
                                  pt_nsec_t now);

int  pt_broker_cancel(pt_broker_t *b, pt_order_id_t oid, pt_nsec_t now);

/* Notify market data trade / level change to queue simulator */
void pt_broker_on_trade(pt_broker_t *b, pt_market_id_t market_id,
                        int is_yes, pt_price_t trade_price, pt_size_t trade_size,
                        int trade_side, pt_nsec_t now);

void pt_broker_on_level_change(pt_broker_t *b, pt_market_id_t market_id,
                              int is_yes, int side, pt_price_t price,
                              int64_t old_size, int64_t new_size,
                              pt_nsec_t now);

/* Broker tick: evaluate resting orders against current book/trades and fire fills */
typedef void (*pt_broker_fill_cb_t)(const pt_order_t *order, pt_size_t filled_shares,
                                    pt_price_t fill_price, void *ud);

int  pt_broker_tick(pt_broker_t *b, pt_market_id_t market_id,
                    const pt_book_t *yes_book, const pt_book_t *no_book,
                    pt_nsec_t now, pt_broker_fill_cb_t cb, void *ud);

const pt_order_t *pt_broker_get_order(const pt_broker_t *b, pt_order_id_t oid);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_BROKER_H */