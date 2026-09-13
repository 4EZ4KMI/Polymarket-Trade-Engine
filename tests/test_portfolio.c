#include "test_harness.h"
#include "portfolio/pt_portfolio.h"
#include "analytics/pt_strategy_stats.h"
#include "analytics/pt_lifecycle_tracker.h"

PT_T(portfolio_buy_and_sell_pnl)
{
    pt_portfolio_t p;
    pt_portfolio_init(&p, 1000.0);
    PT_ASSERT_NEAR(p.cash, 1000.0, 0.001);

    /* Buy 100 YES @ 0.450 -> cost $45 (Strategy A) */
    pt_portfolio_on_fill(&p, 1, 1, PT_SIDE_BID, 100, 450, 0);
    PT_ASSERT_NEAR(p.cash, 955.0, 0.001);
    PT_ASSERT_NEAR(p.total_exposure, 45.0, 0.001);

    /* Mark to market at 0.500 -> unrealized +$5 */
    pt_portfolio_mark(&p, 1, 1, 500);
    PT_ASSERT_NEAR(p.unrealized_pnl, 5.0, 0.001);
    PT_ASSERT_NEAR(pt_portfolio_equity(&p), 1005.0, 0.001);

    /* Sell 50 YES @ 0.550 -> realized +$5.00 on 50 shares */
    pt_portfolio_on_fill(&p, 1, 1, PT_SIDE_ASK, 50, 550, 0);
    PT_ASSERT_NEAR(p.cash, 955.0 + 27.50, 0.001);
    PT_ASSERT_NEAR(p.realized_pnl, 5.0, 0.001);
    PT_ASSERT(p.trades_count == 1);
    PT_ASSERT(p.wins_count == 1);
    PT_ASSERT_NEAR(pt_portfolio_win_rate(&p), 1.0, 0.001);
}

PT_T(portfolio_isolated_strategy_settlement)
{
    pt_portfolio_t p;
    pt_portfolio_init(&p, 1000.0);

    pt_strategy_stats_tracker_t st;
    pt_strat_stats_init(&st, NULL, NULL, NULL, NULL, NULL);

    /* Strategy A (PT_STRAT_PARITY5M = 0) buys 100 YES @ 0.48 ($48 cost) in Market 101 */
    pt_portfolio_on_fill(&p, 101, 1, PT_SIDE_BID, 100, 480, 0);

    /* Strategy B (PT_STRAT_FLOW15M = 1) buys 100 NO @ 0.52 ($52 cost) in Market 101 */
    pt_portfolio_on_fill(&p, 101, 0, PT_SIDE_BID, 100, 520, 1);

    PT_ASSERT_NEAR(p.cash, 1000.0 - 48.0 - 52.0, 0.001);
    PT_ASSERT_NEAR(p.total_exposure, 100.0, 0.001);

    /* Settle Market 101: YES wins (winning_is_yes = 1) */
    pt_portfolio_settle_market(&p, 101, 1, &st, NULL);

    /* Strategy A should have won: payoff $100, cost $48 -> PnL = +$52 (1 win, 0 loss) */
    PT_ASSERT(st.strat_a.wins == 1);
    PT_ASSERT(st.strat_a.losses == 0);
    PT_ASSERT_NEAR(st.strat_a.realized_pnl, 52.0, 0.001);
    PT_ASSERT_NEAR(st.strat_a.win_rate, 1.0, 0.001);

    /* Strategy B should have lost: payoff $0, cost $52 -> PnL = -$52 (0 win, 1 loss) */
    PT_ASSERT(st.strat_b.wins == 0);
    PT_ASSERT(st.strat_b.losses == 1);
    PT_ASSERT_NEAR(st.strat_b.realized_pnl, -52.0, 0.001);
    PT_ASSERT_NEAR(st.strat_b.win_rate, 0.0, 0.001);

    /* Portfolio overall: +$52 - $52 = $0 net PnL, cash = $1000.0 */
    PT_ASSERT_NEAR(p.cash, 1000.0, 0.001);
    PT_ASSERT_NEAR(p.total_exposure, 0.0, 0.001);
    PT_ASSERT_NEAR(p.realized_pnl, 0.0, 0.001);
}
