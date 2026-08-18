/*
 * src/GrpcAgentServer.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * AgentService server — length-prefixed JSON transport (gRPC fallback).
 *
 * Frame format (network byte order):
 *   uint32_t length | UTF-8 JSON payload
 */

#include "cog0/GrpcAgentServer.h"
#include "cog0/Logger.h"
#include "cog0/AtomStore.h"
#include "cog0/AgentServiceJson.h"

#include <cerrno>
#include <cstring>
#include <sstream>

// POSIX / Winsock socket headers
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "Ws2_32.lib")
#  define CLOSE_SOCKET closesocket
#  ifndef MSG_NOSIGNAL
#    define MSG_NOSIGNAL 0
#  endif
#  ifndef SHUT_RDWR
#    define SHUT_RDWR SD_BOTH
#  endif
using SocketIoResult = int;
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <unistd.h>
#  define CLOSE_SOCKET close
#  ifndef INVALID_SOCKET
#    define INVALID_SOCKET (-1)
#  endif
using SocketIoResult = ssize_t;
#endif

namespace cog0 {
namespace {

using agent_json::escape;
using agent_json::extractNumberField;
using agent_json::extractObjectField;
using agent_json::extractStringField;
using agent_json::extractUintField;

std::string okResult(const std::string& resultJson)
{
    return std::string("{\"ok\":true,\"result\":") + resultJson + "}";
}

std::string errResult(const std::string& msg)
{
    return std::string("{\"ok\":false,\"error\":") + escape(msg) + "}";
}

#ifdef _WIN32
// Ensure Winsock is initialized once for this translation unit's socket use.
bool ensureWinsock()
{
    static const bool ok = []() {
        WSADATA wsa{};
        return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }();
    return ok;
}
#endif

} // namespace

// =========================================================================
// Lifecycle
// =========================================================================

GrpcAgentServer::GrpcAgentServer(Agent& agent, uint16_t port)
    : _agent(agent)
    , _port(port)
{}

GrpcAgentServer::~GrpcAgentServer()
{
    stop();
}

bool GrpcAgentServer::usingRealGrpc()
{
#ifdef COG0_HAVE_GRPC
    return true;
#else
    return false;
#endif
}

bool GrpcAgentServer::start()
{
    if (_running.exchange(true))
        return true;

    _serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (_serverFd < 0) {
        _running = false;
        logger().error("GrpcAgentServer: socket() failed");
        return false;
    }

    int yes = 1;
    ::setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(_port);

    if (::bind(_serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        logger().error("GrpcAgentServer: bind() failed on port " + std::to_string(_port));
        ::close(_serverFd);
        _serverFd = -1;
        _running = false;
        return false;
    }

    // Discover ephemeral port if 0 was requested.
    if (_port == 0) {
        socklen_t len = sizeof(addr);
        if (::getsockname(_serverFd, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
            _port = ntohs(addr.sin_port);
        }
    }

    if (::listen(_serverFd, 16) < 0) {
        logger().error("GrpcAgentServer: listen() failed");
        ::close(_serverFd);
        _serverFd = -1;
        _running = false;
        return false;
    }

    _thread = std::thread([this] { _listenLoop(); });
    logger().info("GrpcAgentServer listening on port " + std::to_string(_port) +
                  (usingRealGrpc() ? " (gRPC)" : " (JSON fallback)"));
    return true;
}

void GrpcAgentServer::stop()
{
    if (!_running.exchange(false))
        return;

    if (_serverFd >= 0) {
        ::shutdown(_serverFd, SHUT_RDWR);
        ::close(_serverFd);
        _serverFd = -1;
    }
    if (_thread.joinable())
        _thread.join();

    logger().info("GrpcAgentServer stopped");
}

// =========================================================================
// Direct RPC handlers
// =========================================================================

std::string GrpcAgentServer::handleSetGoal(const std::string& name,
                                           const std::string& description,
                                           double priority)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (name.empty())
        return errResult("goal name is required");

    double prio = (priority > 0.0) ? priority : 1.0;
    auto goal = _agent.setGoal(name, description, prio);
    if (!goal)
        return errResult("failed to set goal");

    std::ostringstream out;
    out << "{\"ok\":true,\"goal_id\":" << escape(name)
        << ",\"message\":" << escape("goal set") << "}";
    return okResult(out.str());
}

std::string GrpcAgentServer::handleInjectPercept(const std::string& source,
                                                 const std::string& content,
                                                 double salience)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (content.empty())
        return errResult("percept content is required");

    double sal = (salience > 0.0) ? salience : 0.5;
    std::string src = source.empty() ? "grpc" : source;
    _agent.addPercept(src, content, sal);

    std::ostringstream out;
    out << "{\"ok\":true,\"message\":" << escape("percept injected") << "}";
    return okResult(out.str());
}

std::vector<GrpcCycleStatus> GrpcAgentServer::handleRunCycles(uint32_t cycles)
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<GrpcCycleStatus> statuses;
    if (cycles == 0)
        cycles = 1;

    for (uint32_t i = 1; i <= cycles; ++i) {
        _agent.runCycles(1);
        GrpcCycleStatus st;
        st.cycle = i;
        st.total = cycles;
        st.phase = "cycle";
        st.done = (i == cycles);
        st.detail = "completed cycle " + std::to_string(i);
        statuses.push_back(std::move(st));
    }
    return statuses;
}

GrpcAgentStatus GrpcAgentServer::handleGetStatus() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    GrpcAgentStatus st;
    st.name = _agent.name();
    st.running = _agent.isRunning();
    st.cycleCount = _agent.cognitiveLoop().cycleCount();
    st.atomCount = _agent.atomStore().size();
    st.pendingTasks = _agent.taskManager().pendingCount();
    st.report = _agent.statusReport();
    return st;
}

