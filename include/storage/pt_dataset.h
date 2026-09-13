#ifndef PMT_PT_DATASET_H
#define PMT_PT_DATASET_H

#include "core/ptypes.h"
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PMT_DATASET_MAGIC 0x504D544453455431ULL /* "PMTDSET1" */
#define PMT_DATASET_VERSION 1

typedef enum {
    PT_DATA_EV_POLY_DELTA = 1,
    PT_DATA_EV_POLY_SNAP  = 2,
    PT_DATA_EV_POLY_TRADE = 3,
    PT_DATA_EV_BTC_TICK   = 4,
    PT_DATA_EV_BTC_TRADE  = 5,
    PT_DATA_EV_LIFECYCLE  = 6
} pt_data_ev_type_t;

#pragma pack(push, 1)
typedef struct {
    uint64_t magic;
    uint32_t version;
    uint32_t reserved;
    uint64_t start_ts_ns;
    uint64_t end_ts_ns;
    uint64_t total_events;
} pt_dataset_header_t;

typedef struct {
    uint8_t    event_type;     /* pt_data_ev_type_t */
    pt_nsec_t  timestamp_ns;
    uint64_t   market_id;
    uint8_t    is_yes;
    uint8_t    side;           /* PT_SIDE_BID / PT_SIDE_ASK */
    pt_price_t price;          /* scaled */
    pt_size_t  size;
    double     btc_mid;
    double     btc_bid;
    double     btc_ask;
} pt_dataset_event_t;
#pragma pack(pop)

typedef struct {
    int                 fd;
    pt_dataset_header_t header;
    void               *mmap_base;
    size_t              file_size;
    uint64_t            current_idx;
} pt_dataset_reader_t;

typedef struct {
    FILE               *fp;
    pt_dataset_header_t header;
    char                filepath[256];
} pt_dataset_writer_t;

/* Writer interface */
int  pt_dataset_writer_open(pt_dataset_writer_t *w, const char *path);
int  pt_dataset_writer_append(pt_dataset_writer_t *w, const pt_dataset_event_t *ev);
void pt_dataset_writer_close(pt_dataset_writer_t *w);

/* Reader interface */
int  pt_dataset_reader_open(pt_dataset_reader_t *r, const char *path);
int  pt_dataset_reader_next(pt_dataset_reader_t *r, pt_dataset_event_t *ev_out);
void pt_dataset_reader_rewind(pt_dataset_reader_t *r);
void pt_dataset_reader_close(pt_dataset_reader_t *r);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_DATASET_H */
