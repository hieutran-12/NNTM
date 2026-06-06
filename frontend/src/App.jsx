import React from "react";
import Dashboard from "./components/Dashboard";

export default function App() {
  return (
    <div className="app">
      <header className="header">
        <h1>Bảng điều khiển giám sát đất</h1>
      </header>
      <main>
        <Dashboard />
      </main>
    </div>
  );
}
