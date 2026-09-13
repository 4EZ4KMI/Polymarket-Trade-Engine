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