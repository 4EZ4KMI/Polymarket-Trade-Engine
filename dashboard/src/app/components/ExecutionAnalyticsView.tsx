import React from "react";
import { ExecutionAnalytics } from "../types";
import { ArrowRight, BarChart3, Activity, Cpu } from "lucide-react";

interface Props {
  analytics: ExecutionAnalytics | null;
}

export const ExecutionAnalyticsView: React.FC<Props> = ({ analytics }) => {
  const hasData = analytics !== null;
  const expEdge = (analytics?.expected_edge_avg ?? 0.0) * 100;
  const execEdge = (analytics?.executable_edge_avg ?? 0.0) * 100;
  const realEdge = (analytics?.realized_edge_avg ?? 0.0) * 100;
  const stratA = analytics?.strat_a;
  const stratB = analytics?.strat_b;
  const totalTrades = analytics?.total_trades ?? 0;
  const hasSharpe = (analytics?.has_sharpe ?? 0) === 1;

  return (
    <div className="bg-slate-900/90 border border-slate-800 rounded-2xl p-6 shadow-xl space-y-6">
      <div className="flex items-center justify-between border-b border-slate-800 pb-4">
        <div className="flex items-center gap-3">
          <BarChart3 className="w-5 h-5 text-indigo-400" />
          <h2 className="text-base font-semibold text-slate-100">
            Execution Analytics & Strategy Win Rates
          </h2>
        </div>
        <span className="text-xs px-2.5 py-1 bg-indigo-950/80 border border-indigo-700/50 rounded-full text-indigo-300 font-mono">
          REAL_CALIBRATED
        </span>
      </div>

      {/* Strategy Breakdown: Strategy A (5m Parity) vs Strategy B (15m Flow Skew) */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        {/* Strategy A */}
        <div className="p-4 bg-slate-950/80 border border-slate-800 rounded-xl space-y-2">
          <div className="flex items-center justify-between">
            <span className="text-xs font-bold text-sky-400 uppercase tracking-wider flex items-center gap-1.5">
              <Cpu className="w-4 h-4" /> Strategy A (5m Parity)
            </span>
            <span className="text-[11px] font-mono px-2 py-0.5 rounded bg-sky-950 text-sky-300 border border-sky-800/60">
              {stratA?.fills ?? 0} fills / {stratA?.orders_submitted ?? 0} ords ({stratA?.signals ?? 0} sigs)
            </span>
          </div>
          <div className="grid grid-cols-3 gap-2 pt-2">
            <div className="bg-slate-900/90 p-2.5 rounded-lg border border-slate-800/80">
              <div className="text-[10px] text-slate-400 uppercase">Win Rate</div>
              <div className="text-base font-mono font-bold text-sky-300">
                {(stratA && (stratA.wins + stratA.losses > 0)) ? `${(stratA.win_rate * 100).toFixed(1)}%` : "--"}
              </div>
              <div className="text-[10px] text-slate-500">{stratA?.wins ?? 0}W / {stratA?.losses ?? 0}L</div>
            </div>
            <div className="bg-slate-900/90 p-2.5 rounded-lg border border-slate-800/80">
              <div className="text-[10px] text-slate-400 uppercase">Would Trade</div>
              <div className="text-base font-mono font-bold text-slate-200">
                {stratA?.would_trade ?? 0}
              </div>
              <div className="text-[10px] text-slate-500">Approved: {stratA?.risk_approved ?? 0}</div>
            </div>
            <div className="bg-slate-900/90 p-2.5 rounded-lg border border-slate-800/80">
              <div className="text-[10px] text-slate-400 uppercase">Net PnL</div>
              <div className={`text-base font-mono font-bold ${(stratA?.net_pnl ?? 0) >= 0 ? "text-emerald-400" : "text-rose-400"}`}>
                ${(stratA?.net_pnl ?? 0.0).toFixed(2)}
              </div>
              <div className="text-[10px] text-slate-500">Fees: ${(stratA?.fees ?? 0.0).toFixed(2)}</div>
            </div>
          </div>
        </div>

        {/* Strategy B */}
        <div className="p-4 bg-slate-950/80 border border-slate-800 rounded-xl space-y-2">
          <div className="flex items-center justify-between">
            <span className="text-xs font-bold text-purple-400 uppercase tracking-wider flex items-center gap-1.5">
              <Activity className="w-4 h-4" /> Strategy B (15m Flow Skew)
            </span>
            <span className="text-[11px] font-mono px-2 py-0.5 rounded bg-purple-950 text-purple-300 border border-purple-800/60">
              {stratB?.fills ?? 0} fills / {stratB?.orders_submitted ?? 0} ords ({stratB?.signals ?? 0} sigs)
            </span>
          </div>
          <div className="grid grid-cols-3 gap-2 pt-2">
            <div className="bg-slate-900/90 p-2.5 rounded-lg border border-slate-800/80">
              <div className="text-[10px] text-slate-400 uppercase">Win Rate</div>
              <div className="text-base font-mono font-bold text-purple-300">
                {(stratB && (stratB.wins + stratB.losses > 0)) ? `${(stratB.win_rate * 100).toFixed(1)}%` : "--"}
              </div>
              <div className="text-[10px] text-slate-500">{stratB?.wins ?? 0}W / {stratB?.losses ?? 0}L</div>
            </div>
            <div className="bg-slate-900/90 p-2.5 rounded-lg border border-slate-800/80">
              <div className="text-[10px] text-slate-400 uppercase">Would Trade</div>
              <div className="text-base font-mono font-bold text-slate-200">
                {stratB?.would_trade ?? 0}
              </div>
              <div className="text-[10px] text-slate-500">Approved: {stratB?.risk_approved ?? 0}</div>
            </div>
            <div className="bg-slate-900/90 p-2.5 rounded-lg border border-slate-800/80">
              <div className="text-[10px] text-slate-400 uppercase">Net PnL</div>
              <div className={`text-base font-mono font-bold ${(stratB?.net_pnl ?? 0) >= 0 ? "text-emerald-400" : "text-rose-400"}`}>
                ${(stratB?.net_pnl ?? 0.0).toFixed(2)}
              </div>
              <div className="text-[10px] text-slate-500">Fees: ${(stratB?.fees ?? 0.0).toFixed(2)}</div>
            </div>
          </div>
        </div>
      </div>

      {/* Edge Decay Funnel: Expected -> Executable -> Realized */}
      <div className="bg-slate-950/60 border border-slate-800/80 rounded-xl p-5 space-y-3">
        <div className="text-xs font-medium text-slate-400 uppercase tracking-wider">
          Edge Decay Decomposition
        </div>
        <div className="grid grid-cols-1 md:grid-cols-5 items-center gap-4 text-center">
          <div className="p-3 bg-slate-900/80 border border-slate-700/50 rounded-lg">
            <div className="text-xs text-slate-400 mb-1">Expected Edge</div>
            <div className="text-lg font-mono font-bold text-sky-400">
              {hasData && expEdge > 0 ? `+${expEdge.toFixed(2)}%` : "--"}
            </div>
            <div className="text-[10px] text-slate-500 mt-1">Raw Model Output</div>
          </div>

          <div className="hidden md:flex justify-center text-slate-600">
            <ArrowRight className="w-5 h-5" />
          </div>

          <div className="p-3 bg-slate-900/80 border border-slate-700/50 rounded-lg">
            <div className="text-xs text-slate-400 mb-1">Executable Edge</div>
            <div className="text-lg font-mono font-bold text-amber-400">
              {hasData && execEdge > 0 ? `+${execEdge.toFixed(2)}%` : "--"}
            </div>
            <div className="text-[10px] text-slate-500 mt-1">After Book Depth & Fees</div>
          </div>

          <div className="hidden md:flex justify-center text-slate-600">
            <ArrowRight className="w-5 h-5" />
          </div>

          <div className="p-3 bg-slate-900/80 border border-emerald-700/40 rounded-lg bg-emerald-950/20">
            <div className="text-xs text-emerald-300 mb-1">Realized Edge</div>
            <div className="text-lg font-mono font-bold text-emerald-400">
              {hasData && realEdge !== 0 ? `${realEdge >= 0 ? "+" : ""}${realEdge.toFixed(2)}%` : "--"}
            </div>
            <div className="text-[10px] text-emerald-500 mt-1">Post-Fill Settlement</div>
          </div>
        </div>
      </div>

      {/* Secondary Metrics */}
      <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
        <div className="p-4 bg-slate-950/60 border border-slate-800/80 rounded-xl">
          <div className="text-xs text-slate-400 mb-1">Fill Ratio</div>
          <div className="text-xl font-mono font-semibold text-slate-100">
            {hasData && (analytics?.fill_ratio ?? 0) > 0 ? `${((analytics?.fill_ratio ?? 0.0) * 100).toFixed(1)}%` : "--"}
          </div>
          <div className="text-[11px] text-slate-500 mt-1">Real Queue Fills</div>
        </div>

        <div className="p-4 bg-slate-950/60 border border-slate-800/80 rounded-xl">
          <div className="text-xs text-slate-400 mb-1">Adverse Selection</div>
          <div className="text-xl font-mono font-semibold text-rose-400">
            {hasData && (analytics?.adverse_selection_bps ?? 0) > 0 ? `${(analytics?.adverse_selection_bps ?? 0.0).toFixed(1)} bps` : "--"}
          </div>
          <div className="text-[11px] text-slate-500 mt-1">Multi-Horizon Price Decay</div>
        </div>

        <div className="p-4 bg-slate-950/60 border border-slate-800/80 rounded-xl">
          <div className="text-xs text-slate-400 mb-1">Sharpe Ratio</div>
          <div className="text-xl font-mono font-semibold text-indigo-400">
            {hasSharpe ? (analytics?.sharpe ?? 0.0).toFixed(2) : "--"}
          </div>
          <div className="text-[11px] text-slate-500 mt-1">{hasSharpe ? "Annualized Return/Risk" : "Needs >= 5 Settled Trades"}</div>
        </div>

        <div className="p-4 bg-slate-950/60 border border-slate-800/80 rounded-xl">
          <div className="text-xs text-slate-400 mb-1">Profit Factor</div>
          <div className="text-xl font-mono font-semibold text-emerald-400">
            {hasData && (analytics?.profit_factor ?? 0.0) > 0 ? (analytics?.profit_factor ?? 0.0).toFixed(2) : "--"}
          </div>
          <div className="text-[11px] text-slate-500 mt-1">
            {totalTrades > 0 ? `Trades: ${totalTrades}` : "No trades settled yet"}
          </div>
        </div>
      </div>
    </div>
  );
};
