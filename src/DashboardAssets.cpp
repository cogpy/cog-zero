/*
 * standalone/src/DashboardAssets.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Embedded HTML/JS dashboard for cog0 MonitoringServer.
 * Self-contained single-page application with no external dependencies.
 */

#include "cog0/DashboardAssets.h"

#include <cstring>

namespace cog0 {

// ==========================================================================
// Dashboard HTML
// ==========================================================================

const char* DASHBOARD_HTML =
R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>cog0 Dashboard</title>
    <style>
        :root {
            --bg-dark: #1a1a2e;
            --bg-card: #16213e;
            --accent: #0f3460;
            --primary: #e94560;
            --text: #eaeaea;
            --text-dim: #8b8b8b;
            --success: #00d26a;
            --warning: #ffc107;
            --error: #dc3545;
        }
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: var(--bg-dark);
            color: var(--text);
            min-height: 100vh;
            padding: 20px;
        }
        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 24px;
            padding-bottom: 16px;
            border-bottom: 1px solid var(--accent);
        }
        .header h1 {
            font-size: 1.8rem;
            font-weight: 600;
            color: var(--primary);
        }
        .status {
            display: flex;
            align-items: center;
            gap: 8px;
        }
        .status-dot {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background: var(--error);
            animation: pulse 2s infinite;
        }
        .status-dot.connected {
            background: var(--success);
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
            gap: 20px;
            margin-bottom: 24px;
        }
        .card {
            background: var(--bg-card);
            border-radius: 12px;
            padding: 20px;
            border: 1px solid var(--accent);
        }
        .card h2 {
            font-size: 0.9rem;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            color: var(--text-dim);
            margin-bottom: 12px;
        }
        .metric {
            font-size: 2.5rem;
            font-weight: 700;
            color: var(--text);
            line-height: 1.2;
        }
        .metric-label {
            font-size: 0.85rem;
            color: var(--text-dim);
            margin-top: 4px;
        }
        .metric-row {
            display: flex;
            justify-content: space-between;
            align-items: baseline;
            padding: 8px 0;
            border-bottom: 1px solid rgba(255,255,255,0.05);
        }
        .metric-row:last-child {
            border-bottom: none;
        }
        .metric-row .label {
            color: var(--text-dim);
        }
        .metric-row .value {
            font-weight: 600;
            font-family: 'SF Mono', Monaco, 'Consolas', monospace;
        }
        .goals-list {
            max-height: 300px;
            overflow-y: auto;
        }
        .goal-item {
            display: flex;
            align-items: center;
            gap: 10px;
            padding: 10px 0;
            border-bottom: 1px solid rgba(255,255,255,0.05);
        }
        .goal-item:last-child {
            border-bottom: none;
        }
        .goal-priority {
            width: 32px;
            height: 32px;
            border-radius: 8px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-weight: 700;
            font-size: 0.8rem;
            flex-shrink: 0;
        }
        .goal-priority.high {
            background: rgba(233, 69, 96, 0.2);
            color: var(--primary);
        }
        .goal-priority.medium {
            background: rgba(255, 193, 7, 0.2);
            color: var(--warning);
        }
        .goal-priority.low {
            background: rgba(0, 210, 106, 0.2);
            color: var(--success);
        }
        .goal-info {
            flex: 1;
            min-width: 0;
        }
        .goal-name {
            font-weight: 500;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }
        .goal-status {
            font-size: 0.8rem;
            color: var(--text-dim);
        }
        .atoms-chart {
            height: 120px;
            display: flex;
            align-items: flex-end;
            gap: 4px;
            padding: 10px 0;
        }
        .atoms-bar {
            flex: 1;
            background: linear-gradient(to top, var(--primary), var(--accent));
            border-radius: 4px 4px 0 0;
            min-height: 4px;
            transition: height 0.3s ease;
        }
        .log-output {
            background: #0d0d1a;
            border-radius: 8px;
            padding: 12px;
            font-family: 'SF Mono', Monaco, 'Consolas', monospace;
            font-size: 0.8rem;
            max-height: 200px;
            overflow-y: auto;
            line-height: 1.6;
        }
        .log-line {
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }
        .log-line.info { color: var(--text); }
        .log-line.warn { color: var(--warning); }
        .log-line.error { color: var(--error); }
        .log-line .timestamp {
            color: var(--text-dim);
            margin-right: 8px;
        }
        footer {
            text-align: center;
            padding: 20px;
            color: var(--text-dim);
            font-size: 0.85rem;
        }
        footer a {
            color: var(--primary);
            text-decoration: none;
        }
        @media (max-width: 600px) {
            .header h1 { font-size: 1.4rem; }
            .metric { font-size: 2rem; }
            body { padding: 12px; }
        }
    </style>
