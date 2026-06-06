import React from "react";
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
} from "chart.js";
import { Line } from "react-chartjs-2";

ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
);

export default function HistoryChart({ data }) {
  const points = 12;
  const stepMs = 5 * 60 * 1000;
  const now = new Date();
  const start = new Date(now.getTime() - points * stepMs);
  start.setSeconds(0, 0);

  const buckets = [];
  for (let i = 0; i <= points; i += 1) {
    const ts = new Date(start.getTime() + i * stepMs);
    buckets.push(ts);
  }

  const historyByBucket = new Map();
  (data || []).forEach((r) => {
    if (!r || !r.created_at) return;
    const ts = new Date(r.created_at);
    const diff = ts.getTime() - start.getTime();
    if (diff < 0 || diff > points * stepMs) return;
    const index = Math.round(diff / stepMs);
    if (index < 0 || index > points) return;
    const value = typeof r.soil_moisture === "number" ? r.soil_moisture : 0;
    const existing = historyByBucket.get(index);
    if (existing === undefined || value > existing) {
      historyByBucket.set(index, value);
    }
  });

  const labels = buckets.map((date) =>
    date.toLocaleString("vi-VN", {
      timeZone: "Asia/Ho_Chi_Minh",
      hour: "2-digit",
      minute: "2-digit",
    }),
  );

  const values = buckets.map((_, index) => historyByBucket.get(index) ?? 0);

  // Ensure the most recent sensor value (if any) is reflected on the right edge
  const nonZeroValues = values.filter((v) => typeof v === "number");
  const maxVal = nonZeroValues.length ? Math.max(...nonZeroValues) : 0;
  const suggestedMax = Math.max(100, Math.ceil(maxVal / 10) * 10 + 10);

  const chartData = {
    labels,
    datasets: [
      {
        label: "Độ ẩm đất (%)",
        data: values,
        borderColor: "#2b8a3e",
        backgroundColor: "rgba(43,138,62,0.15)",
        tension: 0.3,
        pointRadius: 0,
        borderWidth: 2,
        spanGaps: true,
        fill: true,
      },
      {
        label: "Hiện tại",
        data: values.map((_, i) =>
          i === values.length - 1 ? values[i] : null,
        ),
        borderColor: "#fff",
        backgroundColor: "#fff",
        pointRadius: 4,
        showLine: false,
      },
    ],
  };

  const options = {
    responsive: true,
    plugins: {
      legend: { position: "top" },
      title: { display: true, text: "Biểu đồ độ ẩm theo thời gian" },
      tooltip: {
        callbacks: {
          label: function (context) {
            return `${context.parsed.y} %`;
          },
        },
      },
    },
    scales: {
      y: {
        title: { display: true, text: "% độ ẩm" },
        beginAtZero: true,
        suggestedMin: 0,
        suggestedMax: suggestedMax,
        ticks: { stepSize: 10 },
        grid: { color: "rgba(255,255,255,0.08)", drawBorder: true },
      },
      x: {
        ticks: { maxRotation: 0, autoSkip: true, maxTicksLimit: 10 },
        grid: { color: "rgba(255,255,255,0.05)" },
      },
    },
  };

  return (
    <div className="card">
      <h3>Biểu đồ lịch sử</h3>
      <Line options={options} data={chartData} />
    </div>
  );
}
