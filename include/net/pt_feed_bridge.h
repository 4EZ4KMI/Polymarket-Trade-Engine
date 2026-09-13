#ifndef PMT_PT_FEED_BRIDGE_H
#define PMT_PT_FEED_BRIDGE_H

#include "net/pt_reactor.h"
#include "net/pt_feed_parsers.h"
#include "orderbook/pt_book.h"
#include "features/pt_features.h"
#include "risk/pt_risk.h"
#include "telemetry/pt_telemetry.h"
#include "storage/pt_dataset.h"
#include "core/pt_market_registry.h"
#include "portfolio/pt_portfolio.h"
#include "execution/pt_broker.h"
#include "analytics/pt_strategy_stats.h"
#include "analytics/pt_adverse_selection.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int                          listen_fd;
    int                          client_fd;
    char                         buf[65536];
    size_t                       buf_len;
    pt_reactor_t                *reactor;
    pt_book_t                   *yes_book;
    pt_book_t                   *no_book;
    pt_btc_t                    *btc_engine;
    pt_risk_engine_t            *risk;
    pt_telemetry_t              *telemetry;
    pt_dataset_writer_t         *dataset_writer;
    double                      *last_btc_price;
    pt_market_registry_t        *registry;
    pt_portfolio_t              *portfolio;
    pt_broker_t                 *broker;
    pt_strategy_stats_tracker_t *stats;
    pt_lifecycle_tracker_t      *lifecycle;
    pt_adverse_tracker_t        *adverse;
    uint64_t                     total_poly_events;
    uint64_t                     total_btc_events;
    uint64_t                     total_discovery_events;
    uint64_t                     total_resolution_events;
} pt_feed_bridge_t;

/* Initialize feed bridge listening on TCP port */
int pt_feed_bridge_init(pt_feed_bridge_t *b,
                        pt_reactor_t *reactor,
                        int tcp_port,
                        pt_book_t *yes_book,
                        pt_book_t *no_book,
                        pt_btc_t *btc_engine,
                        pt_risk_engine_t *risk,
                        pt_telemetry_t *telemetry,
                        pt_dataset_writer_t *dataset_writer,
                        double *last_btc_price,
                        pt_market_registry_t *registry,
                        pt_portfolio_t *portfolio,
                        pt_broker_t *broker,
                        pt_strategy_stats_tracker_t *stats,
                        pt_lifecycle_tracker_t *lifecycle,
                        pt_adverse_tracker_t *adverse);

void pt_feed_bridge_close(pt_feed_bridge_t *b);

/* Process a single raw JSON line from real market feeds */
void pt_feed_bridge_on_line(pt_feed_bridge_t *b, const char *line, size_t len, pt_nsec_t now);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_FEED_BRIDGE_H */

