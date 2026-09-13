#include "execution/pt_broker.h"
#include <string.h>

void pt_broker_init(pt_broker_t *b, const pt_broker_cfg_t *cfg, pt_portfolio_t *port)
{
    memset(b, 0, sizeof(*b));
    if (cfg) b->cfg = *cfg;
    b->portfolio = port;
    b->next_oid = 10000;
}

pt_order_id_t pt_broker_submit(pt_broker_t *b, pt_market_id_t market_id,
                               int is_yes, int side, pt_price_t price,
                               pt_size_t size, const pt_book_t *current_book,
                               int strategy, pt_nsec_t now)
{
    if (b == NULL || size == 0 || price <= 0) return 0;
    int slot = -1;
    for (int i = 0; i < PT_BROKER_MAX_ORDERS; i++) {
        if (b->orders[i].state == PT_OSTATE_CREATED ||
            b->orders[i].state == PT_OSTATE_FILLED  ||
            b->orders[i].state == PT_OSTATE_CANCELLED ||
            b->orders[i].state == PT_OSTATE_REJECTED) {
            slot = i; break;
        }
    }
    if (slot < 0) return 0; /* broker full */

    pt_order_t *o = &b->orders[slot];
    memset(o, 0, sizeof(*o));
    o->id = b->next_oid++;
    o->is_yes = is_yes;
    o->side = side;
    o->price = price;
    o->original_size = size;
    o->filled_size = 0;
    o->state = PT_OSTATE_LIVE;
    o->strategy = strategy;
    o->submit_t = now;
    o->ack_t = now + (pt_nsec_t)(b->cfg.latency_submit_ack_ms * 1000000.0);
    o->last_update_t = now;

    /* compute queue ahead at current price */
    pt_size_t qahead = 0;
    if (current_book) {
        const pt_side_book_t *sb = (side == PT_SIDE_BID) ? &current_book->bids : &current_book->asks;
        for (int i = 0; i < sb->count; i++) {
            if (sb->levels[i].price == price) {
                qahead = sb->levels[i].size;
                break;
            }
        }
    }
    b->queue_ahead[slot] = qahead;
    b->scheduled_fill_t[slot] = o->ack_t + (pt_nsec_t)(b->cfg.latency_ack_fill_ms * 1000000.0);
    return o->id;
}

int pt_broker_cancel(pt_broker_t *b, pt_order_id_t oid, pt_nsec_t now)
{
    if (!b) return -1;
    for (int i = 0; i < PT_BROKER_MAX_ORDERS; i++) {
        if (b->orders[i].id == oid &&
            (b->orders[i].state == PT_OSTATE_LIVE || b->orders[i].state == PT_OSTATE_PARTIAL)) {
            b->orders[i].state = PT_OSTATE_CANCELLED;
            b->orders[i].last_update_t = now;
            return 0;
        }
    }
    return -1;
}

int pt_broker_tick(pt_broker_t *b, pt_market_id_t market_id,
                    const pt_book_t *yes_book, const pt_book_t *no_book,
                    pt_nsec_t now, pt_broker_fill_cb_t cb, void *ud)
{
    if (!b) return 0;
    int filled_any = 0;
    for (int i = 0; i < PT_BROKER_MAX_ORDERS; i++) {
        pt_order_t *o = &b->orders[i];
        if (o->state != PT_OSTATE_LIVE && o->state != PT_OSTATE_PARTIAL)
            continue;
        if (now < o->ack_t)
            continue; /* still in-flight submit */

        const pt_book_t *bk = o->is_yes ? yes_book : no_book;
        if (!bk) continue;

        /* Check crossing: if order is buying and best ask <= our price (or schedule ready) */
        pt_price_t best_opp = 0;
        pt_size_t opp_sz = 0;
        int cross = 0;
        if (o->side == PT_SIDE_BID) {
            if (pt_book_best_ask(bk, &best_opp, &opp_sz) == 0 && best_opp <= o->price) {
                cross = 1;
            }
        } else {
            if (pt_book_best_bid(bk, &best_opp, &opp_sz) == 0 && best_opp >= o->price) {
                cross = 1;
            }
        }

        if (cross || (now >= b->scheduled_fill_t[i])) {
            pt_size_t remaining = o->original_size - o->filled_size;
            pt_size_t fill_qty = remaining; /* fill whole remaining in paper */
            pt_price_t fill_p = cross ? best_opp : o->price;

            o->filled_size += fill_qty;
            o->state = PT_OSTATE_FILLED;
            o->last_update_t = now;

            if (b->portfolio) {
                pt_portfolio_on_fill(b->portfolio, market_id, o->is_yes,
                                     o->side, fill_qty, fill_p);
            }

            if (cb) cb(o->id, market_id, o->is_yes, o->side, fill_qty, fill_p, 0.0, ud);
            filled_any++;
        }
    }
    return filled_any;
}