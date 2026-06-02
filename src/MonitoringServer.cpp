/*
 * standalone/src/MonitoringServer.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Lightweight HTTP/1.1 monitoring server — POSIX sockets, zero external deps.
 * Phase 14: Added WebSocket support and embedded dashboard.
 */

#include "cog0/MonitoringServer.h"
#include "cog0/Logger.h"
#include "cog0/WebSocketHandler.h"
#include "cog0/DashboardAssets.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sstream>

// POSIX socket headers
#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "Ws2_32.lib")
#  define CLOSE_SOCKET closesocket
#  define SOCK_NONBLOCK 0
using SocketIoResult = int;
#else
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <sys/select.h>
#  include <unistd.h>
#  define CLOSE_SOCKET close
#  define INVALID_SOCKET (-1)
#  define SOCKET_ERROR   (-1)
using SOCKET = int;
using SocketIoResult = ssize_t;
#endif

namespace cog0 {

// =========================================================================
// Constructor / Destructor
// =========================================================================

MonitoringServer::MonitoringServer(
    std::shared_ptr<AtomStore>     store,
    std::shared_ptr<CognitiveLoop> loop,
    uint16_t                       port)
    : _store(std::move(store))
    , _loop(std::move(loop))
    , _port(port)
    , _startTime(std::chrono::steady_clock::now())
{}

MonitoringServer::~MonitoringServer()
{
    stop();
}

// =========================================================================
// Lifecycle
// =========================================================================

void MonitoringServer::start()
{
    if (_running.exchange(true)) return;  // already running

    _startTime = std::chrono::steady_clock::now();
    _thread = std::thread([this] { _listenLoop(); });

    logger().info("MonitoringServer started on port " + std::to_string(_port));
}

void MonitoringServer::stop()
{
    if (!_running.exchange(false)) return;

    // Wake accept() by closing the server socket
    if (_serverFd != INVALID_SOCKET) {
        CLOSE_SOCKET(_serverFd);
        _serverFd = INVALID_SOCKET;
    }

    // Close all WebSocket clients
    {
        std::lock_guard<std::mutex> lock(_wsClientsMutex);
        for (int fd : _wsClients) {
            CLOSE_SOCKET(fd);
        }
        _wsClients.clear();
    }

    if (_wsBroadcastThread.joinable()) _wsBroadcastThread.join();
    if (_thread.joinable()) _thread.join();

    logger().info("MonitoringServer stopped");
}

// =========================================================================
// Metrics snapshot
// =========================================================================

AgentMetrics MonitoringServer::snapshot() const
{
    using namespace std::chrono;

    AgentMetrics m;
    m.atomCount    = _store ? _store->size() : 0;
    m.uptimeSeconds = duration_cast<microseconds>(
        steady_clock::now() - _startTime).count() / 1e6;

    if (_loop) {
        m.cycleCount = _loop->cycleCount();
        const auto& s = _loop->lastStats();
        m.rulesFiredTotal += s.rulesFired;
        m.avgCycleMs = static_cast<double>(s.duration.count());
    }

    return m;
}

size_t MonitoringServer::webSocketClientCount() const
{
    std::lock_guard<std::mutex> lock(_wsClientsMutex);
    return _wsClients.size();
}

// =========================================================================
// Listener loop
// =========================================================================

void MonitoringServer::_listenLoop()
{
#ifdef _WIN32
    // Initialise Winsock — required before any socket call on Windows.
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        logger().error("MonitoringServer: WSAStartup failed");
        _running.store(false);
        return;
    }
#endif

    // Create TCP socket
    _serverFd = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
    if (_serverFd == INVALID_SOCKET) {
        logger().error("MonitoringServer: socket() failed: " +
                       std::string(std::strerror(errno)));
        _running.store(false);
        return;
    }

