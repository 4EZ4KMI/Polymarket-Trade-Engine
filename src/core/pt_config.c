#include "core/pt_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void pt_config_set_defaults(pt_engine_config_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));

    cfg->http_port = 8080;
    strncpy(cfg->log_dir, "data/logs", sizeof(cfg->log_dir) - 1);
    cfg->mode = PT_MODE_PAPER;
    cfg->initial_capital = 10000.0;

    /* Execution & Queue */
    cfg->execution.latency_submit_ack_ms = 1.5;
    cfg->execution.latency_ack_fill_ms = 1.0;
    cfg->execution.latency_cancel_ms = 1.5;
    cfg->execution.queue_model = PT_QUEUE_MODEL_REALISTIC;
    cfg->execution.slippage_bps = 1.0;
    cfg->execution.adverse_selection_bps = 2.0;
    cfg->execution.live_trading_enabled = 0; /* HARD LOCK */

    /* Fees */
    pt_fee_schedule_default(&cfg->fees);

    /* Risk */
    cfg->risk.max_position_per_market = 2000.0;
    cfg->risk.max_total_exposure = 10000.0;
    cfg->risk.max_order_size_shares = 1000.0;
    cfg->risk.max_daily_loss = 200.0;
    cfg->risk.max_drawdown = 300.0;
    cfg->risk.max_polymarket_stale_ns = 5000000000ULL;
    cfg->risk.max_binance_stale_ns = 2000000000ULL;
    cfg->risk.max_spread_for_entry = 0.04;
    cfg->risk.max_consecutive_losses = 5;
    cfg->risk.min_confidence_floor = 0.50;
    cfg->risk.max_orders_per_sec = 50;
    cfg->risk.max_latency_ms = 100.0;

    /* Strategy A (Parity Arb) */
    cfg->strategy_arb.fees = cfg->fees;
    cfg->strategy_arb.slippage_bps = 1.0;
    cfg->strategy_arb.latency_buffer_pct = 0.001;
    cfg->strategy_arb.risk_buffer_pct = 0.001;
    cfg->strategy_arb.min_edge_pct = 0.005;
    cfg->strategy_arb.min_liquidity = 10;
    cfg->strategy_arb.max_unhedged_loss = 10.0;

    cfg->arb_mgr.max_loss_per_trade = 10.0;
    cfg->arb_mgr.requote_tolerance = 0.005;
    cfg->arb_mgr.allow_requote = 1;
    cfg->arb_mgr.max_unhedged_time_ms = 500.0;
    cfg->arb_mgr.max_unhedged_size = 500;
    cfg->arb_mgr.hedge_timeout_ms = 1000.0;

    /* Strategy B (Flow Skew) */
    cfg->strategy_flow.model.type = PT_MODEL_HEURISTIC;
    cfg->strategy_flow.min_btc_momentum_pct = 0.0001;
    cfg->strategy_flow.min_btc_velocity = 5.0;
    cfg->strategy_flow.min_polymarket_imbalance = 0.05;
    cfg->strategy_flow.max_polymarket_spread = 0.04;
    cfg->strategy_flow.min_expected_edge = 0.005;
    cfg->strategy_flow.min_confidence = 0.50;
    cfg->strategy_flow.default_order_size = 100;
    cfg->strategy_flow.time_to_expiry_min_sec = 30.0;
    cfg->strategy_flow.time_to_expiry_max_sec = 900.0;
    cfg->strategy_flow.taker_fee_bps = 2.0;
    cfg->strategy_flow.slippage_bps = 1.0;
}

static char *trim_(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

int pt_config_load_file(pt_engine_config_t *cfg, const char *path)
{
    if (!cfg || !path) return -1;
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *p = trim_(line);
        if (*p == '#' || *p == ';' || *p == '[' || *p == '\0') continue;
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim_(p);
        char *val = trim_(eq + 1);

        if (strcmp(key, "http_port") == 0) cfg->http_port = atoi(val);
        else if (strcmp(key, "initial_capital") == 0) cfg->initial_capital = atof(val);
        else if (strcmp(key, "max_daily_loss") == 0) cfg->risk.max_daily_loss = atof(val);
        else if (strcmp(key, "max_drawdown") == 0) cfg->risk.max_drawdown = atof(val);
        else if (strcmp(key, "min_edge_pct") == 0) cfg->strategy_arb.min_edge_pct = atof(val);
        else if (strcmp(key, "taker_fee_bps") == 0) {
            cfg->fees.taker_fee_bps = atof(val);
            cfg->strategy_arb.fees.taker_fee_bps = atof(val);
            cfg->strategy_flow.taker_fee_bps = atof(val);
        }
    }
    fclose(fp);
    return 0;
}

int pt_config_validate(const pt_engine_config_t *cfg, char *err_buf, size_t err_len)
{
    if (!cfg) {
        if (err_buf) snprintf(err_buf, err_len, "Config struct is NULL");
        return -1;
    }
    if (cfg->http_port <= 0 || cfg->http_port > 65535) {
        if (err_buf) snprintf(err_buf, err_len, "Invalid http_port %d", cfg->http_port);
        return -1;
    }
    if (cfg->initial_capital <= 0.0) {
        if (err_buf) snprintf(err_buf, err_len, "Initial capital must be > 0");
        return -1;
    }
    if (cfg->risk.max_daily_loss <= 0.0 || cfg->risk.max_drawdown <= 0.0) {
        if (err_buf) snprintf(err_buf, err_len, "Risk loss limits must be positive numbers");
        return -1;
    }
    if (cfg->execution.live_trading_enabled != 0) {
        if (err_buf) snprintf(err_buf, err_len, "SAFETY VIOLATION: live_trading_enabled must be 0");
        return -1;
    }
    return 0;
}
