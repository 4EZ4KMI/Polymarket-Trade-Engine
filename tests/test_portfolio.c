#include "test_harness.h"
#include "portfolio/pt_portfolio.h"

PT_T(portfolio_buy_and_sell_pnl)
{
    pt_portfolio_t p;
    pt_portfolio_init(&p, 1000.0);
    PT_ASSERT_NEAR(p.cash, 1000.0, 0.001);

    /* Buy 100 YES @ 0.450 -> cost $45 */
    pt_portfolio_on_fill(&p, 1, 1, PT_SIDE_BID, 100, 450);
    PT_ASSERT_NEAR(p.cash, 955.0, 0.001);
    PT_ASSERT_NEAR(p.total_exposure, 45.0, 0.001);

    /* Mark to market at 0.500 -> unrealized +$5 */
    pt_portfolio_mark(&p, 1, 1, 500);
    PT_ASSERT_NEAR(p.unrealized_pnl, 5.0, 0.001);
    PT_ASSERT_NEAR(pt_portfolio_equity(&p), 1005.0, 0.001);

    /* Sell 50 YES @ 0.550 -> realized +$5.00 on 50 shares */
    pt_portfolio_on_fill(&p, 1, 1, PT_SIDE_ASK, 50, 550);
    PT_ASSERT_NEAR(p.cash, 955.0 + 27.50, 0.001);
    PT_ASSERT_NEAR(p.realized_pnl, 5.0, 0.001);
    PT_ASSERT(p.trades_count == 1);
    PT_ASSERT(p.wins_count == 1);
    PT_ASSERT_NEAR(pt_portfolio_win_rate(&p), 1.0, 0.001);
}