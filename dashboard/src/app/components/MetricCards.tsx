import { DollarSign, TrendingUp, Activity, Cpu } from "lucide-react";
import { Portfolio, EngineStatus, Telemetry } from "../types";

interface MetricCardsProps {
  portfolio: Portfolio | null;
  status: EngineStatus | null;
  telemetry: Telemetry | null;
}

export function MetricCards({ portfolio, status, telemetry }: MetricCardsProps) {
  const hasBtc = (status?.btc_price ?? 0) > 0;
  const hasLatency = (telemetry?.latency_e2e_p50_us ?? 0) > 0;
  const totalTrades = portfolio?.trades ?? 0;

  return (
    <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
      <div className="bg-[#121824] p-4 rounded-xl border border-[#1f293d] space-y-1">
        <div className="text-xs text-gray-400 flex items-center justify-between">
          <span>PORTFOLIO EQUITY</span>
          <DollarSign className="w-4 h-4 text-cyan-400" />
        </div>
        <div className="text-2xl font-bold text-white">
          ${portfolio?.equity !== undefined ? portfolio.equity.toFixed(2) : "--"}
        </div>
        <div className="text-xs text-gray-500">
          Cash: ${portfolio?.cash !== undefined ? portfolio.cash.toFixed(2) : "--"}
        </div>
      </div>

      <div className="bg-[#121824] p-4 rounded-xl border border-[#1f293d] space-y-1">
        <div className="text-xs text-gray-400 flex items-center justify-between">
          <span>REALIZED PnL</span>
          <TrendingUp className="w-4 h-4 text-emerald-400" />
        </div>
        <div className={`text-2xl font-bold ${(portfolio?.realized_pnl || 0) >= 0 ? "text-emerald-400" : "text-rose-400"}`}>
          ${portfolio?.realized_pnl !== undefined ? portfolio.realized_pnl.toFixed(2) : "0.00"}
        </div>
        <div className="text-xs text-gray-500">
          Trades: {totalTrades} • WinRate: {totalTrades > 0 && portfolio?.win_rate !== undefined ? `${(portfolio.win_rate * 100).toFixed(1)}%` : "--"}
        </div>
      </div>

      <div className="bg-[#121824] p-4 rounded-xl border border-[#1f293d] space-y-1">
        <div className="text-xs text-gray-400 flex items-center justify-between">
          <span>BINANCE BTC SPOT</span>
          <Activity className="w-4 h-4 text-amber-400" />
        </div>
        <div className="text-2xl font-bold text-amber-400">
          {hasBtc ? `$${status!.btc_price.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}` : "WAITING FOR FEED"}
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
          {hasLatency ? `${telemetry!.latency_e2e_p50_us.toFixed(1)} µs` : "--"}
        </div>
        <div className="text-xs text-gray-500">
          {hasLatency ? `p90: ${telemetry!.latency_e2e_p90_us.toFixed(1)} µs • max: ${telemetry!.latency_e2e_max_us.toFixed(1)} µs` : "Awaiting engine ticks"}
        </div>
      </div>
    </div>
  );
}