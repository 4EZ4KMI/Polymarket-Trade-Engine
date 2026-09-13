#ifndef PMT_PT_CSV_LOG_H
#define PMT_PT_CSV_LOG_H

#include "core/ptypes.h"
#include "strategies/pt_signal.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    FILE *trades_fp;
    FILE *signals_fp;
    FILE *rejections_fp;
} pt_csv_logger_t;

int  pt_csv_init(pt_csv_logger_t *c, const char *log_dir);
void pt_csv_close(pt_csv_logger_t *c);

void pt_csv_log_trade(pt_csv_logger_t *c, pt_nsec_t t, uint64_t trade_id,
                      int strategy, pt_market_id_t m, int is_yes, int side,
                      pt_size_t size, pt_price_t price, double pnl,
                      double latency_ms);

void pt_csv_log_signal(pt_csv_logger_t *c, const pt_signal_t *s);

void pt_csv_log_rejection(pt_csv_logger_t *c, pt_nsec_t t, uint64_t signal_id,
                          const char *reason, double val);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_CSV_LOG_H */