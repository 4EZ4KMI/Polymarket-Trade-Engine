#ifndef PMT_PT_MARKET_REGISTRY_H
#define PMT_PT_MARKET_REGISTRY_H

#include "core/ptypes.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PT_MAX_MARKETS 64

typedef enum {
    PT_MKT_STATE_EMPTY = 0,
    PT_MKT_STATE_DISCOVERED,
    PT_MKT_STATE_SUBSCRIBING,
    PT_MKT_STATE_SNAPSHOT_RECEIVED,
    PT_MKT_STATE_ACTIVE,
    PT_MKT_STATE_EXPIRING,
    PT_MKT_STATE_PENDING_RESOLUTION,
    PT_MKT_STATE_RESOLVED,
    PT_MKT_STATE_CLOSED
} pt_mkt_state_t;

typedef struct {
    pt_market_id_t  market_id;
    char            condition_id[68];
    char            slug[64];
    char            yes_token_id[72];
    char            no_token_id[72];
    double          strike;
    pt_nsec_t       start_time_ns;
    pt_nsec_t       end_time_ns;
    int             duration_sec;
    int             eligible_for_strategy_a; /* 5m Parity Arb */
    int             eligible_for_strategy_b; /* 15m Flow Skew */
    pt_mkt_state_t  state;
    int             active_flag;
    int             resolved_winner; /* 1 = YES, 0 = NO, -1 = UNRESOLVED */
    double          resolution_price;
    pt_nsec_t       resolution_time_ns;
    pt_nsec_t       discovered_time_ns;
    pt_nsec_t       snapshot_time_ns;
} pt_market_entry_t;

typedef struct {
    pt_market_entry_t markets[PT_MAX_MARKETS];
    size_t            count;
    pt_market_id_t    active_market_id;
    pt_market_id_t    active_5m_id;
    pt_market_id_t    active_15m_id;
} pt_market_registry_t;

void pt_market_registry_init(pt_market_registry_t *reg);

/* Register a discovered market from real Polymarket metadata */
pt_market_entry_t *pt_market_registry_add(pt_market_registry_t *reg,
                                          pt_market_id_t market_id,
                                          const char *condition_id,
                                          const char *slug,
                                          const char *yes_token_id,
                                          const char *no_token_id,
                                          double strike,
                                          pt_nsec_t start_time_ns,
                                          pt_nsec_t end_time_ns,
                                          pt_nsec_t now);

/* Lookup functions */
pt_market_entry_t *pt_market_registry_find(pt_market_registry_t *reg, pt_market_id_t id);
pt_market_entry_t *pt_market_registry_find_by_condition(pt_market_registry_t *reg, const char *condition_id);
pt_market_entry_t *pt_market_registry_find_by_token(pt_market_registry_t *reg, const char *token_id, int *is_yes);
pt_market_entry_t *pt_market_registry_get_active(pt_market_registry_t *reg);
pt_market_entry_t *pt_market_registry_get_active_5m(pt_market_registry_t *reg);
pt_market_entry_t *pt_market_registry_get_active_15m(pt_market_registry_t *reg);

/* State transitions */
int  pt_market_registry_set_state(pt_market_registry_t *reg, pt_market_id_t market_id, pt_mkt_state_t state);
void pt_market_registry_on_snapshot(pt_market_registry_t *reg, pt_market_id_t market_id, pt_nsec_t now);

/* Verified real outcome resolution */
int  pt_market_registry_resolve_verified(pt_market_registry_t *reg,
                                         const char *condition_id,
                                         pt_market_id_t market_id,
                                         const char *winning_asset_id,
                                         const char *winning_outcome,
                                         double resolution_price,
                                         pt_nsec_t now,
                                         int *out_winner);

/* Real outcome resolution backward-compatibility wrapper */
int  pt_market_registry_resolve(pt_market_registry_t *reg,
                                const char *condition_id,
                                pt_market_id_t market_id,
                                int winning_is_yes,
                                double resolution_price,
                                pt_nsec_t now);

/* Check if market is tradeable */
int  pt_market_entry_can_trade(const pt_market_entry_t *m, pt_nsec_t now);
double pt_market_entry_time_to_expiry_sec(const pt_market_entry_t *m, pt_nsec_t now);

/* Convert state enum to human readable string */
const char *pt_mkt_state_to_str(pt_mkt_state_t st);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_MARKET_REGISTRY_H */
