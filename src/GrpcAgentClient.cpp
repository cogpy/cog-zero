/*
 * src/GrpcAgentClient.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * AgentService client — length-prefixed JSON transport (gRPC fallback).
 */

#include "cog0/GrpcAgentClient.h"

#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace cog0 {
namespace {

std::string jsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
    return out;
}

bool responseOk(const std::string& json)
{
    auto pos = json.find("\"ok\"");
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    auto t = json.find("true", pos);
    auto f = json.find("false", pos);
    if (t == std::string::npos) return false;
    if (f != std::string::npos && f < t) return false;
    return true;
}

std::string extractStringField(const std::string& json, const std::string& key)
{
    const std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return {};
    size_t i = pos + 1;
    std::string out;
    while (i < json.size()) {
        char c = json[i++];
        if (c == '\\' && i < json.size()) {
            char n = json[i++];
            switch (n) {
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                default: out.push_back(n); break;
            }
        } else if (c == '"') {
            break;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

uint64_t extractUint64Field(const std::string& json, const std::string& key, uint64_t def = 0)
{
    const std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) return def;
    pos = json.find_first_of("0123456789", pos + 1);
    if (pos == std::string::npos) return def;
    try {
        size_t end = 0;
        unsigned long long v = std::stoull(json.substr(pos), &end);
        return static_cast<uint64_t>(v);
    } catch (...) {
        return def;
    }
}

bool extractBoolField(const std::string& json, const std::string& key, bool def = false)
{
    const std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) return def;
    auto t = json.find("true", pos);
    auto f = json.find("false", pos);
    if (t != std::string::npos && (f == std::string::npos || t < f)) return true;
    if (f != std::string::npos) return false;
    return def;
}

} // namespace

GrpcAgentClient::GrpcAgentClient(const std::string& host, uint16_t port)
{
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

    int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        _lastError = "socket() failed";
        ::freeaddrinfo(res);
        return;
    }

    if (::connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        _lastError = "connect() failed to " + host + ":" + portStr;
        ::close(fd);
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
        ::close(_fd);
        _fd = -1;
    }
}

bool GrpcAgentClient::_sendFrame(int fd, const std::string& payload)
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

bool GrpcAgentClient::_recvFrame(int fd, std::string& out)
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
    if (len > 16u * 1024u * 1024u)
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

bool GrpcAgentClient::_call(const std::string& method,
                            const std::string& paramsJson,
                            std::string& outResponseJson)
{
    if (_fd < 0) {
        _lastError = "not connected";
        return false;
    }

    std::ostringstream req;
    req << "{\"method\":" << jsonEscape(method)
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
    params << "{\"name\":" << jsonEscape(name)
           << ",\"description\":" << jsonEscape(description)
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
    params << "{\"source\":" << jsonEscape(source)
           << ",\"content\":" << jsonEscape(content)
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
    params << "{\"name_prefix\":" << jsonEscape(namePrefix)
           << ",\"type\":" << jsonEscape(typeFilter)
           << ",\"limit\":" << limit << "}";
    std::string resp;
    if (!_call("QueryAtoms", params.str(), resp))
        return false;
    outAtomListJson = resp;
    return true;
}

} // namespace cog0
