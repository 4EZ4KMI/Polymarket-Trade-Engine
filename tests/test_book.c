#include "test_harness.h"
#include "orderbook/pt_book.h"

/* Book-wrap helper so each test starts clean */
static void seed_book(pt_book_t *b)
{
    pt_book_init(b);
    /* asks ascending: 0.480@120, 0.490@300, 0.500@50 */
    pt_level_t a[3] = { {480,120}, {490,300}, {500,50} };
    pt_book_snapshot(b, PT_SIDE_ASK, a, 3, 1);
    /* bids: best bid highest; store ascending 0.440@80, 0.450@200, 0.460@150 */
    pt_level_t bid[3] = { {440,80}, {450,200}, {460,150} };
    pt_book_snapshot(b, PT_SIDE_BID, bid, 3, 1);
}

PT_T(book_update_and_best)
{
    pt_book_t b;
    seed_book(&b);
    pt_price_t p; pt_size_t s;

    PT_ASSERT(pt_book_best_ask(&b, &p, &s) == 0);
    PT_ASSERT(p == 480 && s == 120);
    PT_ASSERT(pt_book_best_bid(&b, &p, &s) == 0);
    PT_ASSERT(p == 460 && s == 150);

    /* delta add to best ask level */
    PT_ASSERT(pt_book_update(&b, PT_SIDE_ASK, 480, +30, 1, 2) == 0);
    PT_ASSERT(pt_book_depth_volume(&b, PT_SIDE_ASK, 1) == 150);

    /* remove level by absolute zero */
    PT_ASSERT(pt_book_update(&b, PT_SIDE_ASK, 490, 0, 0, 3) == 0);
    PT_ASSERT(pt_book_depth_volume(&b, PT_SIDE_ASK, 2) == (150 + 50));
    PT_ASSERT(pt_book_best_ask(&b, &p, NULL) == 0 && p == 480);
}

PT_T(book_walk_avg_price)
{
    pt_book_t b;
    seed_book(&b);
    int64_t avg = 0; int used = 0;
    /* want 150 shares executable at 0.480 -> fills 120@480 + 30@490 */
    pt_size_t filled = pt_book_walk(&b, PT_SIDE_ASK, 150, &avg, &used);
    PT_ASSERT(filled == 150);
    PT_ASSERT(used == 2);
    PT_ASSERT(avg == (120*480 + 30*490) / 150); /* 482 */
}

PT_T(book_walk_partial)
{
    pt_book_t b;
    seed_book(&b);
    int64_t avg = 0; int used = 0;
    /* want more than available */
    pt_size_t filled = pt_book_walk(&b, PT_SIDE_ASK, 2000, &avg, &used);
    PT_ASSERT(filled == 120 + 300 + 50); /* 470, whole ask side */
    PT_ASSERT(used == 3);
}

PT_T(book_mid_and_imbalance)
{
    pt_book_t b;
    seed_book(&b);
    double mid;
    PT_ASSERT(pt_book_mid(&b, &mid) == 0);
    PT_ASSERT_NEAR(mid, 0.470, 0.0005);
    /* L1 imbalance = (150 - 120)/(150+120) = 30/270 = 0.1111 */
    double imb = pt_book_imbalance(&b, 1);
    PT_ASSERT_NEAR(imb, 30.0/270.0, 0.001);
}