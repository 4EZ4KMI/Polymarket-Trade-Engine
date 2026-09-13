#include "net/pt_feed_parsers.h"
#include "util/pt_json.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

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

    pt_json_tok_t cid, mid, slug, ytok, ntok, strk, st_t, end_t;
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

    return (out->condition_id[0] != '\0' || out->market_id > 0) ? 1 : 0;
}

int pt_parse_market_resolution_msg(const char *msg, size_t len, pt_market_resolution_msg_t *out)
{
    if (!msg || len == 0 || !out) return 0;
    memset(out, 0, sizeof(*out));

    pt_json_tok_t et;
    if (pt_json_get(msg, len, "event_type", &et)) {
        char ev[32]; pt_json_tok_str(&et, ev, sizeof(ev));
        if (strcmp(ev, "market_resolution") != 0 && strcmp(ev, "resolution") != 0)
            return 0;
    } else {
        return 0;
    }

    pt_json_tok_t cid, mid, win, resp, rest;
    if (pt_json_get(msg, len, "condition_id", &cid)) {
        pt_json_tok_str(&cid, out->condition_id, sizeof(out->condition_id));
    }
    if (pt_json_get(msg, len, "market_id", &mid)) {
        int64_t v = 0; pt_json_tok_int64(&mid, &v); out->market_id = (uint64_t)v;
    }
    if (pt_json_get(msg, len, "winning_outcome", &win)) {
        char w[16]; pt_json_tok_str(&win, w, sizeof(w));
        out->winning_is_yes = (w[0] == 'Y' || w[0] == 'y' || w[0] == '1') ? 1 : 0;
    }
    if (pt_json_get(msg, len, "resolution_price", &resp)) {
        pt_json_tok_double(&resp, &out->resolution_price);
    }
    if (pt_json_get(msg, len, "timestamp", &rest)) {
        int64_t v = 0; pt_json_tok_int64(&rest, &v); out->resolution_time_ms = (uint64_t)v;
    }

    return (out->condition_id[0] != '\0' || out->market_id > 0) ? 1 : 0;
}
