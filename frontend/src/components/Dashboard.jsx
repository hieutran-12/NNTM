import React, { useEffect, useState, lazy, Suspense } from "react";
const HistoryChart = lazy(() => import("./HistoryChart"));

const API_BASE = import.meta.env.VITE_API_BASE || "http://localhost:8000";

const STALE_MINUTES = 5; // consider data stale after this many minutes

function LatestCard({ data }) {
  if (!data || Object.keys(data).length === 0)
    return <div className="card">Không có dữ liệu</div>;

  const moistureValueRaw =
    typeof data.soil_moisture === "number" ? data.soil_moisture : null;
  const moistureValue =
    moistureValueRaw !== null ? moistureValueRaw.toFixed(1) : "—";

  // freshness
  let created = null;
  try {
    created = data.created_at ? new Date(data.created_at) : null;
  } catch (e) {
    created = null;
  }
  const now = new Date();
  const ageMs = created ? now.getTime() - created.getTime() : Infinity;
  const isStale = ageMs === Infinity || ageMs > STALE_MINUTES * 60 * 1000;

  return (
    <div className="card">
      <h3>Dữ liệu mới nhất</h3>
      <div>Thiết bị: {data.device_id}</div>
      <div>
        Độ ẩm đất hiện tại: {isStale ? "—" : `${moistureValue}%`}
        {isStale ? (
          <span style={{ color: "#f1c40f", marginLeft: 8 }}>
            (Dữ liệu cũ: {created ? Math.round(ageMs / 60000) : "?"} phút)
          </span>
        ) : null}
      </div>
      <div>pH: {data.ph ?? "—"}</div>
      <div>Độ dẫn điện: {data.conductivity ?? "—"}</div>
      <div>
        Thời gian:{" "}
        {new Date(data.created_at).toLocaleString("vi-VN", {
          timeZone: "Asia/Ho_Chi_Minh",
          year: "numeric",
          month: "2-digit",
          day: "2-digit",
          hour: "2-digit",
          minute: "2-digit",
        })}
      </div>
    </div>
  );
}