std::string GrpcAgentServer::handleQueryAtoms(const std::string& namePrefix,
                                              const std::string& typeFilter,
                                              uint32_t limit)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto& store = _agent.atomStore();

    std::vector<Atom::Handle> atoms;
    for (int t = static_cast<int>(AtomType::CONCEPT);
         t <= static_cast<int>(AtomType::CUSTOM); ++t) {
        auto batch = store.getByType(static_cast<AtomType>(t));
        atoms.insert(atoms.end(), batch.begin(), batch.end());
    }

    std::ostringstream out;
    out << "{\"atoms\":[";
    bool first = true;
    uint32_t count = 0;
    for (const auto& a : atoms) {
        if (!typeFilter.empty() && atomTypeName(a->type()) != typeFilter)
            continue;
        if (!namePrefix.empty() &&
            a->name().compare(0, namePrefix.size(), namePrefix) != 0)
            continue;

        if (!first) out << ',';
        first = false;
        out << "{"
            << "\"type\":" << escape(atomTypeName(a->type())) << ","
            << "\"name\":" << escape(a->name()) << ","
            << "\"sti\":" << a->sti() << ","
            << "\"lti\":" << a->lti() << ","
            << "\"strength\":" << a->tv().strength << ","
            << "\"confidence\":" << a->tv().confidence
            << "}";
        ++count;
        if (limit > 0 && count >= limit) break;
    }
    out << "]}";
    return okResult(out.str());
}

std::string GrpcAgentServer::dispatchJson(const std::string& requestJson)
{
    std::string method = extractStringField(requestJson, "method");
    std::string params = extractObjectField(requestJson, "params");

    if (method.empty())
        return errResult("missing method");

    if (method == "SetGoal") {
        return handleSetGoal(extractStringField(params, "name"),
                             extractStringField(params, "description"),
                             extractNumberField(params, "priority", 1.0));
    }
    if (method == "InjectPercept") {
        return handleInjectPercept(extractStringField(params, "source"),
                                   extractStringField(params, "content"),
                                   extractNumberField(params, "salience", 0.5));
    }
    if (method == "RunCycles") {
        uint32_t cycles = extractUintField(params, "cycles", 1);
        auto statuses = handleRunCycles(cycles);
        std::ostringstream out;
        out << "{\"statuses\":[";
        for (size_t i = 0; i < statuses.size(); ++i) {
            if (i) out << ',';
            const auto& s = statuses[i];
            out << "{"
                << "\"cycle\":" << s.cycle << ","
                << "\"total\":" << s.total << ","
                << "\"phase\":" << escape(s.phase) << ","
                << "\"done\":" << (s.done ? "true" : "false") << ","
                << "\"detail\":" << escape(s.detail)
                << "}";
        }
        out << "]}";
        return okResult(out.str());
    }
    if (method == "GetStatus") {
        auto st = handleGetStatus();
        std::ostringstream out;
        out << "{"
            << "\"name\":" << escape(st.name) << ","
            << "\"running\":" << (st.running ? "true" : "false") << ","
            << "\"cycle_count\":" << st.cycleCount << ","
            << "\"atom_count\":" << st.atomCount << ","
            << "\"pending_tasks\":" << st.pendingTasks << ","
            << "\"report\":" << escape(st.report)
            << "}";
        return okResult(out.str());
    }
    if (method == "QueryAtoms") {
        return handleQueryAtoms(extractStringField(params, "name_prefix"),
                                extractStringField(params, "type"),
                                extractUintField(params, "limit", 0));
    }

    return errResult("unknown method: " + method);
}

// =========================================================================
// Network
// =========================================================================

bool GrpcAgentServer::_sendFrame(int fd, const std::string& payload)
{
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    if (::send(fd, &len, sizeof(len), MSG_NOSIGNAL) != static_cast<ssize_t>(sizeof(len)))
        return false;
    size_t sent = 0;
    while (sent < payload.size()) {
        ssize_t n = ::send(fd, payload.data() + sent, payload.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool GrpcAgentServer::_recvFrame(int fd, std::string& out)
{
    uint32_t len_be = 0;
    size_t got = 0;
    while (got < sizeof(len_be)) {
        ssize_t n = ::recv(fd, reinterpret_cast<char*>(&len_be) + got,
                           sizeof(len_be) - got, 0);
        if (n <= 0) return false;
        got += static_cast<size_t>(n);
    }
    uint32_t len = ntohl(len_be);
    if (len > 16u * 1024u * 1024u)  // 16 MiB cap
        return false;

    out.assign(len, '\0');
    got = 0;
    while (got < len) {
        ssize_t n = ::recv(fd, &out[got], len - got, 0);
        if (n <= 0) return false;
        got += static_cast<size_t>(n);
    }
    return true;
}

void GrpcAgentServer::_listenLoop()
{
    while (_running.load()) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(_serverFd, &readSet);
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        int ready = ::select(_serverFd + 1, &readSet, nullptr, nullptr, &tv);
        if (ready <= 0) continue;

        sockaddr_in client{};
        socklen_t clen = sizeof(client);
        int cfd = ::accept(_serverFd, reinterpret_cast<sockaddr*>(&client), &clen);
        if (cfd < 0) {
            if (!_running.load()) break;
            continue;
        }
        _handleClient(cfd);
        ::close(cfd);
    }
}

void GrpcAgentServer::_handleClient(int fd)
{
    // Process one or more framed requests until client disconnects.
    while (_running.load()) {
        std::string req;
        if (!_recvFrame(fd, req))
            break;
        std::string resp = dispatchJson(req);
        if (!_sendFrame(fd, resp))
            break;
    }
}

} // namespace cog0
