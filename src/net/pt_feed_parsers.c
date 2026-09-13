#include "net/pt_feed_parsers.h"
#include "util/pt_json.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <strings.h>

static int compare_levels_asc_(const void *a, const void *b)
{
    const pt_level_t *la = (const pt_level_t *)a;
    const pt_level_t *lb = (const pt_level_t *)b;
    if (la->price < lb->price) return -1;
    if (la->price > lb->price) return 1;
    return 0;
}

int pt_parse_polymarket_book_snap(const char *msg, size_t len, pt_poly_book_snap_t *out)
{
    if (!msg || len == 0 || !out) return 0;
    memset(out, 0, sizeof(*out));

    pt_json_tok_t et;
    if (pt_json_get(msg, len, "event_type", &et)) {
        char ev[32]; pt_json_tok_str(&et, ev, sizeof(ev));
        if (strcmp(ev, "book") != 0) return 0;
    } else {
        return 0;
    }

    pt_json_tok_t astok, mktok, htok, tstok, bidstok, askstok;
    if (pt_json_get(msg, len, "asset_id", &astok)) {
        pt_json_tok_str(&astok, out->asset_id, sizeof(out->asset_id));
    }
    if (pt_json_get(msg, len, "market", &mktok)) {
        pt_json_tok_str(&mktok, out->market, sizeof(out->market));
    }
    if (pt_json_get(msg, len, "hash", &htok)) {
        pt_json_tok_str(&htok, out->hash, sizeof(out->hash));
    }
    if (pt_json_get(msg, len, "timestamp", &tstok)) {
        int64_t ts_ms = 0; pt_json_tok_int64(&tstok, &ts_ms);
        out->timestamp_ns = (pt_nsec_t)ts_ms * 1000000ULL;
    }

    /* Parse bids array */
    if (pt_json_get(msg, len, "bids", &bidstok) && bidstok.type == PT_JSON_ARRAY) {
        const char *iter = NULL;
        pt_json_tok_t elem;
        while ((iter = pt_json_array_next(bidstok.p, bidstok.len, iter, &elem)) != NULL) {
            if (out->bid_count >= PT_POLY_MAX_SNAP_LEVELS) break;
            pt_json_tok_t ptok, stok;
            double p = 0.0, sz = 0.0;
            if (pt_json_get(elem.p, elem.len, "price", &ptok)) {
                pt_json_tok_double(&ptok, &p);
            }
            if (pt_json_get(elem.p, elem.len, "size", &stok)) {
                pt_json_tok_double(&stok, &sz);
            }
            if (p > 0.0 && sz > 0.0) {
                out->bids[out->bid_count].price = (pt_price_t)(p * (double)PT_PRICE_SCALE + 0.5);
                out->bids[out->bid_count].size  = (pt_size_t)(sz + 0.5);
                out->bid_count++;
            }
        }
        if (out->bid_count > 1) {
            qsort(out->bids, out->bid_count, sizeof(pt_level_t), compare_levels_asc_);
        }
    }

    /* Parse asks array */
    if (pt_json_get(msg, len, "asks", &askstok) && askstok.type == PT_JSON_ARRAY) {
        const char *iter = NULL;
        pt_json_tok_t elem;
        while ((iter = pt_json_array_next(askstok.p, askstok.len, iter, &elem)) != NULL) {
            if (out->ask_count >= PT_POLY_MAX_SNAP_LEVELS) break;
            pt_json_tok_t ptok, stok;
            double p = 0.0, sz = 0.0;
            if (pt_json_get(elem.p, elem.len, "price", &ptok)) {
                pt_json_tok_double(&ptok, &p);
            }
            if (pt_json_get(elem.p, elem.len, "size", &stok)) {
                pt_json_tok_double(&stok, &sz);
            }
            if (p > 0.0 && sz > 0.0) {
                out->asks[out->ask_count].price = (pt_price_t)(p * (double)PT_PRICE_SCALE + 0.5);
                out->asks[out->ask_count].size  = (pt_size_t)(sz + 0.5);
                out->ask_count++;
            }
        }
        if (out->ask_count > 1) {
            qsort(out->asks, out->ask_count, sizeof(pt_level_t), compare_levels_asc_);
        }
    }

    return (out->asset_id[0] != '\0') ? 1 : 0;
}
int pt_parse_polymarket_price_changes(const char *msg, size_t len, pt_poly_price_changes_t *out)
{
    if (!msg || len == 0 || !out) return 0;
    memset(out, 0, sizeof(*out));

    pt_json_tok_t et;
    if (pt_json_get(msg, len, "event_type", &et)) {
        char ev[32]; pt_json_tok_str(&et, ev, sizeof(ev));
        if (strcmp(ev, "price_change") != 0) return 0;
    } else {
        return 0;
    }

    pt_json_tok_t mktok, tstok, pctok;
    if (pt_json_get(msg, len, "market", &mktok)) {
        pt_json_tok_str(&mktok, out->market, sizeof(out->market));
    }
    if (pt_json_get(msg, len, "timestamp", &tstok)) {
        int64_t ts_ms = 0; pt_json_tok_int64(&tstok, &ts_ms);
        out->timestamp_ns = (pt_nsec_t)ts_ms * 1000000ULL;
    }

    char top_asset[80] = {0};
    pt_json_tok_t top_astok;
    if (pt_json_get(msg, len, "asset_id", &top_astok)) {
        pt_json_tok_str(&top_astok, top_asset, sizeof(top_asset));
    }

    if (pt_json_get(msg, len, "price_changes", &pctok) && pctok.type == PT_JSON_ARRAY) {
        const char *iter = NULL;
        pt_json_tok_t elem;
        while ((iter = pt_json_array_next(pctok.p, pctok.len, iter, &elem)) != NULL) {
            if (out->count >= PT_POLY_MAX_PRICE_CHANGES) break;
            pt_poly_price_change_entry_t *entry = &out->changes[out->count];
            memset(entry, 0, sizeof(*entry));

            pt_json_tok_t astok, ptok, stok, sdtok, bbtok, batok;
            if (pt_json_get(elem.p, elem.len, "asset_id", &astok)) {
                pt_json_tok_str(&astok, entry->asset_id, sizeof(entry->asset_id));
            } else if (top_asset[0] != '\0') {
                strncpy(entry->asset_id, top_asset, sizeof(entry->asset_id) - 1);
            }

            if (pt_json_get(elem.p, elem.len, "side", &sdtok)) {
                char s[16]; pt_json_tok_str(&sdtok, s, sizeof(s));
                entry->side = (s[0] == 'B' || s[0] == 'b') ? PT_SIDE_BID : PT_SIDE_ASK;
            }

            if (pt_json_get(elem.p, elem.len, "price", &ptok)) {
                double p = 0.0; pt_json_tok_double(&ptok, &p);
                entry->price = (pt_price_t)(p * (double)PT_PRICE_SCALE + 0.5);
            }

            if (pt_json_get(elem.p, elem.len, "size", &stok)) {
                double sz = 0.0; pt_json_tok_double(&stok, &sz);
                entry->size = (pt_size_t)(sz + 0.5);
            }

            if (pt_json_get(elem.p, elem.len, "best_bid", &bbtok)) {
                double bb = 0.0; pt_json_tok_double(&bbtok, &bb);
                entry->best_bid = (pt_price_t)(bb * (double)PT_PRICE_SCALE + 0.5);
            }
            if (pt_json_get(elem.p, elem.len, "best_ask", &batok)) {
                double ba = 0.0; pt_json_tok_double(&batok, &ba);
                entry->best_ask = (pt_price_t)(ba * (double)PT_PRICE_SCALE + 0.5);
            }

            if (entry->asset_id[0] != '\0' && entry->price > 0) {
                out->count++;
            }
        }
    }

    return (out->count > 0) ? 1 : 0;
}


