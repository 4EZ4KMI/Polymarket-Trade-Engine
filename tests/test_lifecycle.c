#include "test_harness.h"
#include "core/pt_market_lifecycle.h"
#include "core/pt_market_registry.h"
#include "portfolio/pt_portfolio.h"

PT_T(market_lifecycle_trading_cutoff)
{
    pt_market_info_t m;
    pt_nsec_t open_t = 1000000000ULL;
    pt_nsec_t expiry_t = 1000000000ULL + 300000000000ULL; /* 300s */
    pt_market_info_init(&m, 101, "BTC-5M", 87500.0, open_t, expiry_t, 10.0);

    /* 100s into trading -> can trade */
    PT_ASSERT(pt_market_can_trade(&m, open_t + 100000000000ULL) == 1);

    /* 5s before expiry (within 10s cutoff) -> CANNOT trade */
    PT_ASSERT(pt_market_can_trade(&m, expiry_t - 5000000000ULL) == 0);
}

PT_T(market_lifecycle_settlement)
{
    pt_market_info_t m;
    pt_nsec_t open_t = 1000000000ULL;
    pt_nsec_t expiry_t = open_t + 300000000000ULL;
    pt_market_info_init(&m, 101, "BTC-5M", 87500.0, open_t, expiry_t, 10.0);

    pt_portfolio_t p;
    pt_portfolio_init(&p, 1000.0);

    /* Buy 100 YES @ 0.45 ($45 cost) */
    pt_portfolio_on_fill(&p, 101, 1, PT_SIDE_BID, 100, 450, 0);
    PT_ASSERT_NEAR(p.cash, 955.0, 0.001);

    /* Tick at expiry with BTC = 88000.0 (> 87500 strike -> YES wins) */
    m.state = PT_LIFECYCLE_EXPIRED;
    int resolved = pt_market_lifecycle_tick(&m, 88000.0, expiry_t, &p, NULL, NULL);
    PT_ASSERT(resolved == 1);
    PT_ASSERT(m.state == PT_LIFECYCLE_RESOLVED);
    PT_ASSERT(m.resolved_winning_outcome == 1);

    /* Portfolio should receive 100 * $1.00 = $100 payoff -> Final Cash = $955 + $100 = $1055 */
    PT_ASSERT_NEAR(p.cash, 1055.0, 0.001);
    PT_ASSERT_NEAR(p.realized_pnl, 55.0, 0.001);
}

PT_T(market_registry_lifecycle_discovery_to_resolution)
{
    pt_market_registry_t reg;
    pt_market_registry_init(&reg);

    /* 1. Initial State: EMPTY */
    PT_ASSERT(reg.count == 0);
    PT_ASSERT(pt_market_registry_get_active(&reg) == NULL);

    /* 2. DISCOVERED via real metadata */
    pt_nsec_t t0 = 1000000000ULL;
    pt_market_entry_t *m = pt_market_registry_add(&reg, 101, "0xabcdef123456", "btc-5m-88000",
                                                   "token_yes_123", "token_no_123", 88000.0,
                                                   t0, t0 + 300000000000ULL, t0);
    PT_ASSERT(m != NULL);
    PT_ASSERT(m->state == PT_MKT_STATE_DISCOVERED);
    PT_ASSERT(pt_market_entry_can_trade(m, t0) == 0); /* Not tradeable yet until snapshot */

    /* 3. SUBSCRIBING */
    pt_market_registry_set_state(&reg, 101, PT_MKT_STATE_SUBSCRIBING);
    PT_ASSERT(m->state == PT_MKT_STATE_SUBSCRIBING);

    /* 4. SNAPSHOT RECEIVED -> ACTIVE */
    pt_market_registry_on_snapshot(&reg, 101, t0 + 1000000ULL);
    PT_ASSERT(m->state == PT_MKT_STATE_ACTIVE);
    PT_ASSERT(pt_market_entry_can_trade(m, t0 + 1000000ULL) == 1);

    /* 5. Lookup by token ID */
    int is_yes = -1;
    pt_market_entry_t *found = pt_market_registry_find_by_token(&reg, "token_no_123", &is_yes);
    PT_ASSERT(found == m);
    PT_ASSERT(is_yes == 0);

    /* 5b. Strict fail-closed: Unknown token returns NULL */
    int is_yes_unknown = -1;
    pt_market_entry_t *unrec = pt_market_registry_find_by_token(&reg, "token_completely_unknown", &is_yes_unknown);
    PT_ASSERT(unrec == NULL);
    PT_ASSERT(is_yes_unknown == -1);

    /* 6. REAL RESOLUTION from oracle outcome */
    int res = pt_market_registry_resolve(&reg, "0xabcdef123456", 101, 1, 88150.0, t0 + 300000000000ULL);
    PT_ASSERT(res == 0);
    PT_ASSERT(m->state == PT_MKT_STATE_RESOLVED);
    PT_ASSERT(m->resolved_winner == 1);
    PT_ASSERT_NEAR(m->resolution_price, 88150.0, 0.001);
    PT_ASSERT(pt_market_entry_can_trade(m, t0 + 300000000000ULL) == 0);
}

