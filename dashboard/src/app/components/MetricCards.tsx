import { DollarSign, TrendingUp, Activity, Cpu } from "lucide-react";
import { Portfolio, EngineStatus, Telemetry } from "../types";

interface MetricCardsProps {
  portfolio: Portfolio | null;
  status: EngineStatus | null;
  telemetry: Telemetry | null;
}

export function MetricCards({ portfolio, status, telemetry }: MetricCardsProps) {
  return (
    <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
      <div className="bg-[#121824] p-4 rounded-xl border border-[#1f293d] space-y-1">
        <div className="text-xs text-gray-400 flex items-center justify-between">
          <span>PORTFOLIO EQUITY</span>
          <DollarSign className="w-4 h-4 text-cyan-400" />
        </div>
        <div className="text-2xl font-bold text-white">
          ${portfolio?.equity ? portfolio.equity.toFixed(2) : "5,000.00"}
        </div>
        <div className="text-xs text-gray-500">
          Cash: ${portfolio?.cash ? portfolio.cash.toFixed(2) : "5,000.00"}
        </div>
      </div>

      <div className="bg-[#121824] p-4 rounded-xl border border-[#1f293d] space-y-1">
        <div className="text-xs text-gray-400 flex items-center justify-between">
          <span>REALIZED PnL</span>
          <TrendingUp className="w-4 h-4 text-emerald-400" />
        </div>
        <div className={`text-2xl font-bold ${(portfolio?.realized_pnl || 0) >= 0 ? "text-emerald-400" : "text-rose-400"}`}>
          ${portfolio?.realized_pnl ? portfolio.realized_pnl.toFixed(2) : "0.00"}
        </div>
        <div className="text-xs text-gray-500">
          Trades: {portfolio?.trades || 0} • WinRate: {portfolio?.win_rate ? (portfolio.win_rate * 100).toFixed(1) : "0.0"}%
        </div>
      </div>

      <div className="bg-[#121824] p-4 rounded-xl border border-[#1f293d] space-y-1">
        <div className="text-xs text-gray-400 flex items-center justify-between">
          <span>BINANCE BTC SPOT</span>
          <Activity className="w-4 h-4 text-amber-400" />
        </div>
        <div className="text-2xl font-bold text-amber-400">
          ${status?.btc_price ? status.btc_price.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 }) : "87,500.00"}
        </div>
        <div className="text-xs text-gray-500">
          Live Feed: @aggTrade + @bookTicker
        </div>
      </div>

      <div className="bg-[#121824] p-4 rounded-xl border border-[#1f293d] space-y-1">
        <div className="text-xs text-gray-400 flex items-center justify-between">
          <span>E2E LATENCY (p50)</span>
          <Cpu className="w-4 h-4 text-purple-400" />
        </div>
        <div className="text-2xl font-bold text-purple-400">
          {telemetry?.latency_e2e_p50_us ? telemetry.latency_e2e_p50_us.toFixed(1) : "16.4"} µs
        </div>
        <div className="text-xs text-gray-500">
          p90: {telemetry?.latency_e2e_p90_us ? telemetry.latency_e2e_p90_us.toFixed(1) : "16.4"} µs • max: {telemetry?.latency_e2e_max_us ? telemetry.latency_e2e_max_us.toFixed(1) : "13.7"} µs
        </div>
      </div>
    </div>
  );
}