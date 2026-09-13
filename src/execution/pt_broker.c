#include "execution/pt_broker.h"
#include <string.h>

void pt_broker_init(pt_broker_t *b, const pt_broker_cfg_t *cfg, pt_portfolio_t *port)
{
    if (!b) return;
    memset(b, 0, sizeof(*b));
    if (cfg) b->cfg = *cfg;
    b->portfolio = port;

    pt_sim_queue_cfg_t scfg = {
        .net_latency_ms = b->cfg.latency_submit_ack_ms > 0 ? b->cfg.latency_submit_ack_ms : 1.5,
        .exchange_process_ms = b->cfg.latency_ack_fill_ms > 0 ? b->cfg.latency_ack_fill_ms : 1.0,
        .cancel_latency_ms = b->cfg.latency_cancel_ms > 0 ? b->cfg.latency_cancel_ms : 1.5,
        .queue_model = b->cfg.queue_model ? b->cfg.queue_model : PT_QUEUE_MODEL_REALISTIC,
        .adverse_selection_bps = b->cfg.adverse_selection_bps > 0 ? b->cfg.adverse_selection_bps : 2.0
    };
    pt_sim_queue_init(&b->sim_queue, &scfg);
}

pt_order_id_t pt_broker_submit(pt_broker_t *b, pt_market_id_t market_id,
                               int is_yes, int side, pt_price_t price,
                               pt_size_t size, const pt_book_t *current_book,
                               int strategy, pt_nsec_t now)
{
    if (!b) return 0;
    return pt_sim_queue_submit(&b->sim_queue, market_id, is_yes, side, PT_OTYPE_LIMIT,
                               price, size, current_book, strategy, 0, now);
}

pt_order_id_t pt_broker_submit_ex(pt_broker_t *b, pt_market_id_t market_id,
                                  int is_yes, int side, pt_order_type_t type,
                                  pt_price_t price, pt_size_t size,
                                  const pt_book_t *current_book,
                                  int strategy, uint64_t signal_id,
                                  pt_nsec_t now)
{
    if (!b) return 0;
    return pt_sim_queue_submit(&b->sim_queue, market_id, is_yes, side, type,
                               price, size, current_book, strategy, signal_id, now);
}

int pt_broker_cancel(pt_broker_t *b, pt_order_id_t oid, pt_nsec_t now)
{
    if (!b) return -1;
    return pt_sim_queue_cancel(&b->sim_queue, oid, now);
}

void pt_broker_on_trade(pt_broker_t *b, pt_market_id_t market_id,
                        int is_yes, pt_price_t trade_price, pt_size_t trade_size,
                        int trade_side, pt_nsec_t now)
{
    if (!b) return;
    pt_sim_queue_on_trade(&b->sim_queue, market_id, is_yes, trade_price, trade_size, trade_side, now);
}

void pt_broker_on_level_change(pt_broker_t *b, pt_market_id_t market_id,
                              int is_yes, int side, pt_price_t price,
                              int64_t old_size, int64_t new_size,
                              pt_nsec_t now)
{
    if (!b) return;
    pt_sim_queue_on_level_change(&b->sim_queue, market_id, is_yes, side, price, old_size, new_size, now);
}

typedef struct {
    pt_broker_t         *broker;
    pt_broker_fill_cb_t  user_cb;
    void                *user_ud;
    pt_market_id_t       market_id;
} broker_tick_ctx_t;

static void on_sim_fill_(pt_order_t *o, pt_size_t fill_qty, pt_price_t fill_price,
                         double adverse_sel, void *ud)
{
    broker_tick_ctx_t *ctx = (broker_tick_ctx_t *)ud;
    if (!ctx || !ctx->broker) return;

    if (ctx->broker->portfolio) {
        pt_portfolio_on_fill(ctx->broker->portfolio, ctx->market_id,
                             o->is_yes, o->side, fill_qty, fill_price);
    }

    if (ctx->user_cb) {
        ctx->user_cb(o->id, ctx->market_id, o->is_yes, o->side,
                     fill_qty, fill_price, o->strategy, ctx->user_ud);
    }
}

int pt_broker_tick(pt_broker_t *b, pt_market_id_t market_id,
                   const pt_book_t *yes_book, const pt_book_t *no_book,
                   pt_nsec_t now, pt_broker_fill_cb_t cb, void *ud)
{
    if (!b) return 0;
    broker_tick_ctx_t ctx = {
        .broker = b,
        .user_cb = cb,
        .user_ud = ud,
        .market_id = market_id
    };
    return pt_sim_queue_tick(&b->sim_queue, market_id, yes_book, no_book, now, on_sim_fill_, &ctx);
}

const pt_order_t *pt_broker_get_order(const pt_broker_t *b, pt_order_id_t oid)
{
    if (!b) return NULL;
    for (int i = 0; i < PT_SIM_MAX_ORDERS; i++) {
        if (b->sim_queue.orders[i].id == oid) {
            return &b->sim_queue.orders[i];
        }
    }
    return NULL;
}
