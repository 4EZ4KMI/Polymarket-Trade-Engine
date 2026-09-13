#include "net/pt_feed_bridge.h"
#include "util/pt_clock.h"
#include "util/pt_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>

static void set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void on_client_data(int fd, int events, void *ud);
static void on_accept(int fd, int events, void *ud);

int pt_feed_bridge_init(pt_feed_bridge_t *b,
                        pt_reactor_t *reactor,
                        int tcp_port,
                        pt_book_t *yes_book,
                        pt_book_t *no_book,
                        pt_btc_t *btc_engine,
                        pt_risk_engine_t *risk,
                        pt_telemetry_t *telemetry,
                        pt_dataset_writer_t *dataset_writer,
                        double *last_btc_price)
{
    if (!b || !reactor) return -1;
    memset(b, 0, sizeof(*b));
    b->reactor = reactor;
    b->yes_book = yes_book;
    b->no_book = no_book;
    b->btc_engine = btc_engine;
    b->risk = risk;
    b->telemetry = telemetry;
    b->dataset_writer = dataset_writer;
    b->last_btc_price = last_btc_price;
    b->client_fd = -1;

    b->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (b->listen_fd < 0) return -1;

    int opt = 1;
    setsockopt(b->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblock(b->listen_fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)tcp_port);

    if (bind(b->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(b->listen_fd);
        b->listen_fd = -1;
        return -1;
    }

    if (listen(b->listen_fd, 8) != 0) {
        close(b->listen_fd);
        b->listen_fd = -1;
        return -1;
    }

    pt_reactor_add(reactor, b->listen_fd, PT_EV_READ, on_accept, b);
    printf("[FEED BRIDGE] Listening for 100%% real market feeds on TCP port %d\n", tcp_port);
    return 0;
}

void pt_feed_bridge_close(pt_feed_bridge_t *b)
{
    if (!b) return;
    if (b->client_fd >= 0) {
        pt_reactor_del(b->reactor, b->client_fd);
        close(b->client_fd);
        b->client_fd = -1;
    }
    if (b->listen_fd >= 0) {
        pt_reactor_del(b->reactor, b->listen_fd);
        close(b->listen_fd);
        b->listen_fd = -1;
    }
}

static void on_accept(int fd, int events, void *ud)
{
    pt_feed_bridge_t *b = (pt_feed_bridge_t *)ud;
    if (!b) return;

    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int client = accept(fd, (struct sockaddr *)&client_addr, &len);
    if (client < 0) return;

    if (b->client_fd >= 0) {
        pt_reactor_del(b->reactor, b->client_fd);
        close(b->client_fd);
    }

    b->client_fd = client;
    b->buf_len = 0;
    set_nonblock(client);
    pt_reactor_add(b->reactor, client, PT_EV_READ, on_client_data, b);
    printf("[FEED BRIDGE] Real data feeder connected (fd=%d)\n", client);
}

static void on_client_data(int fd, int events, void *ud)
{
    pt_feed_bridge_t *b = (pt_feed_bridge_t *)ud;
    if (!b) return;

    if (events & (PT_EV_ERROR)) {
        pt_reactor_del(b->reactor, fd);
        close(fd);
        b->client_fd = -1;
        return;
    }

    char tmp[4096];
    ssize_t n = read(fd, tmp, sizeof(tmp));
    if (n <= 0) {
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            pt_reactor_del(b->reactor, fd);
            close(fd);
            b->client_fd = -1;
            printf("[FEED BRIDGE] Real data feeder disconnected\n");
        }
        return;
    }

    pt_nsec_t now = pt_clock_mono_ns();

    if (b->buf_len + (size_t)n > sizeof(b->buf) - 1) {
        b->buf_len = 0;
    }
    memcpy(b->buf + b->buf_len, tmp, (size_t)n);
    b->buf_len += (size_t)n;
    b->buf[b->buf_len] = '\0';

    char *start = b->buf;
    char *eol;
    while ((eol = strchr(start, '\n')) != NULL) {
        *eol = '\0';
        size_t line_len = (size_t)(eol - start);
        if (line_len > 0) {
            pt_feed_bridge_on_line(b, start, line_len, now);
        }
        start = eol + 1;
    }

    size_t processed = (size_t)(start - b->buf);
    if (processed < b->buf_len) {
        memmove(b->buf, start, b->buf_len - processed);
        b->buf_len -= processed;
    } else {
        b->buf_len = 0;
    }
}

void pt_feed_bridge_on_line(pt_feed_bridge_t *b, const char *line, size_t len, pt_nsec_t now)
{
    if (!b || !line || len == 0) return;

    pt_binance_trade_t btr;
    if (pt_parse_binance_trade(line, len, &btr)) {
        b->total_btc_events++;
        if (b->telemetry) b->telemetry->btc_events_total++;
        if (b->btc_engine) pt_btc_add(b->btc_engine, now, btr.price);
        if (b->last_btc_price) *b->last_btc_price = btr.price;
        if (b->risk) pt_risk_feed_touch_binance(b->risk, now);

        if (b->dataset_writer) {
            pt_dataset_event_t ev = {
                .event_type = PT_DATA_EV_BTC_TICK,
                .timestamp_ns = now,
                .btc_mid = btr.price
            };
            pt_dataset_writer_append(b->dataset_writer, &ev);
        }
        return;
    }

    pt_poly_delta_t pd;
    if (pt_parse_polymarket_book_msg(line, len, &pd)) {
        b->total_poly_events++;
        if (b->telemetry) b->telemetry->poly_events_total++;
        if (b->risk) pt_risk_feed_touch_poly(b->risk, now);

        int is_yes = (strstr(pd.asset_id, "yes") || strstr(pd.asset_id, "YES") || pd.asset_id[0] == '1' || pd.asset_id[0] == '\0');
        pt_book_t *target = is_yes ? b->yes_book : b->no_book;

        if (target) {
            if (pd.is_snapshot) {
                pt_level_t lvl = { .price = pd.price, .size = pd.size };
                pt_book_snapshot(target, pd.side, &lvl, 1, 1);
            } else {
                pt_book_update(target, pd.side, pd.price, pd.size, 1, 1);
            }
        }

        if (b->dataset_writer) {
            pt_dataset_event_t ev = {
                .event_type = PT_DATA_EV_POLY_DELTA,
                .timestamp_ns = now,
                .is_yes = (uint8_t)is_yes,
                .side = (uint8_t)pd.side,
                .price = pd.price,
                .size = pd.size
            };
            pt_dataset_writer_append(b->dataset_writer, &ev);
        }
        return;
    }
}

