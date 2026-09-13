#include "core/pt_market_lifecycle.h"
#include <string.h>

void pt_market_info_init(pt_market_info_t *m, pt_market_id_t market_id,
                         const char *symbol, double strike,
                         pt_nsec_t open_t, pt_nsec_t expiry_t,
                         double stop_cutoff_sec)
{
    if (!m) return;
    memset(m, 0, sizeof(*m));
    m->market_id = market_id;
    if (symbol) strncpy(m->symbol, symbol, sizeof(m->symbol) - 1);
    m->strike_price = strike;
    m->open_time_ns = open_t;
    m->expiry_time_ns = expiry_t;
    m->stop_before_expiry_sec = stop_cutoff_sec > 0 ? stop_cutoff_sec : 10.0;
    m->state = PT_LIFECYCLE_ACTIVE;
}

int pt_market_can_trade(const pt_market_info_t *m, pt_nsec_t now)
{
    if (!m) return 0;
    if (m->state != PT_LIFECYCLE_ACTIVE) return 0;
    if (now < m->open_time_ns) return 0;

    pt_nsec_t cutoff_ns = (pt_nsec_t)(m->stop_before_expiry_sec * 1000000000.0);
    if (now + cutoff_ns >= m->expiry_time_ns) return 0;

    return 1;
}

double pt_market_time_to_expiry_sec(const pt_market_info_t *m, pt_nsec_t now)
{
    if (!m || now >= m->expiry_time_ns) return 0.0;
    return (double)(m->expiry_time_ns - now) / 1e9;
}

int pt_market_lifecycle_tick(pt_market_info_t *m, double current_btc_price,
                              pt_nsec_t now, pt_portfolio_t *portfolio)
{
    if (!m) return 0;

    if (m->state == PT_LIFECYCLE_ACTIVE) {
        pt_nsec_t cutoff_ns = (pt_nsec_t)(m->stop_before_expiry_sec * 1000000000.0);
        if (now + cutoff_ns >= m->expiry_time_ns) {
            m->state = PT_LIFECYCLE_EXPIRING;
        }
    }

    if (m->state == PT_LIFECYCLE_EXPIRING && now >= m->expiry_time_ns) {
        m->state = PT_LIFECYCLE_EXPIRED;
    }

    if (m->state == PT_LIFECYCLE_EXPIRED) {
        m->resolution_price = current_btc_price;
        m->resolved_winning_outcome = (current_btc_price >= m->strike_price) ? 1 : 0;
        m->state = PT_LIFECYCLE_RESOLVED;

        if (portfolio) {
            pt_portfolio_settle_market(portfolio, m->market_id, m->resolved_winning_outcome, NULL, NULL);
        }
        return 1; /* Resolved */
    }

    return 0;
}