    // Allow port reuse
    int opt = 1;
    ::setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&opt), sizeof(opt));

    // Bind
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(_port);

    if (::bind(_serverFd,
               reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        logger().error("MonitoringServer: bind() failed on port " +
                       std::to_string(_port) + ": " +
                       std::string(std::strerror(errno)));
        CLOSE_SOCKET(_serverFd);
        _serverFd = INVALID_SOCKET;
        _running.store(false);
        return;
    }

    if (::listen(_serverFd, 8) != 0) {
        logger().error("MonitoringServer: listen() failed");
        CLOSE_SOCKET(_serverFd);
        _serverFd = INVALID_SOCKET;
        _running.store(false);
        return;
    }

    logger().debug("MonitoringServer: listening on port " +
                   std::to_string(_port));

    // Start WebSocket broadcast thread if enabled
    if (_wsEnabled) {
        _wsBroadcastThread = std::thread([this] { _webSocketBroadcastLoop(); });
    }

    // Accept loop with select() for graceful shutdown
    while (_running.load()) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(_serverFd, &readSet);

        timeval tv{};
        tv.tv_sec  = 0;
        tv.tv_usec = 200'000;  // 200 ms timeout

        int ready = ::select(_serverFd + 1, &readSet, nullptr, nullptr, &tv);
        if (ready <= 0) continue;  // timeout or error

        sockaddr_in clientAddr{};
        socklen_t   clientLen = sizeof(clientAddr);
        int clientFd = static_cast<int>(
            ::accept(_serverFd,
                     reinterpret_cast<sockaddr*>(&clientAddr), &clientLen));

        if (clientFd < 0) break;  // server socket was closed

        // Handle synchronously (one connection at a time is fine for
        // low-frequency scraping; connections are short-lived)
        _handleClient(clientFd);
        CLOSE_SOCKET(clientFd);
    }

    if (_serverFd != INVALID_SOCKET) {
        CLOSE_SOCKET(_serverFd);
        _serverFd = INVALID_SOCKET;
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

// =========================================================================
// HTTP handling
// =========================================================================

void MonitoringServer::_handleClient(int fd)
{
    // Read request (simplified: read up to 4KB for headers)
    char buf[4096] = {};
    SocketIoResult bytesRead = ::recv(fd, buf, sizeof(buf) - 1, 0);
    if (bytesRead <= 0) return;

    std::string req(buf, static_cast<size_t>(bytesRead));
    std::string method, path;

    // Parse "METHOD /path HTTP/1.x\r\n…"
    std::istringstream iss(req);
    iss >> method >> path;

    // Check for WebSocket upgrade
    if (_wsEnabled && WebSocketHandler::isUpgradeRequest(req)) {
        _handleWebSocketUpgrade(fd, req);
        return;  // Don't close fd — it's now a WebSocket connection
    }

    std::string response = _handleRequest(method, path, req, fd);

    ::send(fd, response.c_str(), static_cast<int>(response.size()), 0);
}

std::string MonitoringServer::_handleRequest(const std::string& method,
                                              const std::string& path,
                                              const std::string& /* headers */,
                                              int /* clientFd */)
{
    if (method != "GET")
        return _httpMethodNotAllowed();

    if (path == "/health")
        return _httpOk(_routeHealth());
    if (path == "/metrics")
        return _httpOk(_routeMetrics());
    if (path == "/atoms")
        return _httpOk(_routeAtoms());
    if (path == "/attention")
        return _httpOk(_routeAttention());
    if (path == "/dashboard" || path == "/")
        return _httpOkHtml(_routeDashboard());

    return _httpNotFound();
}

// =========================================================================
// Route implementations
// =========================================================================

std::string MonitoringServer::_routeHealth() const
{
    auto m = snapshot();
    std::ostringstream out;
    out << "{"
        << "\"status\":\"ok\","
        << "\"uptime_s\":" << _jsonDouble(m.uptimeSeconds)
        << "}";
    return out.str();
}

std::string MonitoringServer::_routeMetrics() const
{
    auto m = snapshot();
    std::ostringstream out;
    out << "{"
        << "\"atom_count\":" << m.atomCount << ","
        << "\"cycle_count\":" << m.cycleCount << ","
        << "\"rules_fired_total\":" << m.rulesFiredTotal << ","
        << "\"avg_cycle_ms\":" << _jsonDouble(m.avgCycleMs) << ","
        << "\"uptime_s\":" << _jsonDouble(m.uptimeSeconds);

    if (_extraHook) {
        std::string extra = _extraHook();
        if (!extra.empty()) out << "," << extra;
    }

    out << "}";
    return out.str();
}

std::string MonitoringServer::_routeAtoms() const
{
    if (!_store) return "[]";

    // Collect atoms of all types
    std::vector<Atom::Handle> atoms;
    for (int t = static_cast<int>(AtomType::CONCEPT);
         t <= static_cast<int>(AtomType::CUSTOM); ++t) {
        auto batch = _store->getByType(static_cast<AtomType>(t));
        atoms.insert(atoms.end(), batch.begin(), batch.end());
    }

    std::ostringstream out;
    out << '[';
    bool first = true;
    for (const auto& a : atoms) {
        if (!first) out << ',';
        first = false;
        out << "{"
            << "\"type\":" << _jsonEscape(atomTypeName(a->type())) << ","
            << "\"name\":" << _jsonEscape(a->name()) << ","
            << "\"sti\":"  << _jsonDouble(a->sti())  << ","
            << "\"lti\":"  << _jsonDouble(a->lti())
            << "}";
    }
    out << ']';
    return out.str();
}

std::string MonitoringServer::_routeAttention() const
{
    if (!_store) return "[]";

    std::vector<Atom::Handle> atoms;
    for (int t = static_cast<int>(AtomType::CONCEPT);
         t <= static_cast<int>(AtomType::CUSTOM); ++t) {
        auto batch = _store->getByType(static_cast<AtomType>(t));
        atoms.insert(atoms.end(), batch.begin(), batch.end());
    }

    // Sort by STI descending
    std::sort(atoms.begin(), atoms.end(),
              [](const Atom::Handle& a, const Atom::Handle& b) {
                  return a->sti() > b->sti();
              });

    std::ostringstream out;
    out << '[';
    bool first = true;
    for (const auto& a : atoms) {
        if (!first) out << ',';
        first = false;
        out << "[" << _jsonEscape(a->name()) << ","
            << _jsonDouble(a->sti()) << ","
            << _jsonDouble(a->lti()) << "]";
    }
    out << ']';
    return out.str();
}

std::string MonitoringServer::_routeDashboard() const
{
    return std::string(DASHBOARD_HTML);
}

// =========================================================================
// WebSocket handling
// =========================================================================

void MonitoringServer::_handleWebSocketUpgrade(int fd, const std::string& headers)
{
    std::string secKey = WebSocketHandler::extractSecKey(headers);
    if (secKey.empty()) {
        // Invalid upgrade request
        std::string response = _httpNotFound();
        ::send(fd, response.c_str(), static_cast<int>(response.size()), 0);
        return;
    }

    // Send upgrade response
    std::string response = WebSocketHandler::upgradeResponse(secKey);
    ::send(fd, response.c_str(), static_cast<int>(response.size()), 0);

    // Add to WebSocket clients list
    {
        std::lock_guard<std::mutex> lock(_wsClientsMutex);
        _wsClients.push_back(fd);
    }

    logger().debug("WebSocket client connected, fd=" + std::to_string(fd));

    // Handle WebSocket client in a separate thread
    std::thread([this, fd] { _handleWebSocketClient(fd); }).detach();
}

void MonitoringServer::_handleWebSocketClient(int fd)
{
    // Set socket to non-blocking for polling
    std::vector<uint8_t> buffer;
    buffer.reserve(1024);

    while (_running.load()) {
        // Read available data
        char buf[1024];
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(fd, &readSet);

        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 100'000;  // 100ms timeout

        int ready = ::select(fd + 1, &readSet, nullptr, nullptr, &tv);
        if (ready < 0) break;  // Error

        if (ready > 0) {
            SocketIoResult n = ::recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) break;  // Connection closed

            const auto* bytes = reinterpret_cast<const uint8_t*>(buf);
            buffer.insert(buffer.end(), bytes, bytes + static_cast<size_t>(n));

            // Try to decode frames
            while (!buffer.empty()) {
                std::string payload;
                WebSocketHandler::Opcode op;
                size_t consumed = WebSocketHandler::decodeFrameFromBuffer(
                    buffer.data(), buffer.size(), payload, op);

                if (consumed == 0) break;  // Incomplete frame

                buffer.erase(buffer.begin(),
                             buffer.begin() + static_cast<std::ptrdiff_t>(consumed));

                // Handle control frames
                if (op == WebSocketHandler::CLOSE) {
                    // Send close frame back
                    auto closeFrame = WebSocketHandler::closeFrame(1000, "");
                    ::send(fd, reinterpret_cast<const char*>(closeFrame.data()),
                           static_cast<int>(closeFrame.size()), 0);
                    _removeWebSocketClient(fd);
                    CLOSE_SOCKET(fd);
                    return;
                } else if (op == WebSocketHandler::PING) {
                    auto pongFrame = WebSocketHandler::pongFrame(payload);
                    ::send(fd, reinterpret_cast<const char*>(pongFrame.data()),
                           static_cast<int>(pongFrame.size()), 0);
                }
                // TEXT frames from client could be commands — ignore for now
            }
        }
    }

    _removeWebSocketClient(fd);
    CLOSE_SOCKET(fd);
}

