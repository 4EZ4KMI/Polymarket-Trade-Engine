#include "execution/pt_sim_queue.h"
#include <string.h>
#include <math.h>

void pt_sim_queue_init(pt_sim_queue_t *sq, const pt_sim_queue_cfg_t *cfg)
{
    if (!sq) return;
    memset(sq, 0, sizeof(*sq));
    if (cfg) {
        sq->cfg = *cfg;
    } else {
        sq->cfg.net_latency_ms = 2.0;
        sq->cfg.exchange_process_ms = 1.0;
        sq->cfg.cancel_latency_ms = 2.0;
        sq->cfg.queue_model = PT_QUEUE_MODEL_REALISTIC;
        sq->cfg.adverse_selection_bps = 2.0;
    }
    if (sq->cfg.queue_model == 0) {
        sq->cfg.queue_model = PT_QUEUE_MODEL_REALISTIC;
    }
    sq->next_oid = 100000;
}

pt_order_id_t pt_sim_queue_submit(pt_sim_queue_t *sq, pt_market_id_t market_id,
                                  int is_yes, int side, pt_order_type_t type,
                                  pt_price_t price, pt_size_t size,
                                  const pt_book_t *current_book,
                                  int strategy, uint64_t signal_id,
                                  pt_nsec_t now)
{
    if (!sq || size == 0 || price <= 0) return 0;
    
    int slot = -1;
    for (int i = 0; i < PT_SIM_MAX_ORDERS; i++) {
        pt_order_state_t st = sq->orders[i].state;
        if (st == PT_OSTATE_CREATED || st == PT_OSTATE_FILLED ||
            st == PT_OSTATE_CANCELLED || st == PT_OSTATE_REJECTED ||
            st == PT_OSTATE_EXPIRED) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return 0; /* queue pool full */

    pt_order_t *o = &sq->orders[slot];
    memset(o, 0, sizeof(*o));
    o->id = sq->next_oid++;
    o->signal_id = signal_id;
    o->market_id = market_id;
    o->is_yes = is_yes;
    o->side = side;
    o->type = type;
    o->price = price;
    o->original_size = size;
    o->remaining_size = size;
    o->filled_size = 0;
    o->strategy = strategy;
    o->created_t = now;
    o->submit_t = now;
    
    pt_nsec_t net_ns = (pt_nsec_t)(sq->cfg.net_latency_ms * 1000000.0);
    pt_nsec_t proc_ns = (pt_nsec_t)(sq->cfg.exchange_process_ms * 1000000.0);
    o->arrival_t = now + net_ns + proc_ns;
    o->ack_t = o->arrival_t + net_ns;
    o->state = PT_OSTATE_PENDING_SUBMIT;
    o->last_update_t = now;

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
    o->queue_ahead = qahead;
    o->initial_queue = qahead;

    return o->id;
}

int pt_sim_queue_cancel(pt_sim_queue_t *sq, pt_order_id_t oid, pt_nsec_t now)
{
    if (!sq) return -1;
    for (int i = 0; i < PT_SIM_MAX_ORDERS; i++) {
        pt_order_t *o = &sq->orders[i];
        if (o->id == oid &&
            (o->state == PT_OSTATE_PENDING_SUBMIT ||
             o->state == PT_OSTATE_LIVE ||
             o->state == PT_OSTATE_PARTIAL)) {
            
            pt_nsec_t cancel_ns = (pt_nsec_t)(sq->cfg.cancel_latency_ms * 1000000.0);
            pt_nsec_t net_ns = (pt_nsec_t)(sq->cfg.net_latency_ms * 1000000.0);
            o->cancel_req_t = now;
            o->cancel_ack_t = now + cancel_ns + net_ns;
            o->state = PT_OSTATE_PENDING_CANCEL;
            o->last_update_t = now;
            return 0;
        }
    }
    return -1;
}

void pt_sim_queue_on_trade(pt_sim_queue_t *sq, pt_market_id_t market_id,
                           int is_yes, pt_price_t trade_price, pt_size_t trade_size,
                           int trade_side, pt_nsec_t now)
{
    if (!sq || trade_size == 0) return;

    for (int i = 0; i < PT_SIM_MAX_ORDERS; i++) {
        pt_order_t *o = &sq->orders[i];
        if (o->market_id != market_id || o->is_yes != is_yes)
            continue;
        if (o->state == PT_OSTATE_PENDING_SUBMIT && now >= o->arrival_t) {
            o->state = PT_OSTATE_LIVE;
        }
        if (o->state != PT_OSTATE_LIVE && o->state != PT_OSTATE_PARTIAL &&
            o->state != PT_OSTATE_PENDING_CANCEL)
            continue;
        if (now < o->arrival_t)
            continue;

        int interacts = 0;
        if (o->side == PT_SIDE_BID) {
            if (trade_side == PT_SIDE_ASK && trade_price <= o->price) interacts = 1;
        } else {
            if (trade_side == PT_SIDE_BID && trade_price >= o->price) interacts = 1;
        }

        if (interacts) {
            if (o->queue_ahead >= trade_size) {
                o->queue_ahead -= trade_size;
            } else {
                pt_size_t excess_trade = trade_size - o->queue_ahead;
                o->queue_ahead = 0;
                pt_size_t fill_qty = (excess_trade < o->remaining_size) ? excess_trade : o->remaining_size;
                if (fill_qty > 0) {
                    o->filled_size += fill_qty;
                    o->remaining_size -= fill_qty;
                    o->cum_cost_scaled += (int64_t)fill_qty * (int64_t)o->price;
                    o->avg_fill_price = (pt_price_t)(o->cum_cost_scaled / o->filled_size);
                    o->last_update_t = now;
                    if (o->remaining_size == 0) {
                        o->state = PT_OSTATE_FILLED;
                    } else {
                        o->state = PT_OSTATE_PARTIAL;
                    }
                }
            }
        }
    }
}

void pt_sim_queue_on_level_change(pt_sim_queue_t *sq, pt_market_id_t market_id,
                                  int is_yes, int side, pt_price_t price,
                                  int64_t old_size, int64_t new_size,
                                  pt_nsec_t now)
{
    if (!sq || old_size <= new_size) return;
    int64_t delta_reduction = old_size - new_size;

    for (int i = 0; i < PT_SIM_MAX_ORDERS; i++) {
        pt_order_t *o = &sq->orders[i];
        if (o->market_id != market_id || o->is_yes != is_yes || o->side != side || o->price != price)
            continue;
        if (o->state == PT_OSTATE_PENDING_SUBMIT && now >= o->arrival_t) {
            o->state = PT_OSTATE_LIVE;
        }
        if (o->state != PT_OSTATE_LIVE && o->state != PT_OSTATE_PARTIAL &&
            o->state != PT_OSTATE_PENDING_CANCEL)
            continue;
        if (now < o->arrival_t)
            continue;

        if (sq->cfg.queue_model == PT_QUEUE_MODEL_CONSERVATIVE) {
            continue;
        } else if (sq->cfg.queue_model == PT_QUEUE_MODEL_AGGRESSIVE) {
            if ((pt_size_t)delta_reduction >= o->queue_ahead) {
                o->queue_ahead = 0;
            } else {
                o->queue_ahead -= (pt_size_t)delta_reduction;
            }
        } else {
            if (old_size > 0 && o->queue_ahead > 0) {
                double prop = (double)o->queue_ahead / (double)old_size;
                pt_size_t reduced = (pt_size_t)round((double)delta_reduction * prop);
                if (reduced > o->queue_ahead) reduced = o->queue_ahead;
                o->queue_ahead -= reduced;
            }
        }
    }
}

int pt_sim_queue_tick(pt_sim_queue_t *sq, pt_market_id_t market_id,
                      const pt_book_t *yes_book, const pt_book_t *no_book,
                      pt_nsec_t now, pt_sim_fill_callback_t cb, void *ud)
{
    if (!sq) return 0;
    int fills_count = 0;

    for (int i = 0; i < PT_SIM_MAX_ORDERS; i++) {
        pt_order_t *o = &sq->orders[i];
        if (o->market_id != market_id) continue;

        if (o->state == PT_OSTATE_PENDING_SUBMIT && now >= o->arrival_t) {
            o->state = PT_OSTATE_LIVE;
            o->last_update_t = now;
        }

        if (o->state == PT_OSTATE_PENDING_CANCEL && now >= o->cancel_ack_t) {
            o->state = PT_OSTATE_CANCELLED;
            o->last_update_t = now;
            continue;
        }

        if (o->state != PT_OSTATE_LIVE && o->state != PT_OSTATE_PARTIAL &&
            o->state != PT_OSTATE_PENDING_CANCEL)
            continue;

        if (now < o->arrival_t) continue;

        const pt_book_t *bk = o->is_yes ? yes_book : no_book;
        if (!bk) continue;

        if (o->type == PT_OTYPE_MARKET || o->type == PT_OTYPE_IOC) {
            int64_t avg_p = 0;
            int lv_used = 0;
            int opp_side = (o->side == PT_SIDE_BID) ? PT_SIDE_ASK : PT_SIDE_BID;
            pt_size_t can_fill = pt_book_walk(bk, opp_side, o->remaining_size, &avg_p, &lv_used);
            
            if (can_fill > 0) {
                o->filled_size += can_fill;
                o->remaining_size -= can_fill;
                o->cum_cost_scaled += (int64_t)can_fill * avg_p;
                o->avg_fill_price = (pt_price_t)(o->cum_cost_scaled / o->filled_size);
                o->last_update_t = now;
                
                if (cb) cb(o, can_fill, (pt_price_t)avg_p, 0.0, ud);
                fills_count++;
            }
            if (o->remaining_size == 0) {
                o->state = PT_OSTATE_FILLED;
            } else {
                o->state = PT_OSTATE_CANCELLED;
            }
            continue;
        }

        pt_price_t opp_best = 0;
        pt_size_t opp_sz = 0;
        int swept = 0;

        if (o->side == PT_SIDE_BID) {
            if (pt_book_best_ask(bk, &opp_best, &opp_sz) == 0 && opp_best < o->price) {
                swept = 1;
            }
        } else {
            if (pt_book_best_bid(bk, &opp_best, &opp_sz) == 0 && opp_best > o->price) {
                swept = 1;
            }
        }

        if (swept) {
            pt_size_t fill_qty = o->remaining_size;
            double adv = (double)o->price * (sq->cfg.adverse_selection_bps / 10000.0);
            pt_price_t fill_p = o->price;

            o->filled_size += fill_qty;
            o->remaining_size = 0;
            o->cum_cost_scaled += (int64_t)fill_qty * (int64_t)fill_p;
            o->avg_fill_price = (pt_price_t)(o->cum_cost_scaled / o->filled_size);
            o->adverse_selection += adv * (double)fill_qty / (double)PT_PRICE_SCALE;
            o->state = PT_OSTATE_FILLED;
            o->last_update_t = now;

            if (cb) cb(o, fill_qty, fill_p, adv, ud);
            fills_count++;
        }
    }

    return fills_count;
}

