#include "net/pt_http_server.h"
#include "util/pt_clock.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void set_nonblock_(int fd)
{
    int fl = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static void send_response_(int fd, int code, const char *status_txt,
                           const char *content_type, const char *body)
{
    char hdr[512];
    size_t body_len = body ? strlen(body) : 0;
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n\r\n",
        code, status_txt, content_type, body_len);
    write(fd, hdr, (size_t)hlen);
    if (body && body_len > 0)
        write(fd, body, body_len);
}

static void handle_client_(int cfd, int events, void *ud)
{
    pt_http_server_t *s = (pt_http_server_t *)ud;
    char buf[2048];
    ssize_t n = read(cfd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        pt_reactor_del(s->reactor, cfd);
        close(cfd);
        return;
    }
    buf[n] = '\0';

    if (strncmp(buf, "OPTIONS", 7) == 0) {
        send_response_(cfd, 204, "No Content", "text/plain", "");
        pt_reactor_del(s->reactor, cfd);
        close(cfd);
        return;
    }

    char body[4096];
    body[0] = '\0';

    if (strncmp(buf, "GET /api/status", 15) == 0 || strncmp(buf, "GET / ", 6) == 0) {
        double uptime_s = (double)(pt_clock_mono_ns() - s->start_time_ns) / 1e9;
        snprintf(body, sizeof(body),
            "{\"status\":\"running\",\"mode\":\"%s\",\"uptime_sec\":%.1f,\"kill_switch_tripped\":%d,\"kill_reason\":\"%s\",\"btc_price\":%.2f}",
            (s->mode == PT_MODE_LIVE) ? "LIVE" : "PAPER",
            uptime_s,
            pt_risk_is_tripped(s->risk),
            s->risk && s->risk->kill_reason ? s->risk->kill_reason : "none",
            s->btc_mid);
        send_response_(cfd, 200, "OK", "application/json", body);
    }
    else if (strncmp(buf, "GET /api/telemetry", 18) == 0) {
        if (s->telemetry) pt_telemetry_json(s->telemetry, body, sizeof(body));
        else snprintf(body, sizeof(body), "{}");
        send_response_(cfd, 200, "OK", "application/json", body);
    }
    else if (strncmp(buf, "GET /api/portfolio", 18) == 0) {
        if (s->portfolio) {
            snprintf(body, sizeof(body),
                "{\"cash\":%.2f,\"equity\":%.2f,\"realized_pnl\":%.2f,\"unrealized_pnl\":%.2f,\"total_exposure\":%.2f,\"trades\":%llu,\"win_rate\":%.4f}",
                s->portfolio->cash,
                pt_portfolio_equity(s->portfolio),
                s->portfolio->realized_pnl,
                s->portfolio->unrealized_pnl,
                s->portfolio->total_exposure,
                (unsigned long long)s->portfolio->trades_count,
                pt_portfolio_win_rate(s->portfolio));
        } else {
            snprintf(body, sizeof(body), "{}");
        }
        send_response_(cfd, 200, "OK", "application/json", body);
    }
    else if (strncmp(buf, "GET /api/book", 13) == 0) {
        pt_price_t y_bid=0, y_ask=0, n_bid=0, n_ask=0;
        pt_size_t y_bsz=0, y_asz=0, n_bsz=0, n_asz=0;
        if (s->yes_book) {
            pt_book_best_bid(s->yes_book, &y_bid, &y_bsz);
            pt_book_best_ask(s->yes_book, &y_ask, &y_asz);
        }
        if (s->no_book) {
            pt_book_best_bid(s->no_book, &n_bid, &n_bsz);
            pt_book_best_ask(s->no_book, &n_ask, &n_asz);
        }
        snprintf(body, sizeof(body),
            "{\"yes\":{\"best_bid\":%.3f,\"bid_size\":%llu,\"best_ask\":%.3f,\"ask_size\":%llu},"
            "\"no\":{\"best_bid\":%.3f,\"bid_size\":%llu,\"best_ask\":%.3f,\"ask_size\":%llu}}",
            (double)y_bid/PT_PRICE_SCALE, (unsigned long long)y_bsz,
            (double)y_ask/PT_PRICE_SCALE, (unsigned long long)y_asz,
            (double)n_bid/PT_PRICE_SCALE, (unsigned long long)n_bsz,
            (double)n_ask/PT_PRICE_SCALE, (unsigned long long)n_asz);
        send_response_(cfd, 200, "OK", "application/json", body);
    }
    else if (strncmp(buf, "POST /api/kill", 14) == 0) {
        pt_risk_trip_kill(s->risk, "manual_dashboard_kill");
        send_response_(cfd, 200, "OK", "application/json", "{\"status\":\"tripped\"}");
    }
    else if (strncmp(buf, "POST /api/resume", 16) == 0) {
        pt_risk_reset_kill(s->risk);
        send_response_(cfd, 200, "OK", "application/json", "{\"status\":\"resumed\"}");
    }
    else {
        send_response_(cfd, 404, "Not Found", "application/json", "{\"error\":\"not_found\"}");
    }

    pt_reactor_del(s->reactor, cfd);
    close(cfd);
}

static void on_accept_(int sfd, int events, void *ud)
{
    pt_http_server_t *s = (pt_http_server_t *)ud;
    struct sockaddr_in cli;
    socklen_t clen = sizeof(cli);
    int cfd = accept(sfd, (struct sockaddr *)&cli, &clen);
    if (cfd >= 0) {
        set_nonblock_(cfd);
        pt_reactor_add(s->reactor, cfd, PT_EV_READ, handle_client_, s);
    }
}

int pt_http_server_init(pt_http_server_t *s, int port, pt_reactor_t *reactor,
                         pt_portfolio_t *portf, pt_telemetry_t *telem,
                         pt_risk_engine_t *risk, const pt_book_t *yes_book,
                         const pt_book_t *no_book, pt_mode_t mode)
{
    if (!s || !reactor) return -1;
    memset(s, 0, sizeof(*s));
    s->port          = port;
    s->reactor       = reactor;
    s->portfolio     = portf;
    s->telemetry     = telem;
    s->risk          = risk;
    s->yes_book      = yes_book;
    s->no_book       = no_book;
    s->mode          = mode;
    s->start_time_ns = pt_clock_mono_ns();

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblock_(fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 64) != 0) {
        close(fd);
        return -1;
    }

    s->server_fd = fd;
    pt_reactor_add(reactor, fd, PT_EV_READ, on_accept_, s);
    return 0;
}

void pt_http_server_stop(pt_http_server_t *s)
{
    if (s && s->server_fd >= 0) {
        if (s->reactor) pt_reactor_del(s->reactor, s->server_fd);
        close(s->server_fd);
        s->server_fd = -1;
    }
}

void pt_http_server_update_btc(pt_http_server_t *s, double btc_mid)
{
    if (s) s->btc_mid = btc_mid;
}