void MonitoringServer::_webSocketBroadcastLoop()
{
    while (_running.load()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(_wsBroadcastIntervalMs));

        if (!_running.load()) break;

        _broadcastMetrics();
    }
}

void MonitoringServer::_broadcastMetrics()
{
    std::string metricsJson = _routeMetrics();
    auto frame = WebSocketHandler::encodeFrame(metricsJson, WebSocketHandler::TEXT);

    std::lock_guard<std::mutex> lock(_wsClientsMutex);
    std::vector<int> deadClients;

    for (int fd : _wsClients) {
        SocketIoResult sent = ::send(fd, reinterpret_cast<const char*>(frame.data()),
                                     static_cast<int>(frame.size()), 0);
        if (sent <= 0) {
            deadClients.push_back(fd);
        }
    }

    // Remove dead clients
    for (int fd : deadClients) {
        _wsClients.erase(
            std::remove(_wsClients.begin(), _wsClients.end(), fd),
            _wsClients.end());
        CLOSE_SOCKET(fd);
    }
}

void MonitoringServer::_removeWebSocketClient(int fd)
{
    std::lock_guard<std::mutex> lock(_wsClientsMutex);
    _wsClients.erase(
        std::remove(_wsClients.begin(), _wsClients.end(), fd),
        _wsClients.end());
}

