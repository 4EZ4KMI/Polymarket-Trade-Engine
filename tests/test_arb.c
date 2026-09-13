#include "test_harness.h"
#include "execution/pt_arb.h"
#include "execution/pt_arb_mgr.h"

/* Seeds books where gross edge = 1 - (0.470 + 0.480) = 0.050 */
static void seed_arb_books(pt_book_t *y, pt_book_t *n)
{
    pt_book_init(y); pt_book_init(n);
    /* YES asks: 0.470@100, 0.480@200, 0.490@500 */
    pt_level_t ya[3] = { {470, 100}, {480, 200}, {490, 500} };
    pt_book_snapshot(y, PT_SIDE_ASK, ya, 3, 1);
    /* NO asks: 0.480@100, 0.490@200, 0.500@500 */
    pt_level_t na[3] = { {480, 100}, {490, 200}, {500, 500} };
    pt_book_snapshot(n, PT_SIDE_ASK, na, 3, 1);
}

PT_T(arb_edge_calculation)
{
    pt_book_t y, n;
    seed_arb_books(&y, &n);
    pt_arb_cfg_t cfg = {
        .taker_fee_bps = 2.0,      /* 0.02% */
        .slippage_bps  = 0.0,
        .latency_buffer_pct = 0.002, /* 0.2% */
        .risk_buffer_pct    = 0.001, /* 0.1% */
        .min_edge_pct       = 0.01,  /* 1% */
        .min_liquidity      = 50
    };
    pt_arb_opp_t opp;
    int ok = pt_arb_calc(&y, &n, &cfg, 500, &opp);
    PT_ASSERT(ok == 1);
    PT_ASSERT(opp.has_opportunity == 1);
    PT_ASSERT_NEAR(opp.gross_edge, 0.050, 0.0001);
    /* at size 100: asks 0.470+0.480 = 0.950; net ~ 0.050 - (fees+buffers) > 0.01 */
    PT_ASSERT(opp.max_executable_size >= 100);
    PT_ASSERT(opp.net_edge > 0.01);
    PT_ASSERT(opp.expected_profit > 0.0);
}

PT_T(arb_partial_fill_protection)
{
    pt_book_t y, n;
    seed_arb_books(&y, &n);
    pt_arb_cfg_t acfg = {
        .taker_fee_bps = 2.0, .slippage_bps = 0.0,
        .latency_buffer_pct = 0.002, .risk_buffer_pct = 0.001,
        .min_edge_pct = 0.01, .min_liquidity = 50
    };
    pt_arb_mgr_cfg_t mcfg = {
        .max_loss_per_trade = 5.0, /* $5 max loss */
        .requote_tolerance  = 0.01,
        .allow_requote      = 1
    };
    pt_arb_mgr_t mgr;
    pt_arb_mgr_init(&mgr, &acfg, &mcfg, &y, &n);

    pt_arb_opp_t opp;
    pt_arb_calc(&y, &n, &acfg, 200, &opp);
    PT_ASSERT(opp.has_opportunity);

    pt_arb_op_t op;
    int idx = pt_arb_mgr_open(&mgr, &opp, &op, 1001, 1000000ULL);
    PT_ASSERT(idx >= 0);

    /* Partial fill on YES: got 80 of 100 at 0.470 */
    pt_arb_action_t act;
    pt_arb_mgr_on_fill(&mgr, idx, 1, 80, 470, 1001000ULL, &act);
    /* Manager must signal hedging NO leg with the 80 excess */
    PT_ASSERT(act.action == PT_ARB_ACT_SUBMIT);
    PT_ASSERT(act.leg == 1); /* NO leg */
    PT_ASSERT(act.size == 80);

    /* Simulate edge disappearance in books (NO ask jumps to 0.550) */
    pt_level_t bad_no[1] = { {550, 200} };
    pt_book_snapshot(&n, PT_SIDE_ASK, bad_no, 1, 2);
    /* Fill NO with only 20 -> now YES has 60 excess, but edge vanished */
    pt_arb_mgr_on_fill(&mgr, idx, 0, 20, 550, 1002000ULL, &act);
    /* Must cancel further aggressive entries / hedge existing deficit */
    PT_ASSERT(act.action == PT_ARB_ACT_CANCEL || act.action == PT_ARB_ACT_ABORT);
}