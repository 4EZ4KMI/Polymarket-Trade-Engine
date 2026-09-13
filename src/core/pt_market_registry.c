#include "core/pt_market_registry.h"
#include <string.h>
#include <stdio.h>

void pt_market_registry_init(pt_market_registry_t *reg)
{
    if (!reg) return;
    memset(reg, 0, sizeof(*reg));
    reg->active_market_id = 0;
}

pt_market_entry_t *pt_market_registry_add(pt_market_registry_t *reg,
                                          pt_market_id_t market_id,
                                          const char *condition_id,
                                          const char *slug,
                                          const char *yes_token_id,
                                          const char *no_token_id,
                                          double strike,
                                          pt_nsec_t start_time_ns,
                                          pt_nsec_t end_time_ns,
                                          pt_nsec_t now)
{
    if (!reg) return NULL;

    for (size_t i = 0; i < reg->count; i++) {
        if (reg->markets[i].market_id == market_id ||
            (condition_id && strcmp(reg->markets[i].condition_id, condition_id) == 0)) {
            pt_market_entry_t *m = &reg->markets[i];
            if (slug) strncpy(m->slug, slug, sizeof(m->slug) - 1);
            if (yes_token_id) strncpy(m->yes_token_id, yes_token_id, sizeof(m->yes_token_id) - 1);
            if (no_token_id) strncpy(m->no_token_id, no_token_id, sizeof(m->no_token_id) - 1);
            if (strike > 0.0) m->strike = strike;
            if (start_time_ns > 0) m->start_time_ns = start_time_ns;
            if (end_time_ns > 0) m->end_time_ns = end_time_ns;
            return m;
        }
    }

    if (reg->count >= PT_MAX_MARKETS) return NULL;

    pt_market_entry_t *m = &reg->markets[reg->count++];
    memset(m, 0, sizeof(*m));
    m->market_id = market_id ? market_id : (pt_market_id_t)reg->count;
    if (condition_id) strncpy(m->condition_id, condition_id, sizeof(m->condition_id) - 1);
    if (slug) strncpy(m->slug, slug, sizeof(m->slug) - 1);
    if (yes_token_id) strncpy(m->yes_token_id, yes_token_id, sizeof(m->yes_token_id) - 1);
    if (no_token_id) strncpy(m->no_token_id, no_token_id, sizeof(m->no_token_id) - 1);
    m->strike = strike;
    m->start_time_ns = start_time_ns;
    m->end_time_ns = end_time_ns;
    m->state = PT_MKT_STATE_DISCOVERED;
    m->active_flag = 1;
    m->resolved_winner = -1;
    m->discovered_time_ns = now;

    if (reg->active_market_id == 0) {
        reg->active_market_id = m->market_id;
    }
    return m;
}

pt_market_entry_t *pt_market_registry_find_by_condition(pt_market_registry_t *reg, const char *condition_id)
{
    if (!reg || !condition_id || condition_id[0] == '\0') return NULL;
    for (size_t i = 0; i < reg->count; i++) {
        if (strcmp(reg->markets[i].condition_id, condition_id) == 0) return &reg->markets[i];
    }
    return NULL;
}

pt_market_entry_t *pt_market_registry_find_by_token(pt_market_registry_t *reg, const char *token_id, int *is_yes)
{
    if (!reg || !token_id || token_id[0] == '\0') return NULL;
    for (size_t i = 0; i < reg->count; i++) {
        if (strcmp(reg->markets[i].yes_token_id, token_id) == 0) {
            if (is_yes) *is_yes = 1;
            return &reg->markets[i];
        }
        if (strcmp(reg->markets[i].no_token_id, token_id) == 0) {
            if (is_yes) *is_yes = 0;
            return &reg->markets[i];
        }
    }
    return NULL;
}

