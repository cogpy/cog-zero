/*
 * standalone/include/cog0/MonitoringServer.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Lightweight HTTP/1.1 monitoring server for the standalone cog0 agent.
 *
 * Exposes a small set of JSON endpoints that can feed Prometheus scrapers,
 * dashboards, or simple curl-based health checks.  Built on POSIX sockets
 * (Linux / macOS) and std::thread.  Optional OpenSSL TLS when built with
 * COG0_HAVE_OPENSSL.
 *
 * Endpoints
 * ─────────
 *   GET /health     — {"status":"ok","uptime_s":<n>,"tls_enabled":...}
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
 *   srv.enableTLS("cert.pem", "key.pem");  // optional TLS
 *   srv.start();          // starts background listener thread
 *   // … agent runs …
 *   srv.stop();           // graceful shutdown
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "AtomStore.h"
#include "CognitiveLoop.h"
#include "TlsSocket.h"

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

    // ----------------------------------------------------------------
    // TLS support (Phase 14 Feature 2.5)

    /// Enable TLS using PEM certificate and private key paths.
    /// Returns false if OpenSSL is unavailable or files cannot be loaded.
    /// Must be called before start() (or while stopped).
    bool enableTLS(const std::string& certPath, const std::string& keyPath);

    /// Disable TLS (plain HTTP).
    void disableTLS();

    [[nodiscard]] bool tlsEnabled() const { return _tlsEnabled; }
    [[nodiscard]] bool tlsReady() const { return _tlsEnabled && _tlsContext.ready(); }
    [[nodiscard]] const std::string& tlsLastError() const { return _tlsContext.lastError(); }

private:
    void _listenLoop();
    /// @return true if fd was retained (e.g. WebSocket); caller must not close.
    bool _handleClient(int fd);
    /// @return true if fd was retained (e.g. WebSocket); caller must not close.
    bool _handleClientTls(int fd);

    /// Unified send/recv helpers (plain or TLS depending on session).
    int _ioSend(int fd, TlsSocket* tls, const void* data, size_t len);
    int _ioRecv(int fd, TlsSocket* tls, void* data, size_t len);

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
    void _handleWebSocketUpgrade(int fd, const std::string& headers, TlsSocket* tls = nullptr);
    void _handleWebSocketClient(int fd, std::shared_ptr<TlsSocket> tls = nullptr);
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
    // Optional TLS sessions for WebSocket clients (keyed by fd).
    std::map<int, std::shared_ptr<TlsSocket>> _wsTlsSessions;
    mutable std::mutex             _wsClientsMutex;
    std::thread                    _wsBroadcastThread;

    // ---- TLS state ----
    bool                           _tlsEnabled = false;
    TlsContext                     _tlsContext;
};

} // namespace cog0
