#include "test_harness.h"
#include "execution/pt_sim_queue.h"
#include "orderbook/pt_book.h"

PT_T(queue_realistic_depletion)
{
    pt_sim_queue_cfg_t cfg = {
        .net_latency_ms = 0.0,
        .exchange_process_ms = 0.0,
        .cancel_latency_ms = 0.0,
        .queue_model = PT_QUEUE_MODEL_REALISTIC,
        .adverse_selection_bps = 0.0
    };
    pt_sim_queue_t sq;
    pt_sim_queue_init(&sq, &cfg);

    pt_book_t yes_book;
    pt_book_init(&yes_book);
    /* Level: 480 @ 1000 shares */
    pt_level_t bids[1] = { {480, 1000} };
    pt_book_snapshot(&yes_book, PT_SIDE_BID, bids, 1, 1);

    /* Submit bid at 480 for 100 shares */
    pt_order_id_t oid = pt_sim_queue_submit(&sq, 101, 1, PT_SIDE_BID, PT_OTYPE_LIMIT, 480, 100, &yes_book, 0, 1, 1000);
    PT_ASSERT(oid > 0);
    PT_ASSERT(sq.orders[0].queue_ahead == 1000);

    /* Simulate a trade of 400 shares hitting bid at 480 */
    pt_sim_queue_on_trade(&sq, 101, 1, 480, 400, PT_SIDE_ASK, 2000, NULL, NULL);
    PT_ASSERT(sq.orders[0].queue_ahead == 600);
    PT_ASSERT(sq.orders[0].filled_size == 0); /* Still resting behind queue */

    /* Simulate another trade of 700 shares hitting bid at 480 */
    /* 600 remaining queue ahead gets depleted, and 100 shares fill our order */
    pt_sim_queue_on_trade(&sq, 101, 1, 480, 700, PT_SIDE_ASK, 3000, NULL, NULL);
    PT_ASSERT(sq.orders[0].queue_ahead == 0);
    PT_ASSERT(sq.orders[0].filled_size == 100);
    PT_ASSERT(sq.orders[0].state == PT_OSTATE_FILLED);
}

PT_T(queue_conservative_vs_aggressive)
{
    pt_sim_queue_cfg_t cfg_c = { .queue_model = PT_QUEUE_MODEL_CONSERVATIVE };
    pt_sim_queue_t sq_c;
    pt_sim_queue_init(&sq_c, &cfg_c);

    pt_sim_queue_cfg_t cfg_a = { .queue_model = PT_QUEUE_MODEL_AGGRESSIVE };
    pt_sim_queue_t sq_a;
    pt_sim_queue_init(&sq_a, &cfg_a);

    pt_book_t bk;
    pt_book_init(&bk);
    pt_level_t bids[1] = { {500, 1000} };
    pt_book_snapshot(&bk, PT_SIDE_BID, bids, 1, 1);

    pt_sim_queue_submit(&sq_c, 101, 1, PT_SIDE_BID, PT_OTYPE_LIMIT, 500, 100, &bk, 0, 1, 1000);
    pt_sim_queue_submit(&sq_a, 101, 1, PT_SIDE_BID, PT_OTYPE_LIMIT, 500, 100, &bk, 0, 1, 1000);

    /* Cancellation of 300 shares on level 500 */
    pt_sim_queue_on_level_change(&sq_c, 101, 1, PT_SIDE_BID, 500, 1000, 700, 2000);
    pt_sim_queue_on_level_change(&sq_a, 101, 1, PT_SIDE_BID, 500, 1000, 700, 2000);

    /* Conservative assumes cancel happened behind us -> queue_ahead remains 1000 */
    PT_ASSERT(sq_c.orders[0].queue_ahead == 1000);
    /* Aggressive assumes cancel happened ahead of us -> queue_ahead reduces by 300 to 700 */
    PT_ASSERT(sq_a.orders[0].queue_ahead == 700);
}

typedef struct {
    int fills_called;
    pt_size_t total_filled;
    pt_price_t last_price;
} test_queue_cb_ctx_t;

static void test_fill_cb_(pt_order_t *o, pt_size_t fill_qty, pt_price_t fill_price, double adverse, void *ud)
{
    (void)o; (void)adverse;
    test_queue_cb_ctx_t *ctx = (test_queue_cb_ctx_t *)ud;
    if (ctx) {
        ctx->fills_called++;
        ctx->total_filled += fill_qty;
        ctx->last_price = fill_price;
    }
}

PT_T(queue_fill_callback_propagation)
{
    pt_sim_queue_cfg_t cfg = {
        .net_latency_ms = 0.0,
        .exchange_process_ms = 0.0,
        .cancel_latency_ms = 0.0,
        .queue_model = PT_QUEUE_MODEL_REALISTIC,
        .adverse_selection_bps = 2.0
    };
    pt_sim_queue_t sq;
    pt_sim_queue_init(&sq, &cfg);

    pt_book_t bk;
    pt_book_init(&bk);
    pt_level_t bids[1] = { {490, 500} };
    pt_book_snapshot(&bk, PT_SIDE_BID, bids, 1, 1);

    pt_order_id_t oid = pt_sim_queue_submit(&sq, 201, 1, PT_SIDE_BID, PT_OTYPE_LIMIT, 490, 150, &bk, 1, 10, 1000);
    PT_ASSERT(oid > 0);

    test_queue_cb_ctx_t cb_ctx = { 0, 0, 0 };

    /* Trade 1: 300 shares consumed ahead */
    int f1 = pt_sim_queue_on_trade(&sq, 201, 1, 490, 300, PT_SIDE_ASK, 2000, test_fill_cb_, &cb_ctx);
    PT_ASSERT(f1 == 0);
    PT_ASSERT(cb_ctx.fills_called == 0);
    PT_ASSERT(sq.orders[0].queue_ahead == 200);

    /* Trade 2: 300 shares -> 200 ahead consumed + 100 of our order filled */
    int f2 = pt_sim_queue_on_trade(&sq, 201, 1, 490, 300, PT_SIDE_ASK, 3000, test_fill_cb_, &cb_ctx);
    PT_ASSERT(f2 == 1);
    PT_ASSERT(cb_ctx.fills_called == 1);
    PT_ASSERT(cb_ctx.total_filled == 100);
    PT_ASSERT(cb_ctx.last_price == 490);
    PT_ASSERT(sq.orders[0].queue_ahead == 0);
    PT_ASSERT(sq.orders[0].state == PT_OSTATE_PARTIAL);
    PT_ASSERT(sq.orders[0].remaining_size == 50);

    /* Trade 3: 50 shares -> completes remaining size */
    int f3 = pt_sim_queue_on_trade(&sq, 201, 1, 490, 50, PT_SIDE_ASK, 4000, test_fill_cb_, &cb_ctx);
    PT_ASSERT(f3 == 1);
    PT_ASSERT(cb_ctx.fills_called == 2);
    PT_ASSERT(cb_ctx.total_filled == 150);
    PT_ASSERT(sq.orders[0].state == PT_OSTATE_FILLED);
    PT_ASSERT(sq.orders[0].remaining_size == 0);
}
