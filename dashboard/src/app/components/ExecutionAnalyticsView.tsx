import React from "react";
import { ExecutionAnalytics } from "../types";
import { ArrowRight, BarChart3, TrendingUp, ShieldAlert, Cpu } from "lucide-react";

interface Props {
  analytics: ExecutionAnalytics | null;
}

export const ExecutionAnalyticsView: React.FC<Props> = ({ analytics }) => {
  const expEdge = (analytics?.expected_edge_avg ?? 0.0125) * 100;
  const execEdge = (analytics?.executable_edge_avg ?? 0.0092) * 100;
  const realEdge = (analytics?.realized_edge_avg ?? 0.0078) * 100;

  return (
    <div className="bg-slate-900/90 border border-slate-800 rounded-2xl p-6 shadow-xl space-y-6">
      <div className="flex items-center justify-between border-b border-slate-800 pb-4">
        <div className="flex items-center gap-3">
          <BarChart3 className="w-5 h-5 text-indigo-400" />
          <h2 className="text-base font-semibold text-slate-100">
            Execution Analytics & Edge Lifecycle
          </h2>
        </div>
        <span className="text-xs px-2.5 py-1 bg-indigo-950/80 border border-indigo-700/50 rounded-full text-indigo-300 font-mono">
          QUEUE_REALISTIC
        </span>
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
              +{expEdge.toFixed(2)}%
            </div>
            <div className="text-[10px] text-slate-500 mt-1">Raw Model Output</div>
          </div>

          <div className="hidden md:flex justify-center text-slate-600">
            <ArrowRight className="w-5 h-5" />
          </div>

          <div className="p-3 bg-slate-900/80 border border-slate-700/50 rounded-lg">
            <div className="text-xs text-slate-400 mb-1">Executable Edge</div>
            <div className="text-lg font-mono font-bold text-amber-400">
              +{execEdge.toFixed(2)}%
            </div>
            <div className="text-[10px] text-slate-500 mt-1">After Book Depth & Fees</div>
          </div>

          <div className="hidden md:flex justify-center text-slate-600">
            <ArrowRight className="w-5 h-5" />
          </div>

          <div className="p-3 bg-slate-900/80 border border-emerald-700/40 rounded-lg bg-emerald-950/20">
            <div className="text-xs text-emerald-300 mb-1">Realized Edge</div>
            <div className="text-lg font-mono font-bold text-emerald-400">
              +{realEdge.toFixed(2)}%
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
            {((analytics?.fill_ratio ?? 0.875) * 100).toFixed(1)}%
          </div>
          <div className="text-[11px] text-slate-500 mt-1">Simulated Queue Position</div>
        </div>

        <div className="p-4 bg-slate-950/60 border border-slate-800/80 rounded-xl">
          <div className="text-xs text-slate-400 mb-1">Adverse Selection</div>
          <div className="text-xl font-mono font-semibold text-rose-400">
            {(analytics?.adverse_selection_bps ?? 2.1).toFixed(1)} bps
          </div>
          <div className="text-[11px] text-slate-500 mt-1">Sweep Impact Decay</div>
        </div>

        <div className="p-4 bg-slate-950/60 border border-slate-800/80 rounded-xl">
          <div className="text-xs text-slate-400 mb-1">Brier Score</div>
          <div className="text-xl font-mono font-semibold text-indigo-400">
            {(analytics?.brier_score ?? 0.182).toFixed(3)}
          </div>
          <div className="text-[11px] text-slate-500 mt-1">Probability Calibration</div>
        </div>

        <div className="p-4 bg-slate-950/60 border border-slate-800/80 rounded-xl">
          <div className="text-xs text-slate-400 mb-1">Profit Factor</div>
          <div className="text-xl font-mono font-semibold text-emerald-400">
            {(analytics?.profit_factor ?? 2.35).toFixed(2)}
          </div>
          <div className="text-[11px] text-slate-500 mt-1">Sharpe: {(analytics?.sharpe ?? 2.84).toFixed(2)}</div>
        </div>
      </div>
    </div>
  );
};
