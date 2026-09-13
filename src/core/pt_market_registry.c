#include "core/pt_market_registry.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>

void pt_market_registry_init(pt_market_registry_t *reg)
{
    if (!reg) return;
    memset(reg, 0, sizeof(*reg));
    reg->active_market_id = 0;
    reg->active_5m_id = 0;
    reg->active_15m_id = 0;
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
    if (!condition_id || condition_id[0] == '\0') return NULL;
    if (!yes_token_id || yes_token_id[0] == '\0') return NULL;
    if (!no_token_id || no_token_id[0] == '\0') return NULL;
    if (strcmp(yes_token_id, no_token_id) == 0) return NULL;

    int duration = 0;
    if (end_time_ns > start_time_ns) {
        duration = (int)((end_time_ns - start_time_ns) / 1000000000ULL);
    }

    for (size_t i = 0; i < reg->count; i++) {
        if (reg->markets[i].market_id == market_id ||
            strcmp(reg->markets[i].condition_id, condition_id) == 0) {
            pt_market_entry_t *m = &reg->markets[i];
            if (slug) strncpy(m->slug, slug, sizeof(m->slug) - 1);
            if (yes_token_id) strncpy(m->yes_token_id, yes_token_id, sizeof(m->yes_token_id) - 1);
            if (no_token_id) strncpy(m->no_token_id, no_token_id, sizeof(m->no_token_id) - 1);
            if (strike > 0.0) m->strike = strike;
            if (start_time_ns > 0) m->start_time_ns = start_time_ns;
            if (end_time_ns > 0) m->end_time_ns = end_time_ns;
            if (duration > 0) m->duration_sec = duration;
            m->eligible_for_strategy_a = (m->duration_sec >= 240 && m->duration_sec <= 450) ? 1 : 0;
            m->eligible_for_strategy_b = (m->duration_sec >= 750 && m->duration_sec <= 1200) ? 1 : 0;
            return m;
        }
    }

    if (reg->count >= PT_MAX_MARKETS) return NULL;

    pt_market_entry_t *m = &reg->markets[reg->count++];
    memset(m, 0, sizeof(*m));
    m->market_id = market_id ? market_id : (pt_market_id_t)reg->count;
    strncpy(m->condition_id, condition_id, sizeof(m->condition_id) - 1);
    if (slug) strncpy(m->slug, slug, sizeof(m->slug) - 1);
    strncpy(m->yes_token_id, yes_token_id, sizeof(m->yes_token_id) - 1);
    strncpy(m->no_token_id, no_token_id, sizeof(m->no_token_id) - 1);
    m->strike = strike;
    m->start_time_ns = start_time_ns;
    m->end_time_ns = end_time_ns;
    m->duration_sec = duration;
    m->eligible_for_strategy_a = (duration >= 240 && duration <= 450) ? 1 : 0;
    m->eligible_for_strategy_b = (duration >= 750 && duration <= 1200) ? 1 : 0;
    m->state = PT_MKT_STATE_DISCOVERED;
    m->active_flag = 1;
    m->resolved_winner = -1;
    m->discovered_time_ns = now;

    if (reg->active_market_id == 0) {
        reg->active_market_id = m->market_id;
    }
    if (m->eligible_for_strategy_a && reg->active_5m_id == 0) {
        reg->active_5m_id = m->market_id;
    }
    if (m->eligible_for_strategy_b && reg->active_15m_id == 0) {
        reg->active_15m_id = m->market_id;
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
        if (m && m->state != PT_MKT_STATE_CLOSED && m->state != PT_MKT_STATE_RESOLVED && m->state != PT_MKT_STATE_PENDING_RESOLUTION) {
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

pt_market_entry_t *pt_market_registry_get_active_5m(pt_market_registry_t *reg)
{
    if (!reg || reg->count == 0) return NULL;
    if (reg->active_5m_id != 0) {
        pt_market_entry_t *m = pt_market_registry_find(reg, reg->active_5m_id);
        if (m && m->eligible_for_strategy_a && m->state != PT_MKT_STATE_CLOSED && m->state != PT_MKT_STATE_RESOLVED && m->state != PT_MKT_STATE_PENDING_RESOLUTION) {
            return m;
        }
    }
    for (size_t i = 0; i < reg->count; i++) {
        if (reg->markets[i].eligible_for_strategy_a &&
            (reg->markets[i].state == PT_MKT_STATE_ACTIVE ||
             reg->markets[i].state == PT_MKT_STATE_SNAPSHOT_RECEIVED ||
             reg->markets[i].state == PT_MKT_STATE_SUBSCRIBING ||
             reg->markets[i].state == PT_MKT_STATE_DISCOVERED)) {
            reg->active_5m_id = reg->markets[i].market_id;
            return &reg->markets[i];
        }
    }
    return NULL;
}

pt_market_entry_t *pt_market_registry_get_active_15m(pt_market_registry_t *reg)
{
    if (!reg || reg->count == 0) return NULL;
    if (reg->active_15m_id != 0) {
        pt_market_entry_t *m = pt_market_registry_find(reg, reg->active_15m_id);
        if (m && m->eligible_for_strategy_b && m->state != PT_MKT_STATE_CLOSED && m->state != PT_MKT_STATE_RESOLVED && m->state != PT_MKT_STATE_PENDING_RESOLUTION) {
            return m;
        }
    }
    for (size_t i = 0; i < reg->count; i++) {
        if (reg->markets[i].eligible_for_strategy_b &&
            (reg->markets[i].state == PT_MKT_STATE_ACTIVE ||
             reg->markets[i].state == PT_MKT_STATE_SNAPSHOT_RECEIVED ||
             reg->markets[i].state == PT_MKT_STATE_SUBSCRIBING ||
             reg->markets[i].state == PT_MKT_STATE_DISCOVERED)) {
            reg->active_15m_id = reg->markets[i].market_id;
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

int pt_market_registry_resolve_verified(pt_market_registry_t *reg,
                                         const char *condition_id,
                                         pt_market_id_t market_id,
                                         const char *winning_asset_id,
                                         const char *winning_outcome,
                                         double resolution_price,
                                         pt_nsec_t now,
                                         int *out_winner)
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

    /* Reject duplicate resolution (prevent double-settlement) */
    if (m->state == PT_MKT_STATE_RESOLVED) {
        return -2; /* Already resolved */
    }

    int winner = -1;
    if (winning_asset_id && winning_asset_id[0] != '\0') {
        if (strcmp(m->yes_token_id, winning_asset_id) == 0) {
            winner = 1;
        } else if (strcmp(m->no_token_id, winning_asset_id) == 0) {
            winner = 0;
        } else {
            /* Unknown winning asset! Fail safely: do NOT settle, leave pending */
            m->state = PT_MKT_STATE_PENDING_RESOLUTION;
            return -3;
        }
    } else if (winning_outcome && winning_outcome[0] != '\0') {
        if (strcasecmp(winning_outcome, "YES") == 0 || strcmp(winning_outcome, "1") == 0 || strcasecmp(winning_outcome, "UP") == 0) {
            winner = 1;
        } else if (strcasecmp(winning_outcome, "NO") == 0 || strcmp(winning_outcome, "0") == 0 || strcasecmp(winning_outcome, "DOWN") == 0) {
            winner = 0;
        } else {
            m->state = PT_MKT_STATE_PENDING_RESOLUTION;
            return -3;
        }
    } else {
        m->state = PT_MKT_STATE_PENDING_RESOLUTION;
        return -3;
    }

    m->state = PT_MKT_STATE_RESOLVED;
    m->resolved_winner = winner;
    m->resolution_price = resolution_price > 0.0 ? resolution_price : 1.0;
    m->resolution_time_ns = now;
    m->active_flag = 0;
    if (out_winner) *out_winner = winner;
    return 0;
}

int pt_market_registry_resolve(pt_market_registry_t *reg,
                                const char *condition_id,
                                pt_market_id_t market_id,
                                int winning_is_yes,
                                double resolution_price,
                                pt_nsec_t now)
{
    const char *outcome = winning_is_yes ? "YES" : "NO";
    int dummy_winner = 0;
    return pt_market_registry_resolve_verified(reg, condition_id, market_id, NULL, outcome, resolution_price, now, &dummy_winner);
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
