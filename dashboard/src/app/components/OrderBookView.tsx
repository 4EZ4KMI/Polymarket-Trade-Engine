import { BarChart2, Terminal } from "lucide-react";
import { OrderBook, Telemetry, EngineStatus } from "../types";

interface ViewsProps {
  book: OrderBook | null;
  telemetry: Telemetry | null;
  status: EngineStatus | null;
}

export function OrderBookView({ book }: { book: OrderBook | null }) {
  const hasYesBook = (book?.yes?.best_bid ?? 0) > 0 || (book?.yes?.best_ask ?? 0) > 0;
  const hasNoBook = (book?.no?.best_bid ?? 0) > 0 || (book?.no?.best_ask ?? 0) > 0;
  const hasLiveBooks = hasYesBook && hasNoBook;

  const paritySum = (book?.yes?.best_ask || 0) + (book?.no?.best_ask || 0);
  const arbEdge = (hasLiveBooks && paritySum > 0) ? (1.0 - paritySum) * 100 : 0;

  return (
    <div className="md:col-span-2 bg-[#121824] p-5 rounded-xl border border-[#1f293d] space-y-4">
      <div className="flex items-center justify-between">
        <h2 className="text-sm font-bold text-white flex items-center gap-2">
          <BarChart2 className="w-4 h-4 text-cyan-400" />
          POLYMARKET BTC 5m/15m DUAL L2 ORDER BOOK
        </h2>
        <div className="text-xs px-2.5 py-1 rounded bg-[#0b0e14] border border-[#1f293d] text-gray-300">
          {hasLiveBooks ? (
            <>
              Parity Sum: <span className="font-bold text-white">${paritySum.toFixed(3)}</span>
              {arbEdge > 0.0 ? (
                <span className="ml-2 text-emerald-400 font-bold">
                  (Arb Edge: +{arbEdge.toFixed(2)}%)
                </span>
              ) : (
                <span className="ml-2 text-gray-500">(No Arb)</span>
              )}
            </>
          ) : (
            <span className="text-amber-400 font-mono">WAITING FOR LIVE MARKET</span>
          )}
        </div>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        {/* YES Side */}
        <div className="bg-[#0b0e14] p-3 rounded-lg border border-[#1f293d] space-y-2">
          <div className="flex justify-between items-center text-xs font-bold text-emerald-400 border-b border-[#1f293d] pb-1.5">
            <span>YES TOKEN</span>
            <span>Best Bid / Ask</span>
          </div>
          <div className="flex justify-between items-center text-xs py-1">
            <span className="text-gray-400">Best Bid:</span>
            <span className="font-bold text-emerald-400">
              {hasYesBook && (book?.yes?.best_bid ?? 0) > 0 ? `$${book!.yes.best_bid.toFixed(3)} (${book!.yes.bid_size} shs)` : "--"}
            </span>
          </div>
          <div className="flex justify-between items-center text-xs py-1">
            <span className="text-gray-400">Best Ask:</span>
            <span className="font-bold text-rose-400">
              {hasYesBook && (book?.yes?.best_ask ?? 0) > 0 ? `$${book!.yes.best_ask.toFixed(3)} (${book!.yes.ask_size} shs)` : "--"}
            </span>
          </div>
          <div className="w-full bg-[#121824] h-2 rounded-full overflow-hidden flex">
            <div className="bg-emerald-500 h-full" style={{ width: hasYesBook ? "50%" : "0%" }} />
            <div className="bg-rose-500 h-full" style={{ width: hasYesBook ? "50%" : "0%" }} />
          </div>
        </div>

        {/* NO Side */}
        <div className="bg-[#0b0e14] p-3 rounded-lg border border-[#1f293d] space-y-2">
          <div className="flex justify-between items-center text-xs font-bold text-indigo-400 border-b border-[#1f293d] pb-1.5">
            <span>NO TOKEN</span>
            <span>Best Bid / Ask</span>
          </div>
          <div className="flex justify-between items-center text-xs py-1">
            <span className="text-gray-400">Best Bid:</span>
            <span className="font-bold text-emerald-400">
              {hasNoBook && (book?.no?.best_bid ?? 0) > 0 ? `$${book!.no.best_bid.toFixed(3)} (${book!.no.bid_size} shs)` : "--"}
            </span>
          </div>
          <div className="flex justify-between items-center text-xs py-1">
            <span className="text-gray-400">Best Ask:</span>
            <span className="font-bold text-rose-400">
              {hasNoBook && (book?.no?.best_ask ?? 0) > 0 ? `$${book!.no.best_ask.toFixed(3)} (${book!.no.ask_size} shs)` : "--"}
            </span>
          </div>
          <div className="w-full bg-[#121824] h-2 rounded-full overflow-hidden flex">
            <div className="bg-emerald-500 h-full" style={{ width: hasNoBook ? "50%" : "0%" }} />
            <div className="bg-rose-500 h-full" style={{ width: hasNoBook ? "50%" : "0%" }} />
          </div>
        </div>
      </div>
    </div>
  );
}

export function TelemetryView({ telemetry, status }: ViewsProps) {
  return (
    <div className="bg-[#121824] p-5 rounded-xl border border-[#1f293d] space-y-3">
      <h2 className="text-sm font-bold text-white flex items-center gap-2">
        <Terminal className="w-4 h-4 text-cyan-400" />
        ENGINE TELEMETRY
      </h2>
      <div className="space-y-2 text-xs">
        <div className="flex justify-between text-gray-400">
          <span>Market Events:</span>
          <span className="font-mono text-gray-200">{(telemetry?.poly_events || 0) + (telemetry?.btc_events || 0)}</span>
        </div>
        <div className="flex justify-between text-gray-400">
          <span>Signals Evaluated:</span>
          <span className="font-mono text-cyan-400">{telemetry?.signals || 0}</span>
        </div>
        <div className="flex justify-between text-gray-400">
          <span>Orders Placed:</span>
          <span className="font-mono text-gray-200">{telemetry?.orders_submitted || 0}</span>
        </div>
        <div className="flex justify-between text-gray-400">
          <span>Orders Filled:</span>
          <span className="font-mono text-emerald-400">{telemetry?.orders_filled || 0}</span>
        </div>
        <div className="flex justify-between text-gray-400">
          <span>Ring Buffer Drops:</span>
          <span className="font-mono text-emerald-400">{telemetry?.ring_drops || 0} (0%)</span>
        </div>
        <div className="flex justify-between text-gray-400">
          <span>Uptime:</span>
          <span className="font-mono text-gray-200">{status?.uptime_sec ? status.uptime_sec.toFixed(0) : "0"}s</span>
        </div>
      </div>
    </div>
  );
}