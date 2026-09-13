#ifndef PMT_PT_CONFIG_H
#define PMT_PT_CONFIG_H

#include "core/ptypes.h"
#include "execution/pt_broker.h"
#include "execution/pt_arb.h"
#include "execution/pt_arb_mgr.h"
#include "strategies/pt_flow_skew.h"
#include "risk/pt_risk.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int               http_port;
    char              log_dir[128];
    int               feed_port;
    char              dataset_path[128];

    pt_mode_t         mode;
    double            initial_capital;
    
    pt_broker_cfg_t   execution;
    pt_fee_schedule_t fees;
    pt_risk_cfg_t     risk;
    pt_arb_cfg_t      strategy_arb;
    pt_arb_mgr_cfg_t  arb_mgr;
    pt_flow_skew_cfg_t strategy_flow;
} pt_engine_config_t;

/* Set safe defaults for all configuration parameters */
void pt_config_set_defaults(pt_engine_config_t *cfg);

/* Load key-value / INI configuration from file */
int  pt_config_load_file(pt_engine_config_t *cfg, const char *path);

/* Validate all parameters. Returns 0 on success, -1 on dangerous/invalid parameter */
int  pt_config_validate(const pt_engine_config_t *cfg, char *err_buf, size_t err_len);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_CONFIG_H */
