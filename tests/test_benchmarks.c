#include "test_harness.h"
#include "util/pt_clock.h"
#include "orderbook/pt_book.h"
#include "features/pt_features.h"
#include "strategies/pt_model.h"
#include "strategies/pt_flow_skew.h"
#include "execution/pt_arb.h"
#include "execution/pt_sim_queue.h"

#include <stdio.h>

PT_T(benchmark_hot_paths)
{
    pt_book_t yes_book, no_book;
    pt_book_init(&yes_book);
    pt_book_init(&no_book);

    pt_level_t ya[5] = { {480, 200}, {485, 300}, {490, 400}, {495, 500}, {500, 600} };
    pt_level_t yb[5] = { {475, 200}, {470, 300}, {465, 400}, {460, 500}, {455, 600} };
    pt_book_snapshot(&yes_book, PT_SIDE_ASK, ya, 5, 1);
    pt_book_snapshot(&yes_book, PT_SIDE_BID, yb, 5, 1);

    pt_level_t na[5] = { {490, 200}, {495, 300}, {500, 400}, {505, 500}, {510, 600} };
    pt_level_t nb[5] = { {485, 200}, {480, 300}, {475, 400}, {470, 500}, {465, 600} };
    pt_book_snapshot(&no_book, PT_SIDE_ASK, na, 5, 1);
    pt_book_snapshot(&no_book, PT_SIDE_BID, nb, 5, 1);

    const int N_ITER = 100000;

    /* 1. Benchmark Order Book Update */
    pt_nsec_t t0 = pt_clock_mono_ns();
    for (int i = 0; i < N_ITER; i++) {
        pt_book_update(&yes_book, PT_SIDE_ASK, 480 + (i % 10), (i % 50) + 10, 1, i + 2);
    }
    pt_nsec_t t1 = pt_clock_mono_ns();
    double book_ns_per_op = (double)(t1 - t0) / (double)N_ITER;

    /* 2. Benchmark Feature Computation (L1-L10 Imbalances + Microprice) */
    pt_market_features_t feat;
    t0 = pt_clock_mono_ns();
    for (int i = 0; i < N_ITER; i++) {
        pt_book_features_compute(&yes_book, &feat.book);
    }
    t1 = pt_clock_mono_ns();
    double feat_ns_per_op = (double)(t1 - t0) / (double)N_ITER;

    /* 3. Benchmark Parity Arbitrage VWAP Multi-Level Walking */
    pt_arb_cfg_t acfg = {
        .fees = { .taker_fee_bps = 2.0, .maker_fee_bps = 0.0, .maker_rebate_bps = 0.5, .dynamic_fee_scale = 1.0 },
        .slippage_bps = 1.0,
        .latency_buffer_pct = 0.001,
        .risk_buffer_pct = 0.001,
        .min_edge_pct = 0.005,
        .min_liquidity = 10
    };
    pt_arb_opp_t opp;
    t0 = pt_clock_mono_ns();
    for (int i = 0; i < N_ITER; i++) {
        pt_arb_calc(&yes_book, &no_book, &acfg, 500, &opp);
    }
    t1 = pt_clock_mono_ns();
    double arb_ns_per_op = (double)(t1 - t0) / (double)N_ITER;

    /* 4. Benchmark Queue Matching Simulation */
    pt_sim_queue_cfg_t sq_cfg = { .queue_model = PT_QUEUE_MODEL_REALISTIC };
    pt_sim_queue_t sq;
    pt_sim_queue_init(&sq, &sq_cfg);
    pt_sim_queue_submit(&sq, 101, 1, PT_SIDE_BID, PT_OTYPE_LIMIT, 480, 500, &yes_book, 0, 1, 1000);

    t0 = pt_clock_mono_ns();
    for (int i = 0; i < N_ITER; i++) {
        pt_sim_queue_on_trade(&sq, 101, 1, 480, 5, PT_SIDE_ASK, 2000 + i);
    }
    t1 = pt_clock_mono_ns();
    double queue_ns_per_op = (double)(t1 - t0) / (double)N_ITER;

    printf("\n==================== HOT PATH BENCHMARKS ====================\n");
    printf("1. Order Book Level Update:         %.2f ns/op (%.2f M ops/sec)\n",
           book_ns_per_op, 1000.0 / book_ns_per_op);
    printf("2. L1..L10 Microprice & Imbalance:   %.2f ns/op (%.2f M ops/sec)\n",
           feat_ns_per_op, 1000.0 / feat_ns_per_op);
    printf("3. Parity Arb Multi-Level VWAP Calc: %.2f ns/op (%.2f M ops/sec)\n",
           arb_ns_per_op, 1000.0 / arb_ns_per_op);
    printf("4. Realistic Queue Trade Matching:   %.2f ns/op (%.2f M ops/sec)\n",
           queue_ns_per_op, 1000.0 / queue_ns_per_op);
    printf("=============================================================\n");

    PT_ASSERT(book_ns_per_op < 500.0);
    PT_ASSERT(feat_ns_per_op < 500.0);
    PT_ASSERT(arb_ns_per_op < 1000.0);
    PT_ASSERT(queue_ns_per_op < 500.0);
}