int pt_parse_polymarket_book_msg(const char *msg, size_t len, pt_poly_delta_t *out)
{
    if (!msg || len == 0 || !out) return 0;
    memset(out, 0, sizeof(*out));

    pt_json_tok_t et, ptok, stok, sdtok, astok, tstok;
    if (pt_json_get(msg, len, "event_type", &et)) {
        char ev[32]; pt_json_tok_str(&et, ev, sizeof(ev));
        if (strcmp(ev, "book") != 0 && strcmp(ev, "price_change") != 0)
            return 0;
    }

    if (pt_json_get(msg, len, "asset_id", &astok)) {
        pt_json_tok_str(&astok, out->asset_id, sizeof(out->asset_id));
    }

    if (pt_json_get(msg, len, "side", &sdtok)) {
        char s[8]; pt_json_tok_str(&sdtok, s, sizeof(s));
        out->side = (s[0] == 'B' || s[0] == 'b') ? PT_SIDE_BID : PT_SIDE_ASK;
    }

    if (pt_json_get(msg, len, "price", &ptok)) {
        double p = 0; pt_json_tok_double(&ptok, &p);
        out->price = (pt_price_t)(p * (double)PT_PRICE_SCALE + 0.5);
    }

    if (pt_json_get(msg, len, "size", &stok)) {
        double sz = 0; pt_json_tok_double(&stok, &sz);
        out->size = (pt_size_t)(sz + 0.5);
    }

    if (pt_json_get(msg, len, "timestamp", &tstok)) {
        int64_t ts_ms = 0; pt_json_tok_int64(&tstok, &ts_ms);
        out->timestamp_ns = (pt_nsec_t)ts_ms * 1000000ULL;
    }
    return (out->price > 0 || out->size == 0) ? 1 : 0;
}

