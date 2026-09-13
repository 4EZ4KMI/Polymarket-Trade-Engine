#include "storage/pt_csv_log.h"
#include <sys/stat.h>
#include <string.h>

static void mkdir_p_(const char *dir)
{
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", dir);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

int pt_csv_init(pt_csv_logger_t *c, const char *log_dir)
{
    if (c == NULL || log_dir == NULL) return -1;
    memset(c, 0, sizeof(*c));
    mkdir_p_(log_dir);

    char path[512];
    snprintf(path, sizeof(path), "%s/trades.csv", log_dir);
    c->trades_fp = fopen(path, "a");
    if (c->trades_fp && ftell(c->trades_fp) == 0) {
        fprintf(c->trades_fp, "ts_ns,trade_id,strategy,market_id,is_yes,side,size,price,pnl,latency_ms\n");
    }

    snprintf(path, sizeof(path), "%s/signals.csv", log_dir);
    c->signals_fp = fopen(path, "a");
    if (c->signals_fp && ftell(c->signals_fp) == 0) {
        fprintf(c->signals_fp, "ts_ns,signal_id,strategy,market_id,dir,is_yes,target_price,expected_edge,expected_profit,confidence,fill_prob,max_size,risk_score\n");
    }

    snprintf(path, sizeof(path), "%s/rejections.csv", log_dir);
    c->rejections_fp = fopen(path, "a");
    if (c->rejections_fp && ftell(c->rejections_fp) == 0) {
        fprintf(c->rejections_fp, "ts_ns,signal_id,reason,val\n");
    }
    return (c->trades_fp && c->signals_fp && c->rejections_fp) ? 0 : -1;
}

void pt_csv_close(pt_csv_logger_t *c)
{
    if (c == NULL) return;
    if (c->trades_fp) fclose(c->trades_fp);
    if (c->signals_fp) fclose(c->signals_fp);
    if (c->rejections_fp) fclose(c->rejections_fp);
    memset(c, 0, sizeof(*c));
}

void pt_csv_log_trade(pt_csv_logger_t *c, pt_nsec_t t, uint64_t trade_id,
                      int strategy, pt_market_id_t m, int is_yes, int side,
                      pt_size_t size, pt_price_t price, double pnl,
                      double latency_ms)
{
    if (!c || !c->trades_fp) return;
    fprintf(c->trades_fp, "%llu,%llu,%d,%llu,%d,%d,%llu,%lld,%.4f,%.3f\n",
            (unsigned long long)t, (unsigned long long)trade_id, strategy,
            (unsigned long long)m, is_yes, side, (unsigned long long)size,
            (long long)price, pnl, latency_ms);
    fflush(c->trades_fp);
}

void pt_csv_log_signal(pt_csv_logger_t *c, const pt_signal_t *s)
{
    if (!c || !c->signals_fp || !s) return;
    fprintf(c->signals_fp, "%llu,%llu,%d,%llu,%d,%d,%lld,%.6f,%.4f,%.4f,%.4f,%llu,%.4f\n",
            (unsigned long long)s->timestamp, (unsigned long long)s->signal_id,
            s->strategy, (unsigned long long)s->market_id, (int)s->direction,
            s->is_yes, (long long)s->target_price, s->expected_edge,
            s->expected_profit, s->confidence, s->fill_probability,
            (unsigned long long)s->max_size, s->risk_score);
    fflush(c->signals_fp);
}

void pt_csv_log_rejection(pt_csv_logger_t *c, pt_nsec_t t, uint64_t signal_id,
                          const char *reason, double val)
{
    if (!c || !c->rejections_fp) return;
    fprintf(c->rejections_fp, "%llu,%llu,%s,%.4f\n",
            (unsigned long long)t, (unsigned long long)signal_id,
            reason ? reason : "unknown", val);
    fflush(c->rejections_fp);
}