pt_market_entry_t *pt_market_registry_get_active(pt_market_registry_t *reg)
{
    if (!reg || reg->count == 0) return NULL;
    if (reg->active_market_id != 0) {
        pt_market_entry_t *m = pt_market_registry_find(reg, reg->active_market_id);
        if (m && m->state != PT_MKT_STATE_CLOSED && m->state != PT_MKT_STATE_RESOLVED) {
            return m;
        }
    }
    for (size_t i = 0; i < reg->count; i++) {
        if (reg->markets[i].state == PT_MKT_STATE_ACTIVE ||
            reg->markets[i].state == PT_MKT_STATE_SNAPSHOT_RECEIVED ||
            reg->markets[i].state == PT_MKT_STATE_SUBSCRIBING ||
            reg->markets[i].state == PT_MKT_STATE_DISCOVERED) {
            reg->active_market_id = reg->markets[i].market_id;
            return &reg->markets[i];
        }
    }
    return NULL;
}

int pt_market_registry_set_state(pt_market_registry_t *reg, pt_market_id_t market_id, pt_mkt_state_t state)
{
    pt_market_entry_t *m = pt_market_registry_find(reg, market_id);
    if (!m) return -1;
    m->state = state;
    return 0;
}

void pt_market_registry_on_snapshot(pt_market_registry_t *reg, pt_market_id_t market_id, pt_nsec_t now)
{
    pt_market_entry_t *m = pt_market_registry_find(reg, market_id);
    if (!m) return;
    m->snapshot_time_ns = now;
    if (m->state == PT_MKT_STATE_DISCOVERED || m->state == PT_MKT_STATE_SUBSCRIBING) {
        m->state = PT_MKT_STATE_ACTIVE;
    }
}

int pt_market_registry_resolve(pt_market_registry_t *reg,
                                const char *condition_id,
                                pt_market_id_t market_id,
                                int winning_is_yes,
                                double resolution_price,
                                pt_nsec_t now)
{
    if (!reg) return -1;
    pt_market_entry_t *m = NULL;
    if (condition_id && condition_id[0] != '\0') {
        m = pt_market_registry_find_by_condition(reg, condition_id);
    }
    if (!m && market_id > 0) {
        m = pt_market_registry_find(reg, market_id);
    }
    if (!m) return -1;

    m->state = PT_MKT_STATE_RESOLVED;
    m->resolved_winner = winning_is_yes ? 1 : 0;
    m->resolution_price = resolution_price;
    m->resolution_time_ns = now;
    m->active_flag = 0;
    return 0;
}

int pt_market_entry_can_trade(const pt_market_entry_t *m, pt_nsec_t now)
{
    if (!m) return 0;
    if (m->state != PT_MKT_STATE_ACTIVE && m->state != PT_MKT_STATE_SNAPSHOT_RECEIVED) return 0;
    if (m->start_time_ns > 0 && now < m->start_time_ns) return 0;
    if (m->end_time_ns > 0) {
        if (now + 5000000000ULL >= m->end_time_ns) return 0;
    }
    return 1;
}

double pt_market_entry_time_to_expiry_sec(const pt_market_entry_t *m, pt_nsec_t now)
{
    if (!m || m->end_time_ns == 0 || now >= m->end_time_ns) return 0.0;
    return (double)(m->end_time_ns - now) / 1e9;
}

const char *pt_mkt_state_to_str(pt_mkt_state_t st)
{
    switch (st) {
        case PT_MKT_STATE_EMPTY: return "EMPTY";
        case PT_MKT_STATE_DISCOVERED: return "DISCOVERED";
        case PT_MKT_STATE_SUBSCRIBING: return "SUBSCRIBING";
        case PT_MKT_STATE_SNAPSHOT_RECEIVED: return "SNAPSHOT_RECEIVED";
        case PT_MKT_STATE_ACTIVE: return "ACTIVE";
        case PT_MKT_STATE_EXPIRING: return "EXPIRING";
        case PT_MKT_STATE_RESOLVED: return "RESOLVED";
        case PT_MKT_STATE_CLOSED: return "CLOSED";
        default: return "UNKNOWN";
    }
}

pt_market_entry_t *pt_market_registry_find(pt_market_registry_t *reg, pt_market_id_t id)
{
    if (!reg || id == 0) return NULL;
    for (size_t i = 0; i < reg->count; i++) {
        if (reg->markets[i].market_id == id) return &reg->markets[i];
    }
    return NULL;
}