export default function Dashboard() {
  const [latest, setLatest] = useState(null);
  const [history, setHistory] = useState([]);
  const [watering, setWatering] = useState([]);
  const [pumpState, setPumpState] = useState({ pump_on: false });
  const [loading, setLoading] = useState(true);
  const [initialized, setInitialized] = useState(false);

  async function load({ force = false } = {}) {
    if (force || !initialized) setLoading(true);
    try {
      const [lRes, hRes, wRes, pRes] = await Promise.all([
        fetch(`${API_BASE}/api/latest`).then((r) => r.json()),
        fetch(`${API_BASE}/api/history?limit=20`).then((r) => r.json()),
        fetch(`${API_BASE}/api/watering-history?limit=20`).then((r) =>
          r.json(),
        ),
        fetch(`${API_BASE}/api/pump/state`).then((r) => r.json()),
      ]);

      // Treat latest as null when it's stale so UI shows null instead of old value
      try {
        let shouldSetLatest = true;
        if (lRes && lRes.created_at) {
          const created = new Date(lRes.created_at);
          const ageMs = Date.now() - created.getTime();
          if (ageMs > STALE_MINUTES * 60 * 1000) {
            // stale -> treat as no data
            if (latest !== null) setLatest(null);
            shouldSetLatest = false;
          }
        } else {
          if (latest !== null) setLatest(null);
          shouldSetLatest = false;
        }
        if (shouldSetLatest && JSON.stringify(lRes) !== JSON.stringify(latest))
          setLatest(lRes);
      } catch (err) {
        if (JSON.stringify(lRes) !== JSON.stringify(latest)) setLatest(lRes);
      }
      if (JSON.stringify(hRes) !== JSON.stringify(history)) setHistory(hRes);
      if (JSON.stringify(wRes) !== JSON.stringify(watering)) setWatering(wRes);
      if (JSON.stringify(pRes || {}) !== JSON.stringify(pumpState))
        setPumpState(pRes || {});
    } catch (e) {
      console.error(e);
    } finally {
      if (force || !initialized) setLoading(false);
      if (!initialized) setInitialized(true);
    }
  }

  // Full refresh (history, watering, pump state) every 60s
  useEffect(() => {
    load();
    const t = setInterval(() => load(), 60000);
    return () => clearInterval(t);
  }, []);

  // Fast poll just for latest every 3s to appear realtime without touching history
  useEffect(() => {
    let mounted = true;
    async function loadLatest() {
      try {
        const res = await fetch(`${API_BASE}/api/latest`);
        const lRes = await res.json();
        if (!mounted) return;
        // Treat empty response as null
        if (!lRes || Object.keys(lRes).length === 0) {
          if (latest !== null) setLatest(null);
          return;
        }
        // parse times
        const newCreated = lRes.created_at
          ? new Date(lRes.created_at).getTime()
          : 0;
        const curCreated =
          latest && latest.created_at
            ? new Date(latest.created_at).getTime()
            : 0;
        if (newCreated > curCreated) {
          setLatest(lRes);
        }
      } catch (err) {
        console.error("loadLatest error", err);
      }
    }

    loadLatest();
    const fast = setInterval(loadLatest, 3000);
    return () => {
      mounted = false;
      clearInterval(fast);
    };
  }, [latest]);

  async function sendPump(command = "PUMP_ON", duration = 10) {
    try {
      await fetch(`${API_BASE}/api/pump`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          device_id: latest?.device_id || "ESP8266",
          command,
          duration,
        }),
      });
      await load();
    } catch (e) {
      console.error(e);
    }
  }

  return (
    <div className="dashboard">
      <div className="controls">
        <button
          onClick={() => sendPump("PUMP_ON", 10)}
          className={
            pumpState && pumpState.pump_on
              ? "btn primary disabled"
              : "btn primary"
          }
          disabled={pumpState && pumpState.pump_on}
        >
          {pumpState && pumpState.pump_on ? "Bơm đang bật" : "Bật bơm 10s"}
        </button>
        <button onClick={() => sendPump("PUMP_OFF")} className="btn">
          Tắt bơm
        </button>
        <button onClick={() => load({ force: true })} className="btn">
          Làm mới
        </button>
      </div>

      {loading ? (
        <div>Đang tải...</div>
      ) : (
        <div className="grid">
          <LatestCard data={latest} />

          <div style={{ minWidth: 300, minHeight: 200 }}>
            <Suspense fallback={<div>Đang tải biểu đồ...</div>}>
              <ErrorBoundary>
                <HistoryChart data={history} />
              </ErrorBoundary>
            </Suspense>
          </div>

          <div className="card">
            <h3>Trạng thái bơm</h3>
            <div>
              Trạng thái: {pumpState && pumpState.pump_on ? "BẬT" : "TẮT"}
            </div>
            <div>
              Thời gian thay đổi:{" "}
              {pumpState && pumpState.last_changed
                ? new Date(pumpState.last_changed).toLocaleString("vi-VN", {
                    timeZone: "Asia/Ho_Chi_Minh",
                    year: "numeric",
                    month: "2-digit",
                    day: "2-digit",
                    hour: "2-digit",
                    minute: "2-digit",
                  })
                : "—"}
            </div>
            <div style={{ marginTop: 8 }}>
              <button
                onClick={() => sendPump("PUMP_ON", 10)}
                className={
                  pumpState && pumpState.pump_on
                    ? "btn primary disabled"
                    : "btn primary"
                }
                disabled={pumpState && pumpState.pump_on}
              >
                {pumpState && pumpState.pump_on
                  ? "Bơm đang bật"
                  : "Bật bơm 10s"}
              </button>
              <button
                onClick={() => sendPump("PUMP_OFF")}
                className="btn"
                style={{ marginLeft: 8 }}
              >
                Tắt bơm
              </button>
            </div>
          </div>

          <div className="card">
            <h3>Lịch sử</h3>
            <table>
              <thead>
                <tr>
                  <th>Thời gian</th>
                  <th>Độ ẩm</th>
                  <th>pH</th>
                  <th>Độ dẫn điện</th>
                </tr>
              </thead>
              <tbody>
                {history.map((r, i) => (
                  <tr key={i}>
                    <td>
                      {new Date(r.created_at).toLocaleString("vi-VN", {
                        timeZone: "Asia/Ho_Chi_Minh",
                        year: "numeric",
                        month: "2-digit",
                        day: "2-digit",
                        hour: "2-digit",
                        minute: "2-digit",
                      })}
                    </td>
                    <td>{r.soil_moisture ?? "—"}</td>
                    <td>{r.ph ?? "—"}</td>
                    <td>{r.conductivity ?? "—"}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>

          <div className="card">
            <h3>Lịch sử tưới</h3>
            <ul className="watering-list">
              {watering.map((w, i) => (
                <li key={i}>
                  {new Date(w.created_at).toLocaleString("vi-VN", {
                    timeZone: "Asia/Ho_Chi_Minh",
                    year: "numeric",
                    month: "2-digit",
                    day: "2-digit",
                    hour: "2-digit",
                    minute: "2-digit",
                  })}{" "}
                  — {w.command} ({w.duration ?? "—"}s)
                </li>
              ))}
            </ul>
          </div>
        </div>
      )}
    </div>
  );
}

class ErrorBoundary extends React.Component {
  constructor(props) {
    super(props);
    this.state = { hasError: false };
  }

  static getDerivedStateFromError() {
    return { hasError: true };
  }

  componentDidCatch(err, info) {
    console.error("ErrorBoundary caught:", err, info);
  }

  render() {
    if (this.state.hasError) {
      return <div>Biểu đồ hiện không khả dụng.</div>;
    }
    return this.props.children;
  }
}
