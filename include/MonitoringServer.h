/*
 * standalone/include/cog0/MonitoringServer.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Lightweight HTTP/1.1 monitoring server for the standalone cog0 agent.
 *
 * Exposes a small set of JSON endpoints that can feed Prometheus scrapers,
 * dashboards, or simple curl-based health checks.  No external dependencies
 * — the server is built entirely on POSIX sockets (Linux / macOS) and
 * std::thread.
 *
 * Endpoints
 * ─────────
 *   GET /health     — {"status":"ok","uptime_s":<n>}
 *   GET /metrics    — JSON object with AtomStore size, cycle count, timing
 *   GET /atoms      — JSON array of all atoms in the AtomStore
 *   GET /attention  — JSON array of (atom, STI, LTI) sorted by STI
 *   GET /dashboard  — Interactive HTML/JS dashboard (Phase 14)
 *   WS  /ws/metrics — WebSocket endpoint for real-time metrics push
 *
 * Usage
 * ─────
 *   MonitoringServer srv(store, loop, 8080);
 *   srv.enableWebSocket(true);  // enable real-time dashboard
 *   srv.start();          // starts background listener thread
 *   // … agent runs …
 *   srv.stop();           // graceful shutdown
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "AtomStore.h"
#include "CognitiveLoop.h"

namespace cog0 {

// =========================================================================
// AgentMetrics — snapshot of key performance indicators
// =========================================================================

struct AgentMetrics {
    size_t   atomCount      = 0;
    uint64_t cycleCount     = 0;
    uint64_t rulesFiredTotal= 0;
    double   avgCycleMs     = 0.0;
    double   uptimeSeconds  = 0.0;
    size_t   pendingTasks   = 0;
};

// =========================================================================
// MonitoringServer
// =========================================================================

class MonitoringServer {
public:
    /// Construct with shared AtomStore and CognitiveLoop.
    /// @param port  TCP port to listen on (default 8080)
    explicit MonitoringServer(
        std::shared_ptr<AtomStore>    store,
        std::shared_ptr<CognitiveLoop> loop = nullptr,
        uint16_t port = 8080);

    ~MonitoringServer();

    // ----------------------------------------------------------------
    // Lifecycle

    /// Start the background HTTP listener thread.
    void start();

    /// Gracefully stop the listener (blocks until thread exits).
    void stop();

    [[nodiscard]] bool running() const { return _running.load(); }
    [[nodiscard]] uint16_t port() const { return _port; }

    // ----------------------------------------------------------------
    // Metrics snapshot (may be called independently of HTTP)

    AgentMetrics snapshot() const;

    // ----------------------------------------------------------------
    // Optional extra metrics hook

    using MetricsHook = std::function<std::string()>;

    /// Register a callback that returns a JSON object fragment appended
    /// to the /metrics response (e.g. for application-specific counters).
    void setExtraMetricsHook(MetricsHook hook) { _extraHook = std::move(hook); }

    // ----------------------------------------------------------------
    // WebSocket support (Phase 14)

    /// Enable or disable WebSocket support for real-time metrics push.
    void enableWebSocket(bool enable = true) { _wsEnabled = enable; }

    /// Check if WebSocket is enabled.
    [[nodiscard]] bool webSocketEnabled() const { return _wsEnabled; }

    /// Get count of connected WebSocket clients.
    [[nodiscard]] size_t webSocketClientCount() const;

    /// Set WebSocket broadcast interval in milliseconds (default: 500ms).
    void setWebSocketBroadcastInterval(uint32_t ms) { _wsBroadcastIntervalMs = ms; }

private:
    void _listenLoop();
    void _handleClient(int fd);

    std::string _handleRequest(const std::string& method,
                               const std::string& path,
                               const std::string& headers,
                               int clientFd);

    std::string _routeHealth()    const;
    std::string _routeMetrics()   const;
    std::string _routeAtoms()     const;
    std::string _routeAttention() const;
    std::string _routeDashboard() const;

    // ---- WebSocket handling ----
    void _handleWebSocketUpgrade(int fd, const std::string& headers);
    void _handleWebSocketClient(int fd);
    void _webSocketBroadcastLoop();
    void _broadcastMetrics();
    void _removeWebSocketClient(int fd);

    // ---- HTTP helpers ----
    static std::string _httpOk(const std::string& body);
    static std::string _httpOkHtml(const std::string& body);
    static std::string _httpNotFound();
    static std::string _httpMethodNotAllowed();

    // ---- JSON helpers ----
    static std::string _jsonEscape(const std::string& s);
    static std::string _jsonDouble(double v, int prec = 2);

    // ---- State ----
    std::shared_ptr<AtomStore>     _store;
    std::shared_ptr<CognitiveLoop> _loop;   // may be null
    uint16_t                       _port;
    std::atomic<bool>              _running{false};
    int                            _serverFd = -1;
    std::thread                    _thread;
    std::chrono::steady_clock::time_point _startTime;
    MetricsHook                    _extraHook;

    // Accumulated stats
    mutable std::atomic<uint64_t>  _rulesFiredTotal{0};
    mutable std::atomic<uint64_t>  _cycleTimeAccumMs{0};

    // ---- WebSocket state ----
    bool                           _wsEnabled = false;
    uint32_t                       _wsBroadcastIntervalMs = 500;
    std::vector<int>               _wsClients;
    mutable std::mutex             _wsClientsMutex;
    std::thread                    _wsBroadcastThread;
};

} // namespace cog0
