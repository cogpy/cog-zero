/*
 * src/GrpcAgentClient.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * AgentService client — length-prefixed JSON transport (gRPC fallback).
 */

#include "cog0/GrpcAgentClient.h"
#include "cog0/AgentServiceJson.h"

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
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#  define CLOSE_SOCKET ::close
using SocketIoResult = ssize_t;
#endif

namespace cog0 {
namespace {

using agent_json::escape;
using agent_json::extractBoolField;
using agent_json::extractStringField;
using agent_json::extractUint64Field;
using agent_json::responseOk;

#ifdef _WIN32
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

GrpcAgentClient::GrpcAgentClient(const std::string& host, uint16_t port)
{
#ifdef _WIN32
    if (!ensureWinsock()) {
        _lastError = "WSAStartup failed";
        return;
    }
#endif

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    std::string portStr = std::to_string(port);
    int rc = ::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res);
    if (rc != 0 || !res) {
        _lastError = "getaddrinfo failed for " + host;
        return;
    }

    int fd = static_cast<int>(::socket(res->ai_family, res->ai_socktype, res->ai_protocol));
    if (fd < 0) {
        _lastError = "socket() failed";
        ::freeaddrinfo(res);
        return;
    }

    if (::connect(fd, res->ai_addr, static_cast<int>(res->ai_addrlen)) < 0) {
        _lastError = "connect() failed to " + host + ":" + portStr;
        CLOSE_SOCKET(fd);
        ::freeaddrinfo(res);
        return;
    }

    ::freeaddrinfo(res);
    _fd = fd;
}

GrpcAgentClient::~GrpcAgentClient()
{
    close();
}

void GrpcAgentClient::close()
{
    if (_fd >= 0) {
        ::shutdown(_fd, SHUT_RDWR);
        CLOSE_SOCKET(_fd);
        _fd = -1;
    }
}

bool GrpcAgentClient::_sendFrame(int fd, const std::string& payload)
{
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    if (::send(fd, reinterpret_cast<const char*>(&len),
               static_cast<int>(sizeof(len)), MSG_NOSIGNAL)
        != static_cast<SocketIoResult>(sizeof(len)))
        return false;
    size_t sent = 0;
    while (sent < payload.size()) {
        SocketIoResult n = ::send(fd, payload.data() + sent,
                                  static_cast<int>(payload.size() - sent),
                                  MSG_NOSIGNAL);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool GrpcAgentClient::_recvFrame(int fd, std::string& out)
{
    uint32_t len_be = 0;
    size_t got = 0;
    while (got < sizeof(len_be)) {
        SocketIoResult n = ::recv(fd, reinterpret_cast<char*>(&len_be) + got,
                                  static_cast<int>(sizeof(len_be) - got), 0);
        if (n <= 0) return false;
        got += static_cast<size_t>(n);
    }
    uint32_t len = ntohl(len_be);
    if (len > 16u * 1024u * 1024u)
        return false;

    out.assign(len, '\0');
    got = 0;
    while (got < len) {
        SocketIoResult n = ::recv(fd, &out[got], static_cast<int>(len - got), 0);
        if (n <= 0) return false;
        got += static_cast<size_t>(n);
    }
    return true;
}

bool GrpcAgentClient::_call(const std::string& method,
                            const std::string& paramsJson,
                            std::string& outResponseJson)
{
    if (_fd < 0) {
        _lastError = "not connected";
        return false;
    }

    std::ostringstream req;
    req << "{\"method\":" << escape(method)
        << ",\"params\":" << paramsJson << "}";

    if (!_sendFrame(_fd, req.str())) {
        _lastError = "send failed";
        return false;
    }
    if (!_recvFrame(_fd, outResponseJson)) {
        _lastError = "recv failed";
        return false;
    }
    if (!responseOk(outResponseJson)) {
        _lastError = extractStringField(outResponseJson, "error");
        if (_lastError.empty()) _lastError = "RPC failed";
        return false;
    }
    return true;
}

bool GrpcAgentClient::setGoal(const std::string& name,
                              const std::string& description,
                              double priority,
                              std::string& outGoalId,
                              std::string& outMessage)
{
    std::ostringstream params;
    params << "{\"name\":" << escape(name)
           << ",\"description\":" << escape(description)
           << ",\"priority\":" << priority << "}";
    std::string resp;
    if (!_call("SetGoal", params.str(), resp))
        return false;
    outGoalId = extractStringField(resp, "goal_id");
    outMessage = extractStringField(resp, "message");
    return true;
}

bool GrpcAgentClient::injectPercept(const std::string& source,
                                    const std::string& content,
                                    double salience,
                                    std::string& outMessage)
{
    std::ostringstream params;
    params << "{\"source\":" << escape(source)
           << ",\"content\":" << escape(content)
           << ",\"salience\":" << salience << "}";
    std::string resp;
    if (!_call("InjectPercept", params.str(), resp))
        return false;
    outMessage = extractStringField(resp, "message");
    return true;
}

bool GrpcAgentClient::runCycles(uint32_t cycles, std::vector<GrpcCycleStatus>& outStatuses)
{
    std::ostringstream params;
    params << "{\"cycles\":" << cycles << "}";
    std::string resp;
    if (!_call("RunCycles", params.str(), resp))
        return false;

    outStatuses.clear();
    // Parse statuses array loosely by scanning cycle objects.
    auto arrPos = resp.find("\"statuses\"");
    if (arrPos == std::string::npos) return true;
    auto bracket = resp.find('[', arrPos);
    if (bracket == std::string::npos) return true;
    size_t i = bracket + 1;
    while (i < resp.size()) {
        auto objStart = resp.find('{', i);
        if (objStart == std::string::npos) break;
        auto objEnd = resp.find('}', objStart);
        if (objEnd == std::string::npos) break;
        std::string obj = resp.substr(objStart, objEnd - objStart + 1);
        GrpcCycleStatus st;
        st.cycle = extractUint64Field(obj, "cycle");
        st.total = extractUint64Field(obj, "total");
        st.phase = extractStringField(obj, "phase");
        st.done = extractBoolField(obj, "done");
        st.detail = extractStringField(obj, "detail");
        outStatuses.push_back(std::move(st));
        i = objEnd + 1;
        if (resp.find(']', i) < resp.find('{', i)) break;
    }
    return true;
}

bool GrpcAgentClient::getStatus(GrpcAgentStatus& out)
{
    std::string resp;
    if (!_call("GetStatus", "{}", resp))
        return false;
    out.name = extractStringField(resp, "name");
    out.running = extractBoolField(resp, "running");
    out.cycleCount = extractUint64Field(resp, "cycle_count");
    out.atomCount = extractUint64Field(resp, "atom_count");
    out.pendingTasks = extractUint64Field(resp, "pending_tasks");
    out.report = extractStringField(resp, "report");
    return true;
}

bool GrpcAgentClient::queryAtoms(const std::string& namePrefix,
                                 const std::string& typeFilter,
                                 uint32_t limit,
                                 std::string& outAtomListJson)
{
    std::ostringstream params;
    params << "{\"name_prefix\":" << escape(namePrefix)
           << ",\"type\":" << escape(typeFilter)
           << ",\"limit\":" << limit << "}";
    std::string resp;
    if (!_call("QueryAtoms", params.str(), resp))
        return false;
    outAtomListJson = resp;
    return true;
}

} // namespace cog0
