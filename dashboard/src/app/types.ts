export interface EngineStatus {
  status: string;
  mode: string;
  uptime_sec: number;
  kill_switch_tripped: number;
  kill_reason: string;
  btc_price: number;
  market_status?: string;
  market_slug?: string;
  condition_id?: string;
  strike?: number;
  has_live_market?: number;
}

export interface Portfolio {
  cash: number;
  equity: number;
  realized_pnl: number;
  unrealized_pnl: number;
  total_exposure: number;
  trades: number;
  win_rate: number;
}

export interface OrderBook {
  yes: { best_bid: number; bid_size: number; best_ask: number; ask_size: number };
  no: { best_bid: number; bid_size: number; best_ask: number; ask_size: number };
}

export interface Telemetry {
  poly_events: number;
  btc_events: number;
  signals: number;
  orders_submitted: number;
  orders_filled: number;
  orders_cancelled: number;
  ring_drops: number;
  latency_e2e_avg_us: number;
  latency_e2e_p50_us: number;
  latency_e2e_p90_us: number;
  latency_e2e_p99_us: number;
  latency_e2e_max_us: number;
}

export interface StrategyStat {
  signals: number;
  would_trade: number;
  risk_approved: number;
  orders_submitted: number;
  fills: number;
  partial_fills: number;
  cancelled: number;
  wins: number;
  losses: number;
  win_rate: number;
  gross_profit: number;
  gross_loss: number;
  realized_pnl: number;
  fees: number;
  rebates: number;
  net_pnl: number;
  average_expected_edge: number;
  average_executable_edge: number;
  average_realized_edge: number;
  fill_rate: number;
}

export interface ExecutionAnalytics {
  expected_edge_avg: number;
  executable_edge_avg: number;
  realized_edge_avg: number;
  fill_ratio: number;
  adverse_selection_bps: number;
  avg_slippage_bps: number;
  avg_queue_ahead: number;
  brier_score: number;
  calibration_error: number;
  profit_factor: number;
  sharpe: number;
  has_sharpe?: number;
  total_trades?: number;
  strat_a?: StrategyStat;
  strat_b?: StrategyStat;
}

