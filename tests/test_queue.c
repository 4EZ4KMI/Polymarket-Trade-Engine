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
    pt_sim_queue_on_trade(&sq, 101, 1, 480, 400, PT_SIDE_ASK, 2000);
    PT_ASSERT(sq.orders[0].queue_ahead == 600);
    PT_ASSERT(sq.orders[0].filled_size == 0); /* Still resting behind queue */

    /* Simulate another trade of 700 shares hitting bid at 480 */
    /* 600 remaining queue ahead gets depleted, and 100 shares fill our order */
    pt_sim_queue_on_trade(&sq, 101, 1, 480, 700, PT_SIDE_ASK, 3000);
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
