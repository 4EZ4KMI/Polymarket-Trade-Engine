#ifndef PMT_PT_SIM_QUEUE_H
#define PMT_PT_SIM_QUEUE_H

#include "core/ptypes.h"
#include "execution/pt_order.h"
#include "orderbook/pt_book.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PT_SIM_MAX_ORDERS 256

typedef enum {
    PT_QUEUE_MODEL_CONSERVATIVE = 1, /* all cancellations happen BEHIND our order */
    PT_QUEUE_MODEL_REALISTIC    = 2, /* proportional cancellations based on queue position */
    PT_QUEUE_MODEL_AGGRESSIVE   = 3  /* all cancellations happen AHEAD of our order */
} pt_queue_model_t;

typedef struct {
    double           net_latency_ms;        /* one-way network latency */
    double           exchange_process_ms;   /* exchange matching engine queue latency */
    double           cancel_latency_ms;     /* cancel latency */
    pt_queue_model_t queue_model;           /* default = REALISTIC */
    double           adverse_selection_bps; /* price degradation when market sweeps through */
} pt_sim_queue_cfg_t;

typedef struct {
    pt_order_t         orders[PT_SIM_MAX_ORDERS];
    pt_sim_queue_cfg_t cfg;
    pt_order_id_t      next_oid;
} pt_sim_queue_t;

void pt_sim_queue_init(pt_sim_queue_t *sq, const pt_sim_queue_cfg_t *cfg);

/* Place a new order into the simulation queue */
pt_order_id_t pt_sim_queue_submit(pt_sim_queue_t *sq, pt_market_id_t market_id,
                                  int is_yes, int side, pt_order_type_t type,
                                  pt_price_t price, pt_size_t size,
                                  const pt_book_t *current_book,
                                  int strategy, uint64_t signal_id,
                                  pt_nsec_t now);

/* Request order cancellation */
int pt_sim_queue_cancel(pt_sim_queue_t *sq, pt_order_id_t oid, pt_nsec_t now);

/* Notify simulator of a trade event on the market */
void pt_sim_queue_on_trade(pt_sim_queue_t *sq, pt_market_id_t market_id,
                           int is_yes, pt_price_t trade_price, pt_size_t trade_size,
                           int trade_side, pt_nsec_t now);

/* Notify simulator of a book level change */
void pt_sim_queue_on_level_change(pt_sim_queue_t *sq, pt_market_id_t market_id,
                                  int is_yes, int side, pt_price_t price,
                                  int64_t old_size, int64_t new_size,
                                  pt_nsec_t now);

/* Process time tick: handle arrival acks, cancel acks, market sweeps */
typedef void (*pt_sim_fill_callback_t)(pt_order_t *order, pt_size_t fill_qty,
                                       pt_price_t fill_price, double adverse_sel,
                                       void *ud);

int pt_sim_queue_tick(pt_sim_queue_t *sq, pt_market_id_t market_id,
                      const pt_book_t *yes_book, const pt_book_t *no_book,
                      pt_nsec_t now, pt_sim_fill_callback_t cb, void *ud);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_SIM_QUEUE_H */