int pt_parse_polymarket_trade_msg(const char *msg, size_t len, pt_poly_trade_t *out)
{
    if (!msg || len == 0 || !out) return 0;
    memset(out, 0, sizeof(*out));

    pt_json_tok_t et, ptok, stok, sdtok, astok, tstok;
    if (pt_json_get(msg, len, "event_type", &et)) {
        char ev[32]; pt_json_tok_str(&et, ev, sizeof(ev));
        if (strcmp(ev, "trade") != 0 && strcmp(ev, "last_trade_price") != 0 && strcmp(ev, "match") != 0)
            return 0;
    } else {
        return 0;
    }

    if (pt_json_get(msg, len, "asset_id", &astok)) {
        pt_json_tok_str(&astok, out->asset_id, sizeof(out->asset_id));
    }

    if (pt_json_get(msg, len, "side", &sdtok)) {
        char s[8]; pt_json_tok_str(&sdtok, s, sizeof(s));
        out->side = (s[0] == 'B' || s[0] == 'b') ? PT_SIDE_BID : PT_SIDE_ASK;
    } else {
        out->side = PT_SIDE_BID;
    }

    if (pt_json_get(msg, len, "price", &ptok)) {
        double p = 0; pt_json_tok_double(&ptok, &p);
        out->price = (pt_price_t)(p * (double)PT_PRICE_SCALE + 0.5);
    }

    if (pt_json_get(msg, len, "size", &stok)) {
        double sz = 0; pt_json_tok_double(&stok, &sz);
        out->size = (pt_size_t)(sz + 0.5);
    }

    if (pt_json_get(msg, len, "timestamp", &tstok)) {
        int64_t ts_ms = 0; pt_json_tok_int64(&tstok, &ts_ms);
        out->timestamp_ns = (pt_nsec_t)ts_ms * 1000000ULL;
    }
    return (out->price > 0 && out->size > 0 && out->asset_id[0] != '\0') ? 1 : 0;
}

int pt_parse_binance_trade(const char *msg, size_t len, pt_binance_trade_t *out)
{
    if (!msg || len == 0 || !out) return 0;
    memset(out, 0, sizeof(*out));
    pt_json_tok_t pt, qt, mt, tt;
    if (!pt_json_get(msg, len, "p", &pt) ||
        !pt_json_get(msg, len, "q", &qt)) {
        return 0;
    }
    pt_json_tok_double(&pt, &out->price);
    pt_json_tok_double(&qt, &out->qty);

    if (pt_json_get(msg, len, "m", &mt)) {
        out->is_buyer_maker = (mt.p && mt.p[0] == 't') ? 1 : 0;
    }
    if (pt_json_get(msg, len, "E", &tt) || pt_json_get(msg, len, "T", &tt)) {
        int64_t ms = 0; pt_json_tok_int64(&tt, &ms);
        out->event_time_ns = (pt_nsec_t)ms * 1000000ULL;
    }
    return (out->price > 0.0) ? 1 : 0;
}

int pt_parse_binance_bookticker(const char *msg, size_t len, double *bid, double *ask, pt_nsec_t *ts_ns)
{
    if (!msg || len == 0) return 0;
    pt_json_tok_t bt, at;
    if (pt_json_get(msg, len, "b", &bt) && pt_json_get(msg, len, "a", &at)) {
        if (bid) pt_json_tok_double(&bt, bid);
        if (ask) pt_json_tok_double(&at, ask);
        return 1;
    }
    return 0;
}

