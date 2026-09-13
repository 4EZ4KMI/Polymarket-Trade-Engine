"use client";

import { useEffect, useState } from "react";
import { XCircle } from "lucide-react";
import { Header } from "./components/Header";
import { MetricCards } from "./components/MetricCards";
import { OrderBookView, TelemetryView } from "./components/OrderBookView";
import { EngineStatus, Portfolio, OrderBook, Telemetry } from "./types";

const API_BASE = "http://localhost:8080";

export default function DashboardPage() {
  const [status, setStatus] = useState<EngineStatus | null>(null);
  const [portfolio, setPortfolio] = useState<Portfolio | null>(null);
  const [book, setBook] = useState<OrderBook | null>(null);
  const [telemetry, setTelemetry] = useState<Telemetry | null>(null);
  const [connected, setConnected] = useState(false);
  const [loading, setLoading] = useState(false);

  const fetchData = async () => {
    try {
      const [resStatus, resPort, resBook, resTelem] = await Promise.all([
        fetch(`${API_BASE}/api/status`).then((r) => r.json()).catch(() => null),
        fetch(`${API_BASE}/api/portfolio`).then((r) => r.json()).catch(() => null),
        fetch(`${API_BASE}/api/book`).then((r) => r.json()).catch(() => null),
        fetch(`${API_BASE}/api/telemetry`).then((r) => r.json()).catch(() => null),
      ]);

      if (resStatus) {
        setStatus(resStatus);
        setPortfolio(resPort);
        setBook(resBook);
        setTelemetry(resTelem);
        setConnected(true);
      } else {
        setConnected(false);
      }
    } catch {
      setConnected(false);
    }
  };

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 500);
    return () => clearInterval(interval);
  }, []);

  const triggerKill = async () => {
    setLoading(true);
    await fetch(`${API_BASE}/api/kill`, { method: "POST" }).catch(() => null);
    await fetchData();
    setLoading(false);
  };

  const resumeTrading = async () => {
    setLoading(true);
    await fetch(`${API_BASE}/api/resume`, { method: "POST" }).catch(() => null);
    await fetchData();
    setLoading(false);
  };

  return (
    <main className="p-6 max-w-7xl mx-auto space-y-6">
      <Header
        status={status}
        connected={connected}
        loading={loading}
        onKill={triggerKill}
        onResume={resumeTrading}
      />

      {status?.kill_switch_tripped ? (
        <div className="p-4 bg-rose-950/60 border border-rose-600/50 rounded-xl flex items-center justify-between">
          <div className="flex items-center gap-3 text-rose-400">
            <XCircle className="w-5 h-5 flex-shrink-0" />
            <div>
              <div className="font-bold text-sm">EMERGENCY KILL SWITCH TRIPPED</div>
              <div className="text-xs text-rose-300/80">
                Reason: {status.kill_reason} • All trading halted, in-flight orders cancelled.
              </div>
            </div>
          </div>
        </div>
      ) : null}

      <MetricCards portfolio={portfolio} status={status} telemetry={telemetry} />

      <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
        <OrderBookView book={book} />
        <TelemetryView book={book} telemetry={telemetry} status={status} />
      </div>
    </main>
  );
}