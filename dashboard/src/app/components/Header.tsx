import { Zap, ShieldAlert, RefreshCw } from "lucide-react";
import { EngineStatus } from "../types";

interface HeaderProps {
  status: EngineStatus | null;
  connected: boolean;
  loading: boolean;
  onKill: () => void;
  onResume: () => void;
}

export function Header({ status, connected, loading, onKill, onResume }: HeaderProps) {
  return (
    <header className="flex flex-col md:flex-row items-start md:items-center justify-between bg-[#121824] p-4 rounded-xl border border-[#1f293d] gap-4">
      <div className="flex items-center gap-3">
        <div className="p-2.5 bg-cyan-500/10 border border-cyan-500/30 rounded-lg text-cyan-400">
          <Zap className="w-6 h-6" />
        </div>
        <div>
          <h1 className="text-xl font-bold tracking-wider text-white flex items-center gap-2">
            POLYMARKET HFT ENGINE
            <span className="text-xs px-2 py-0.5 rounded-full bg-cyan-500/20 text-cyan-300 border border-cyan-500/40 font-mono">
              C11 CORE
            </span>
          </h1>
          <p className="text-xs text-gray-400">
            5m Parity Arbitrage & 15m BTC Flow Skew • Zero-Copy Low-Latency Reactor
          </p>
        </div>
      </div>

      <div className="flex items-center gap-3 flex-wrap">
        <div className="flex items-center gap-2 px-3 py-1.5 rounded-lg bg-[#0b0e14] border border-[#1f293d] text-xs">
          <span
            className={`w-2 h-2 rounded-full ${
              connected ? "bg-emerald-400 animate-pulse" : "bg-rose-500"
            }`}
          />
          <span className="text-gray-300">
            {connected ? "CORE CONNECTED" : "CORE OFFLINE"}
          </span>
        </div>

        <div className="px-3 py-1.5 rounded-lg bg-[#0b0e14] border border-amber-500/30 text-xs font-semibold text-amber-400">
          MODE: {status?.mode || "PAPER"}
        </div>

        {status?.kill_switch_tripped ? (
          <button
            onClick={onResume}
            disabled={loading}
            className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg bg-emerald-600 hover:bg-emerald-500 text-white text-xs font-bold transition-all"
          >
            <RefreshCw className="w-3.5 h-3.5" />
            RESUME TRADING
          </button>
        ) : (
          <button
            onClick={onKill}
            disabled={loading}
            className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg bg-rose-600 hover:bg-rose-500 text-white text-xs font-bold transition-all shadow-lg shadow-rose-900/40"
          >
            <ShieldAlert className="w-3.5 h-3.5" />
            EMERGENCY KILL SWITCH
          </button>
        )}
      </div>
    </header>
  );
}