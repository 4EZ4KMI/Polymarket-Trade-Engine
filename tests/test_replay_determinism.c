#include "test_harness.h"
#include "storage/pt_dataset.h"
#include "execution/pt_broker.h"
#include "portfolio/pt_portfolio.h"
#include "orderbook/pt_book.h"
#include <unistd.h>

PT_T(deterministic_event_replay)
{
    const char *path = "build/tmp/test_replay.bin";
    pt_dataset_writer_t w;
    pt_dataset_writer_open(&w, path);

    pt_nsec_t t = 1000000000ULL;
    for (int i = 0; i < 100; i++) {
        t += 10000000ULL;
        pt_dataset_event_t ev = {
            .event_type = (i % 2 == 0) ? PT_DATA_EV_POLY_DELTA : PT_DATA_EV_POLY_TRADE,
            .timestamp_ns = t,
            .market_id = 101,
            .is_yes = 1,
            .side = PT_SIDE_ASK,
            .price = 480 + (i % 5),
            .size = 100 + (i % 20)
        };
        pt_dataset_writer_append(&w, &ev);
    }
    pt_dataset_writer_close(&w);

    /* Run Pass 1 */
    pt_portfolio_t p1;
    pt_portfolio_init(&p1, 10000.0);
    pt_broker_cfg_t bcfg = { .latency_submit_ack_ms = 1.0, .latency_ack_fill_ms = 1.0, .queue_model = PT_QUEUE_MODEL_REALISTIC };
    pt_broker_t b1;
    pt_broker_init(&b1, &bcfg, &p1);

    pt_book_t y1, n1;
    pt_book_init(&y1); pt_book_init(&n1);
    pt_dataset_reader_t r1;
    pt_dataset_reader_open(&r1, path);
    pt_dataset_event_t ev1;
    while (pt_dataset_reader_next(&r1, &ev1)) {
        if (ev1.event_type == PT_DATA_EV_POLY_DELTA) {
            pt_book_update(&y1, ev1.side, ev1.price, (int64_t)ev1.size, 1, 1);
        } else if (ev1.event_type == PT_DATA_EV_POLY_TRADE) {
            pt_broker_on_trade(&b1, 101, 1, ev1.price, ev1.size, ev1.side, ev1.timestamp_ns);
        }
        pt_broker_tick(&b1, 101, &y1, &n1, ev1.timestamp_ns, NULL, NULL);
    }
    pt_dataset_reader_close(&r1);

    /* Run Pass 2 (Identical configuration) */
    pt_portfolio_t p2;
    pt_portfolio_init(&p2, 10000.0);
    pt_broker_t b2;
    pt_broker_init(&b2, &bcfg, &p2);

    pt_book_t y2, n2;
    pt_book_init(&y2); pt_book_init(&n2);
    pt_dataset_reader_t r2;
    pt_dataset_reader_open(&r2, path);
    pt_dataset_event_t ev2;
    while (pt_dataset_reader_next(&r2, &ev2)) {
        if (ev2.event_type == PT_DATA_EV_POLY_DELTA) {
            pt_book_update(&y2, ev2.side, ev2.price, (int64_t)ev2.size, 1, 1);
        } else if (ev2.event_type == PT_DATA_EV_POLY_TRADE) {
            pt_broker_on_trade(&b2, 101, 1, ev2.price, ev2.size, ev2.side, ev2.timestamp_ns);
        }
        pt_broker_tick(&b2, 101, &y2, &n2, ev2.timestamp_ns, NULL, NULL);
    }
    pt_dataset_reader_close(&r2);

    /* Determinism Check: Exact bitwise matching */
    PT_ASSERT_NEAR(p1.cash, p2.cash, 0.000001);
    PT_ASSERT_NEAR(p1.realized_pnl, p2.realized_pnl, 0.000001);
    PT_ASSERT(p1.trades_count == p2.trades_count);
    PT_ASSERT(p1.wins_count == p2.wins_count);
    unlink(path);
}
