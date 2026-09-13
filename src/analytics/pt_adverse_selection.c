#include "analytics/pt_adverse_selection.h"
#include <string.h>
#include <math.h>

static const pt_nsec_t g_horizons[PT_ADV_HORIZONS] = {
    PT_ADV_H_10MS,
    PT_ADV_H_50MS,
    PT_ADV_H_100MS,
    PT_ADV_H_500MS,
    PT_ADV_H_1S,
    PT_ADV_H_5S
};

void pt_adverse_tracker_init(pt_adverse_tracker_t *t)
{
    if (!t) return;
    memset(t, 0, sizeof(*t));
    t->next_fill_id = 1;
}

uint64_t pt_adverse_record_fill(pt_adverse_tracker_t *t,
                                pt_nsec_t fill_time_ns,
                                pt_market_id_t market_id,
                                int strategy,
                                int is_yes,
                                int side,
                                pt_price_t fill_price,
                                pt_size_t fill_qty)
{
    if (!t || fill_price <= 0) return 0;

    size_t slot;
    if (t->count < PT_ADV_MAX_RECORDS) {
        slot = t->count++;
    } else {
        /* Ring replace oldest */
        slot = (size_t)(t->next_fill_id % PT_ADV_MAX_RECORDS);
    }

    pt_adverse_record_t *r = &t->records[slot];
    memset(r, 0, sizeof(*r));
    r->fill_id = t->next_fill_id++;
    r->fill_time_ns = fill_time_ns;
    r->market_id = market_id;
    r->strategy = strategy;
    r->is_yes = is_yes;
    r->side = side;
    r->fill_price = fill_price;
    r->fill_qty = fill_qty;

    return r->fill_id;
}

void pt_adverse_tracker_on_price(pt_adverse_tracker_t *t,
                                 pt_market_id_t market_id,
                                 int is_yes,
                                 pt_price_t current_mid_price,
                                 pt_nsec_t now)
{
    if (!t || current_mid_price <= 0) return;

    for (size_t i = 0; i < t->count; i++) {
        pt_adverse_record_t *r = &t->records[i];
        if (r->complete || r->fill_price <= 0) continue;
        if (r->market_id != market_id || r->is_yes != is_yes) continue;

        pt_nsec_t elapsed = (now >= r->fill_time_ns) ? (now - r->fill_time_ns) : 0;

        for (int h = 0; h < PT_ADV_HORIZONS; h++) {
            if (!r->horizon_measured[h] && elapsed >= g_horizons[h]) {
                double delta_p;
                if (r->side == PT_SIDE_BID) {
                    /* Bought at fill_price: adverse if price drops */
                    delta_p = (double)(r->fill_price - current_mid_price);
                } else {
                    /* Sold at fill_price: adverse if price rises */
                    delta_p = (double)(current_mid_price - r->fill_price);
                }
                double bps = (delta_p / (double)r->fill_price) * 10000.0;
                r->movement_bps[h] = bps;
                r->horizon_measured[h] = 1;
            }
        }

        if (r->horizon_measured[PT_ADV_HORIZONS - 1]) {
            r->complete = 1;
        }
    }

    /* Update summary aggregates */
    double sums[PT_ADV_HORIZONS] = {0};
    int counts[PT_ADV_HORIZONS] = {0};

    for (size_t i = 0; i < t->count; i++) {
        const pt_adverse_record_t *r = &t->records[i];
        for (int h = 0; h < PT_ADV_HORIZONS; h++) {
            if (r->horizon_measured[h]) {
                sums[h] += r->movement_bps[h];
                counts[h]++;
            }
        }
    }

    if (counts[0] > 0) t->avg_adv_bps_10ms = sums[0] / (double)counts[0];
    if (counts[1] > 0) t->avg_adv_bps_50ms = sums[1] / (double)counts[1];
    if (counts[2] > 0) t->avg_adv_bps_100ms = sums[2] / (double)counts[2];
    if (counts[3] > 0) t->avg_adv_bps_500ms = sums[3] / (double)counts[3];
    if (counts[4] > 0) t->avg_adv_bps_1s = sums[4] / (double)counts[4];
    if (counts[5] > 0) t->avg_adv_bps_5s = sums[5] / (double)counts[5];

    if (counts[4] > 0) {
        t->overall_avg_adv_bps = t->avg_adv_bps_1s;
    } else if (counts[2] > 0) {
        t->overall_avg_adv_bps = t->avg_adv_bps_100ms;
    } else if (counts[0] > 0) {
        t->overall_avg_adv_bps = t->avg_adv_bps_10ms;
    } else {
        t->overall_avg_adv_bps = 0.0;
    }
}

double pt_adverse_get_fill_bps(const pt_adverse_tracker_t *t, uint64_t fill_id)
{
    if (!t || fill_id == 0) return 0.0;

    for (size_t i = 0; i < t->count; i++) {
        const pt_adverse_record_t *r = &t->records[i];
        if (r->fill_id == fill_id) {
            /* Return movement at highest measured horizon */
            for (int h = PT_ADV_HORIZONS - 1; h >= 0; h--) {
                if (r->horizon_measured[h]) {
                    return r->movement_bps[h];
                }
            }
            return 0.0; /* Not yet matured to 10ms */
        }
    }
    return 0.0;
}