</head>
)HTML"
R"HTML(<body>
    <header class="header">
        <h1>🧠 cog0 Dashboard</h1>
        <div class="status">
            <div class="status-dot" id="statusDot"></div>
            <span id="statusText">Connecting...</span>
        </div>
    </header>

    <div class="grid">
        <div class="card">
            <h2>Atoms</h2>
            <div class="metric" id="atomCount">0</div>
            <div class="metric-label">Total atoms in AtomStore</div>
        </div>

        <div class="card">
            <h2>Cycles</h2>
            <div class="metric" id="cycleCount">0</div>
            <div class="metric-label">Cognitive loop iterations</div>
        </div>

        <div class="card">
            <h2>Uptime</h2>
            <div class="metric" id="uptime">0:00:00</div>
            <div class="metric-label">Time since start</div>
        </div>

        <div class="card">
            <h2>Performance</h2>
            <div class="metric-row">
                <span class="label">Avg Cycle Time</span>
                <span class="value" id="avgCycleMs">0.00 ms</span>
            </div>
            <div class="metric-row">
                <span class="label">Rules Fired</span>
                <span class="value" id="rulesFired">0</span>
            </div>
            <div class="metric-row">
                <span class="label">Pending Tasks</span>
                <span class="value" id="pendingTasks">0</span>
            </div>
        </div>
    </div>

    <div class="grid">
        <div class="card">
            <h2>Active Goals</h2>
            <div class="goals-list" id="goalsList">
                <div class="goal-item">
                    <div class="goal-priority medium">-</div>
                    <div class="goal-info">
                        <div class="goal-name">No active goals</div>
                        <div class="goal-status">Waiting for data...</div>
                    </div>
                </div>
            </div>
        </div>

        <div class="card">
            <h2>Atom Count History</h2>
            <div class="atoms-chart" id="atomsChart">
                <!-- Bars added dynamically -->
            </div>
        </div>

        <div class="card">
            <h2>Recent Activity</h2>
            <div class="log-output" id="logOutput">
                <div class="log-line info">
                    <span class="timestamp">[--:--:--]</span>
                    Dashboard initialized
                </div>
            </div>
        </div>
    </div>

    <footer>
        <p>cog0 Agent-Zero Standalone &mdash; <a href="https://github.com/cogpy/cog-zero">GitHub</a></p>
    </footer>

    <script>
        // ================================================================
        // Dashboard State
        // ================================================================
        const state = {
            connected: false,
            ws: null,
            atomHistory: [],
            maxHistory: 30,
            reconnectDelay: 1000,
            maxReconnectDelay: 30000
        };

        // ================================================================
        // DOM Elements
        // ================================================================
        const el = {
            statusDot: document.getElementById('statusDot'),
            statusText: document.getElementById('statusText'),
            atomCount: document.getElementById('atomCount'),
            cycleCount: document.getElementById('cycleCount'),
            uptime: document.getElementById('uptime'),
            avgCycleMs: document.getElementById('avgCycleMs'),
            rulesFired: document.getElementById('rulesFired'),
            pendingTasks: document.getElementById('pendingTasks'),
            goalsList: document.getElementById('goalsList'),
            atomsChart: document.getElementById('atomsChart'),
            logOutput: document.getElementById('logOutput')
        };

        // ================================================================
        // WebSocket Connection
        // ================================================================
        function connect() {
            const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${protocol}//${location.host}/ws/metrics`;

            log('info', `Connecting to ${wsUrl}...`);

            try {
                state.ws = new WebSocket(wsUrl);

                state.ws.onopen = () => {
                    state.connected = true;
                    state.reconnectDelay = 1000;
                    updateStatus(true);
                    log('info', 'WebSocket connected');
                };

                state.ws.onmessage = (event) => {
                    try {
                        const data = JSON.parse(event.data);
                        handleMetrics(data);
                    } catch (e) {
                        log('warn', 'Invalid JSON received');
                    }
                };

                state.ws.onclose = () => {
                    state.connected = false;
                    updateStatus(false);
                    log('warn', 'WebSocket disconnected, reconnecting...');
                    scheduleReconnect();
                };

                state.ws.onerror = () => {
                    log('error', 'WebSocket error');
                };
            } catch (e) {
                log('error', 'Failed to create WebSocket');
                scheduleReconnect();
            }
        }

        function scheduleReconnect() {
            setTimeout(() => {
                connect();
            }, state.reconnectDelay);

            state.reconnectDelay = Math.min(
                state.reconnectDelay * 2,
                state.maxReconnectDelay
            );
        }

        // ================================================================
        // UI Updates
        // ================================================================
        function updateStatus(connected) {
            el.statusDot.classList.toggle('connected', connected);
            el.statusText.textContent = connected ? 'Connected' : 'Disconnected';
        }

        function handleMetrics(data) {
            // Update main metrics
            if (data.atom_count !== undefined) {
                el.atomCount.textContent = formatNumber(data.atom_count);
                updateAtomHistory(data.atom_count);
            }

            if (data.cycle_count !== undefined) {
                el.cycleCount.textContent = formatNumber(data.cycle_count);
            }

            if (data.uptime_s !== undefined) {
                el.uptime.textContent = formatUptime(data.uptime_s);
            }

            if (data.avg_cycle_ms !== undefined) {
                el.avgCycleMs.textContent = data.avg_cycle_ms.toFixed(2) + ' ms';
            }

            if (data.rules_fired_total !== undefined) {
                el.rulesFired.textContent = formatNumber(data.rules_fired_total);
            }

            if (data.pending_tasks !== undefined) {
                el.pendingTasks.textContent = formatNumber(data.pending_tasks);
            }

            // Update goals list
            if (data.goals && Array.isArray(data.goals)) {
                updateGoalsList(data.goals);
            }
        }
)HTML"
R"HTML(

        function updateAtomHistory(count) {
            state.atomHistory.push(count);
            if (state.atomHistory.length > state.maxHistory) {
                state.atomHistory.shift();
            }
            renderAtomChart();
        }

        function renderAtomChart() {
            const max = Math.max(...state.atomHistory, 1);
            el.atomsChart.innerHTML = state.atomHistory.map(count => {
                const height = Math.max(4, (count / max) * 100);
                return `<div class="atoms-bar" style="height: ${height}%"></div>`;
            }).join('');
        }

        function updateGoalsList(goals) {
            if (goals.length === 0) {
                el.goalsList.innerHTML = `
                    <div class="goal-item">
                        <div class="goal-priority medium">-</div>
                        <div class="goal-info">
                            <div class="goal-name">No active goals</div>
                            <div class="goal-status">Agent idle</div>
                        </div>
                    </div>
                `;
                return;
            }

            el.goalsList.innerHTML = goals.map(goal => {
                const priority = goal.priority > 0.7 ? 'high' :
                                goal.priority > 0.3 ? 'medium' : 'low';
                return `
                    <div class="goal-item">
                        <div class="goal-priority ${priority}">${Math.round(goal.priority * 100)}</div>
                        <div class="goal-info">
                            <div class="goal-name">${escapeHtml(goal.name || 'Unknown')}</div>
                            <div class="goal-status">${escapeHtml(goal.status || 'active')}</div>
                        </div>
                    </div>
                `;
            }).join('');
        }

        // ================================================================
        // Logging
        // ================================================================
        function log(level, message) {
            const timestamp = new Date().toLocaleTimeString('en-US', { hour12: false });
            const line = document.createElement('div');
            line.className = `log-line ${level}`;
            line.innerHTML = `<span class="timestamp">[${timestamp}]</span>${escapeHtml(message)}`;
            el.logOutput.appendChild(line);

            // Keep only last 50 lines
            while (el.logOutput.children.length > 50) {
                el.logOutput.removeChild(el.logOutput.firstChild);
            }

            el.logOutput.scrollTop = el.logOutput.scrollHeight;
        }

        // ================================================================
        // Utilities
        // ================================================================
        function formatNumber(n) {
            if (n >= 1e6) return (n / 1e6).toFixed(1) + 'M';
            if (n >= 1e3) return (n / 1e3).toFixed(1) + 'K';
            return n.toString();
        }

        function formatUptime(seconds) {
            const h = Math.floor(seconds / 3600);
            const m = Math.floor((seconds % 3600) / 60);
            const s = Math.floor(seconds % 60);
            return `${h}:${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}`;
        }

        function escapeHtml(str) {
            const div = document.createElement('div');
            div.textContent = str;
            return div.innerHTML;
        }

        // ================================================================
        // Fallback: Poll /metrics if WebSocket unavailable
        // ================================================================
        async function pollMetrics() {
            if (state.connected) return;

            try {
                const response = await fetch('/metrics');
                if (response.ok) {
                    const data = await response.json();
                    handleMetrics(data);
                }
            } catch (e) {
                // Silently fail, WebSocket will retry
            }
        }

        // ================================================================
        // Initialize
        // ================================================================
        document.addEventListener('DOMContentLoaded', () => {
            connect();

            // Fallback polling every 2 seconds when disconnected
            setInterval(pollMetrics, 2000);

            // Initialize chart with empty bars
            for (let i = 0; i < 20; i++) {
                state.atomHistory.push(0);
            }
            renderAtomChart();
        });
    </script>
</body>
</html>
)HTML";

const size_t DASHBOARD_HTML_LEN = std::strlen(DASHBOARD_HTML);

// Minimal favicon (1x1 transparent PNG, base64)
const char* DASHBOARD_FAVICON = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII=";

} // namespace cog0