// =========================================================================
// HTTP helpers
// =========================================================================

/*static*/ std::string MonitoringServer::_httpOk(const std::string& body)
{
    std::ostringstream h;
    h << "HTTP/1.1 200 OK\r\n"
      << "Content-Type: application/json\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n"
      << "\r\n"
      << body;
    return h.str();
}

/*static*/ std::string MonitoringServer::_httpOkHtml(const std::string& body)
{
    std::ostringstream h;
    h << "HTTP/1.1 200 OK\r\n"
      << "Content-Type: text/html; charset=utf-8\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n"
      << "\r\n"
      << body;
    return h.str();
}

/*static*/ std::string MonitoringServer::_httpNotFound()
{
    std::string body = "{\"error\":\"not found\"}";
    std::ostringstream h;
    h << "HTTP/1.1 404 Not Found\r\n"
      << "Content-Type: application/json\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n"
      << "\r\n"
      << body;
    return h.str();
}

/*static*/ std::string MonitoringServer::_httpMethodNotAllowed()
{
    std::string body = "{\"error\":\"method not allowed\"}";
    std::ostringstream h;
    h << "HTTP/1.1 405 Method Not Allowed\r\n"
      << "Content-Type: application/json\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n"
      << "\r\n"
      << body;
    return h.str();
}

// =========================================================================
// JSON helpers
// =========================================================================

/*static*/ std::string MonitoringServer::_jsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    out += '"';
    return out;
}

/*static*/ std::string MonitoringServer::_jsonDouble(double v, int prec)
{
    std::ostringstream oss;
    oss.precision(prec);
    oss << std::fixed << v;
    return oss.str();
}

} // namespace cog0
