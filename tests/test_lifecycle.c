#include "test_harness.h"
#include "core/pt_market_lifecycle.h"
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
    pt_portfolio_on_fill(&p, 101, 1, PT_SIDE_BID, 100, 450);
    PT_ASSERT_NEAR(p.cash, 955.0, 0.001);

    /* Tick at expiry with BTC = 88000.0 (> 87500 strike -> YES wins) */
    m.state = PT_LIFECYCLE_EXPIRED;
    int resolved = pt_market_lifecycle_tick(&m, 88000.0, expiry_t, &p);
    PT_ASSERT(resolved == 1);
    PT_ASSERT(m.state == PT_LIFECYCLE_RESOLVED);
    PT_ASSERT(m.resolved_winning_outcome == 1);

    /* Portfolio should receive 100 * $1.00 = $100 payoff -> Final Cash = $955 + $100 = $1055 */
    PT_ASSERT_NEAR(p.cash, 1055.0, 0.001);
    PT_ASSERT_NEAR(p.realized_pnl, 55.0, 0.001);
}
