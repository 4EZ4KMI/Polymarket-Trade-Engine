#ifndef PMT_PT_HTTP_SERVER_H
#define PMT_PT_HTTP_SERVER_H

#include "core/ptypes.h"
#include "net/pt_reactor.h"
#include "portfolio/pt_portfolio.h"
#include "telemetry/pt_telemetry.h"
#include "risk/pt_risk.h"
#include "orderbook/pt_book.h"
#include "analytics/pt_strategy_stats.h"
#include "core/pt_market_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int                          server_fd;
    int                          port;
    pt_reactor_t                *reactor;
    pt_portfolio_t              *portfolio;
    pt_telemetry_t              *telemetry;
    pt_risk_engine_t            *risk;
    const pt_book_t             *yes_book;
    const pt_book_t             *no_book;
    pt_strategy_stats_tracker_t *stats;
    pt_market_registry_t        *registry;
    double                       btc_mid;
    pt_mode_t                    mode;
    pt_nsec_t                    start_time_ns;
} pt_http_server_t;

int  pt_http_server_init(pt_http_server_t *s, int port, pt_reactor_t *reactor,
                         pt_portfolio_t *portf, pt_telemetry_t *telem,
                         pt_risk_engine_t *risk, const pt_book_t *yes_book,
                         const pt_book_t *no_book, pt_mode_t mode);

void pt_http_server_set_stats(pt_http_server_t *s, pt_strategy_stats_tracker_t *stats);
void pt_http_server_set_registry(pt_http_server_t *s, pt_market_registry_t *registry);
void pt_http_server_stop(pt_http_server_t *s);
void pt_http_server_update_btc(pt_http_server_t *s, double btc_mid);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_HTTP_SERVER_H */