int pt_parse_market_discovery_msg(const char *msg, size_t len, pt_market_discovery_msg_t *out)
{
    if (!msg || len == 0 || !out) return 0;
    memset(out, 0, sizeof(*out));

    pt_json_tok_t et;
    if (pt_json_get(msg, len, "event_type", &et)) {
        char ev[32]; pt_json_tok_str(&et, ev, sizeof(ev));
        if (strcmp(ev, "market_discovery") != 0 && strcmp(ev, "discovery") != 0)
            return 0;
    } else {
        return 0;
    }

    pt_json_tok_t cid, mid, slug, ytok, ntok, strk, st_t, end_t, durtok, ela_tok, elb_tok;
    if (pt_json_get(msg, len, "condition_id", &cid)) {
        pt_json_tok_str(&cid, out->condition_id, sizeof(out->condition_id));
    }
    if (pt_json_get(msg, len, "market_id", &mid)) {
        int64_t v = 0; pt_json_tok_int64(&mid, &v); out->market_id = (uint64_t)v;
    }
    if (pt_json_get(msg, len, "slug", &slug)) {
        pt_json_tok_str(&slug, out->slug, sizeof(out->slug));
    }
    if (pt_json_get(msg, len, "yes_token_id", &ytok)) {
        pt_json_tok_str(&ytok, out->yes_token_id, sizeof(out->yes_token_id));
    }
    if (pt_json_get(msg, len, "no_token_id", &ntok)) {
        pt_json_tok_str(&ntok, out->no_token_id, sizeof(out->no_token_id));
    }
    if (pt_json_get(msg, len, "strike", &strk)) {
        pt_json_tok_double(&strk, &out->strike);
    }
    if (pt_json_get(msg, len, "start_time", &st_t)) {
        int64_t v = 0; pt_json_tok_int64(&st_t, &v); out->start_time_ms = (uint64_t)v;
    }
    if (pt_json_get(msg, len, "end_time", &end_t)) {
        int64_t v = 0; pt_json_tok_int64(&end_t, &v); out->end_time_ms = (uint64_t)v;
    }
    if (pt_json_get(msg, len, "market_duration_sec", &durtok)) {
        int64_t v = 0; pt_json_tok_int64(&durtok, &v); out->duration_sec = (int)v;
    } else if (out->end_time_ms > out->start_time_ms) {
        out->duration_sec = (int)((out->end_time_ms - out->start_time_ms) / 1000);
    }

    if (pt_json_get(msg, len, "eligible_for_strategy_a", &ela_tok)) {
        out->eligible_for_strategy_a = (ela_tok.p && (ela_tok.p[0] == 't' || ela_tok.p[0] == '1')) ? 1 : 0;
    } else {
        out->eligible_for_strategy_a = (out->duration_sec >= 240 && out->duration_sec <= 450) ? 1 : 0;
    }

    if (pt_json_get(msg, len, "eligible_for_strategy_b", &elb_tok)) {
        out->eligible_for_strategy_b = (elb_tok.p && (elb_tok.p[0] == 't' || elb_tok.p[0] == '1')) ? 1 : 0;
    } else {
        out->eligible_for_strategy_b = (out->duration_sec >= 750 && out->duration_sec <= 1200) ? 1 : 0;
    }

    return (out->condition_id[0] != '\0' && out->yes_token_id[0] != '\0' && out->no_token_id[0] != '\0') ? 1 : 0;
}

int pt_parse_market_resolution_msg(const char *msg, size_t len, pt_market_resolution_msg_t *out)
{
    if (!msg || len == 0 || !out) return 0;
    memset(out, 0, sizeof(*out));
    out->winning_is_yes = -1;

    pt_json_tok_t et;
    if (pt_json_get(msg, len, "event_type", &et)) {
        char ev[32]; pt_json_tok_str(&et, ev, sizeof(ev));
        if (strcmp(ev, "market_resolution") != 0 && strcmp(ev, "resolution") != 0 && strcmp(ev, "market_resolved") != 0)
            return 0;
    } else {
        return 0;
    }

    pt_json_tok_t cid, mid, win, watok, resp, rest;
    if (pt_json_get(msg, len, "condition_id", &cid)) {
        pt_json_tok_str(&cid, out->condition_id, sizeof(out->condition_id));
    }
    if (pt_json_get(msg, len, "market_id", &mid)) {
        int64_t v = 0; pt_json_tok_int64(&mid, &v); out->market_id = (uint64_t)v;
    }
    if (pt_json_get(msg, len, "winning_outcome", &win)) {
        pt_json_tok_str(&win, out->winning_outcome, sizeof(out->winning_outcome));
        out->has_winning_outcome = 1;
        if (strcasecmp(out->winning_outcome, "YES") == 0 || strcmp(out->winning_outcome, "1") == 0 || strcasecmp(out->winning_outcome, "UP") == 0) {
            out->winning_is_yes = 1;
        } else if (strcasecmp(out->winning_outcome, "NO") == 0 || strcmp(out->winning_outcome, "0") == 0 || strcasecmp(out->winning_outcome, "DOWN") == 0) {
            out->winning_is_yes = 0;
        }
    }
    if (pt_json_get(msg, len, "winning_asset_id", &watok)) {
        pt_json_tok_str(&watok, out->winning_asset_id, sizeof(out->winning_asset_id));
    }
    if (pt_json_get(msg, len, "resolution_price", &resp)) {
        pt_json_tok_double(&resp, &out->resolution_price);
    } else {
        out->resolution_price = 1.0;
    }
    if (pt_json_get(msg, len, "timestamp", &rest)) {
        int64_t v = 0; pt_json_tok_int64(&rest, &v); out->resolution_time_ms = (uint64_t)v;
    }

    return (out->condition_id[0] != '\0' || out->market_id > 0) ? 1 : 0;
}
