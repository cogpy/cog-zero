/*
 * src/RestApiAdapter.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * RestApiAdapter Implementation
 * REST API tool adapter using POSIX sockets
 * Part of the AGENT-ZERO-GENESIS project - Phase 8: Tool Integration
 */

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include <opencog/agentzero/tools/RestApiAdapter.h>

using namespace opencog;
using namespace opencog::agentzero::tools;

// ========================================================================================
// HttpResponse
// ========================================================================================

std::string HttpResponse::toJSON() const
{
    std::ostringstream j;
    j << "{";
    j << "\"status_code\":" << status_code << ",";
    j << "\"success\":" << (success ? "true" : "false") << ",";
    j << "\"elapsed_ms\":" << std::fixed << std::setprecision(2) << elapsed_ms << ",";
    j << "\"body\":\"" << body << "\",";
    j << "\"error\":\"" << error << "\"";
    j << "}";
    return j.str();
}

// ========================================================================================
// RestApiAdapter
// ========================================================================================

RestApiAdapter::RestApiAdapter(const std::string& base_url, AtomSpacePtr atomspace)
    : _base_url(base_url)
    , _atomspace(atomspace)
{
    // Default headers
    _default_headers["Content-Type"] = "application/json";
    _default_headers["Accept"]       = "application/json";
    _default_headers["User-Agent"]   = "AgentZero-RestApiAdapter/1.0";

    logger().info() << "[RestApiAdapter] Initialised (base_url=" << _base_url << ")";
}

RestApiAdapter::~RestApiAdapter()
{
    logger().info() << "[RestApiAdapter] Destroyed (success=" << _success_count
                    << " failure=" << _failure_count << ")";
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void RestApiAdapter::setDefaultHeader(const std::string& key, const std::string& value)
{
    _default_headers[key] = value;
}

void RestApiAdapter::setBasicAuth(const std::string& username, const std::string& password)
{
    _default_headers["Authorization"] = "Basic " + encodeBase64(username + ":" + password);
}

void RestApiAdapter::setBearerToken(const std::string& token)
{
    _default_headers["Authorization"] = "Bearer " + token;
}

// ---------------------------------------------------------------------------
// HTTP Methods
// ---------------------------------------------------------------------------

HttpResponse RestApiAdapter::get(const std::string& path,
                                  const std::map<std::string, std::string>& params)
{
    std::string full_path = path;
    if (!params.empty()) {
        full_path += "?" + buildQueryString(params);
    }
    return request(HttpMethod::GET, full_path);
}

HttpResponse RestApiAdapter::post(const std::string& path, const std::string& body)
{
    return request(HttpMethod::POST, path, body);
}

HttpResponse RestApiAdapter::put(const std::string& path, const std::string& body)
{
    return request(HttpMethod::PUT, path, body);
}

HttpResponse RestApiAdapter::del(const std::string& path)
{
    return request(HttpMethod::DELETE_, path);
}

HttpResponse RestApiAdapter::request(HttpMethod method,
                                      const std::string& path,
                                      const std::string& body,
                                      const std::map<std::string, std::string>& extra_headers)
{
    std::string full_url = _base_url + path;
    ParsedUrl parsed = parseUrl(full_url);

    // Merge default and extra headers
    std::map<std::string, std::string> headers = _default_headers;
    for (const auto& kv : extra_headers) {
        headers[kv.first] = kv.second;
    }

    // Build HTTP/1.1 request
    std::ostringstream req;
    req << methodToString(method) << " " << parsed.path << " HTTP/1.1\r\n";
    req << "Host: " << parsed.host;
    if (parsed.port != 80 && parsed.port != 443) {
        req << ":" << parsed.port;
    }
    req << "\r\n";

    for (const auto& kv : headers) {
        req << kv.first << ": " << kv.second << "\r\n";
    }

    if (!body.empty()) {
        req << "Content-Length: " << body.size() << "\r\n";
    }
    req << "Connection: close\r\n";
    req << "\r\n";
    req << body;

    auto start = std::chrono::high_resolution_clock::now();
    HttpResponse response = sendRequest(parsed, req.str(), _timeout_ms);
    auto end = std::chrono::high_resolution_clock::now();

    response.elapsed_ms =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;

    updateStats(response, response.elapsed_ms);

    logger().debug() << "[RestApiAdapter] " << methodToString(method) << " " << path
                     << " -> " << response.status_code
                     << " (" << response.elapsed_ms << "ms)";

    return response;
}

// ---------------------------------------------------------------------------
// ToolWrapper integration
// ---------------------------------------------------------------------------

std::shared_ptr<ToolWrapper> RestApiAdapter::createToolWrapper(
    const std::string& tool_name,
    const std::string& path,
    AtomSpacePtr atomspace)
{
    AtomSpacePtr as = atomspace ? atomspace : _atomspace;
    auto tool = std::make_shared<ToolWrapper>(tool_name, ToolType::EXTERNAL_REST_API, as);
    tool->setToolEndpoint(_base_url + path);

    // Capture adapter and path for the custom executor
    auto adapter = std::shared_ptr<RestApiAdapter>(this, [](RestApiAdapter*) {});
    std::string api_path = path;

    tool->setCustomExecutor([adapter, api_path](const ToolExecutionContext& ctx) -> ToolResult {
        ToolResult result = adapter->callTool(api_path, ctx);
        return result;
    });

    logger().info() << "[RestApiAdapter] Created ToolWrapper '" << tool_name
                    << "' for path " << path;
    return tool;
}

ToolResult RestApiAdapter::callTool(const std::string& path,
                                     const ToolExecutionContext& context)
{
    // Serialise parameters to JSON
    std::ostringstream body_stream;
    body_stream << "{";
    bool first = true;
    for (const auto& kv : context.getAllParameters()) {
        if (!first) body_stream << ",";
        body_stream << "\"" << kv.first << "\":\"" << kv.second << "\"";
        first = false;
    }
    body_stream << "}";

    HttpResponse http_resp = post(path, body_stream.str());

    ToolResult tool_result(http_resp.success ? ToolStatus::COMPLETED : ToolStatus::FAILED);
    tool_result.setOutput(http_resp.body);
    tool_result.setExecutionTime(http_resp.elapsed_ms);

    if (!http_resp.success) {
        tool_result.setErrorMessage(http_resp.error.empty()
                                    ? "HTTP " + std::to_string(http_resp.status_code)
                                    : http_resp.error);
    }

    tool_result.setMetadata("status_code", std::to_string(http_resp.status_code));
    tool_result.setMetadata("path", path);

    if (context.getAtomSpace()) {
        Handle result_node = responseToAtom(http_resp, context.getAtomSpace());
        if (result_node != Handle::UNDEFINED) {
            tool_result.addAtomSpaceResult(result_node);
        }
    }

    return tool_result;
}

// ---------------------------------------------------------------------------
// AtomSpace conversion
// ---------------------------------------------------------------------------

Handle RestApiAdapter::responseToAtom(const HttpResponse& response,
                                       AtomSpacePtr atomspace) const
{
    AtomSpacePtr as = atomspace ? atomspace : _atomspace;
    if (!as) {
        return Handle::UNDEFINED;
    }

    try {
        std::string node_name = "HttpResponse_" + std::to_string(response.status_code);
        Handle resp_node = as->add_node(CONCEPT_NODE, node_name);

        double strength = (response.success) ? 1.0 : 0.0;
        TruthValuePtr tv = SimpleTruthValue::createTV(strength, 1.0);
        resp_node->setTruthValue(tv);

        // Store response body as a predicate evaluation
        if (!response.body.empty()) {
            Handle pred_node  = as->add_node(PREDICATE_NODE, "http_body");
            Handle body_node  = as->add_node(CONCEPT_NODE, response.body.substr(0, 256));
            HandleSeq eval_seq;
            eval_seq.push_back(pred_node);
            HandleSeq list_seq;
            list_seq.push_back(resp_node);
            list_seq.push_back(body_node);
            Handle list_link = as->add_link(LIST_LINK, std::move(list_seq));
            eval_seq.push_back(list_link);
            as->add_link(EVALUATION_LINK, std::move(eval_seq));
        }

        return resp_node;

    } catch (const std::exception& e) {
        logger().error() << "[RestApiAdapter] responseToAtom failed: " << e.what();
        return Handle::UNDEFINED;
    }
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

std::string RestApiAdapter::getStatistics() const
{
    int total = _success_count + _failure_count;
    double rate = (total > 0)
                ? (static_cast<double>(_success_count) / static_cast<double>(total))
                : 0.0;
    double avg = (total > 0) ? (_total_elapsed_ms / static_cast<double>(total)) : 0.0;

    std::ostringstream j;
    j << "{";
    j << "\"base_url\":\"" << _base_url << "\",";
    j << "\"total_requests\":" << total << ",";
    j << "\"success_count\":" << _success_count << ",";
    j << "\"failure_count\":" << _failure_count << ",";
    j << "\"success_rate\":" << std::fixed << std::setprecision(3) << rate << ",";
    j << "\"avg_elapsed_ms\":" << std::fixed << std::setprecision(2) << avg;
    j << "}";
    return j.str();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

RestApiAdapter::ParsedUrl RestApiAdapter::parseUrl(const std::string& url) const
{
    ParsedUrl parsed;
    parsed.port = 80;

    // Determine scheme
    std::string rest = url;
    if (rest.substr(0, 8) == "https://") {
        parsed.scheme  = "https";
        parsed.port    = 443;
        rest = rest.substr(8);
    } else if (rest.substr(0, 7) == "http://") {
        parsed.scheme = "http";
        rest = rest.substr(7);
    } else {
        parsed.scheme = "http";
    }

    // Split host[:port] from path
    size_t slash_pos = rest.find('/');
    std::string host_part;
    if (slash_pos == std::string::npos) {
        host_part    = rest;
        parsed.path  = "/";
    } else {
        host_part   = rest.substr(0, slash_pos);
        parsed.path = rest.substr(slash_pos);
    }

    // Extract optional port from host
    size_t colon_pos = host_part.find(':');
    if (colon_pos != std::string::npos) {
        parsed.host = host_part.substr(0, colon_pos);
        try {
            parsed.port = std::stoi(host_part.substr(colon_pos + 1));
        } catch (...) { /* keep default */ }
    } else {
        parsed.host = host_part;
    }

    if (parsed.path.empty()) {
        parsed.path = "/";
    }

    return parsed;
}

std::string RestApiAdapter::buildQueryString(
    const std::map<std::string, std::string>& params) const
{
    std::ostringstream qs;
    bool first = true;
    for (const auto& kv : params) {
        if (!first) qs << "&";
        qs << urlEncode(kv.first) << "=" << urlEncode(kv.second);
        first = false;
    }
    return qs.str();
}

std::string RestApiAdapter::methodToString(HttpMethod method) const
{
    switch (method) {
        case HttpMethod::GET:     return "GET";
        case HttpMethod::POST:    return "POST";
        case HttpMethod::PUT:     return "PUT";
        case HttpMethod::DELETE_: return "DELETE";
        case HttpMethod::PATCH:   return "PATCH";
    }
    return "GET";
}

HttpResponse RestApiAdapter::sendRequest(const ParsedUrl& parsed,
                                          const std::string& raw_request,
                                          double timeout_ms)
{
    HttpResponse response;

    // Resolve host
    struct addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* res = nullptr;
    std::string port_str = std::to_string(parsed.port);
    int rc = getaddrinfo(parsed.host.c_str(), port_str.c_str(), &hints, &res);
    if (rc != 0) {
        response.success = false;
        response.error   = std::string("DNS resolution failed: ") + gai_strerror(rc);
        logger().warn() << "[RestApiAdapter] " << response.error;
        return response;
    }

    // Create socket
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        freeaddrinfo(res);
        response.success = false;
        response.error   = "socket() failed";
        return response;
    }

    // Bound connect + I/O so offline / firewalled endpoints cannot hang tests.
    double effective_timeout = timeout_ms > 0.0 ? timeout_ms : 1000.0;
    if (effective_timeout > 10000.0) effective_timeout = 10000.0;

    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    }

    int cret = connect(sockfd, res->ai_addr, res->ai_addrlen);
    if (cret < 0 && errno != EINPROGRESS) {
        freeaddrinfo(res);
        close(sockfd);
        response.success = false;
        response.error   = "connect() failed: " + std::string(strerror(errno));
        logger().warn() << "[RestApiAdapter] " << response.error;
        return response;
    }
    if (cret < 0 && errno == EINPROGRESS) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(sockfd, &wfds);
        struct timeval tv{};
        tv.tv_sec  = static_cast<long>(effective_timeout / 1000.0);
        tv.tv_usec = static_cast<long>(std::fmod(effective_timeout, 1000.0) * 1000.0);
        int sel = select(sockfd + 1, nullptr, &wfds, nullptr, &tv);
        if (sel <= 0) {
            freeaddrinfo(res);
            close(sockfd);
            response.success = false;
            response.error   = "connect() timed out";
            logger().warn() << "[RestApiAdapter] " << response.error;
            return response;
        }
        int so_error = 0;
        socklen_t len = sizeof(so_error);
        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error != 0) {
            freeaddrinfo(res);
            close(sockfd);
            response.success = false;
            response.error   = "connect() failed: " + std::string(strerror(so_error));
            logger().warn() << "[RestApiAdapter] " << response.error;
            return response;
        }
    }
    freeaddrinfo(res);

    if (flags >= 0) {
        fcntl(sockfd, F_SETFL, flags);
    }
    struct timeval iotv{};
    iotv.tv_sec  = static_cast<long>(effective_timeout / 1000.0);
    iotv.tv_usec = static_cast<long>(std::fmod(effective_timeout, 1000.0) * 1000.0);
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &iotv, sizeof(iotv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &iotv, sizeof(iotv));

    // Send request
    size_t total_sent = 0;
    while (total_sent < raw_request.size()) {
        ssize_t sent = send(sockfd, raw_request.c_str() + total_sent,
                            raw_request.size() - total_sent, 0);
        if (sent <= 0) {
            close(sockfd);
            response.success = false;
            response.error   = "send() failed";
            return response;
        }
        total_sent += static_cast<size_t>(sent);
    }

    // Receive response
    std::string raw_response;
    {
        std::array<char, 4096> buf{};
        while (true) {
            ssize_t n = recv(sockfd, buf.data(), buf.size() - 1, 0);
            if (n <= 0) break;
            buf[n] = '\0';
            raw_response.append(buf.data(), static_cast<size_t>(n));
        }
    }
    close(sockfd);

    // Parse status line
    size_t crlf1 = raw_response.find("\r\n");
    if (crlf1 == std::string::npos) {
        response.success = false;
        response.error   = "Malformed HTTP response";
        return response;
    }
    std::string status_line = raw_response.substr(0, crlf1);
    // "HTTP/1.1 200 OK"
    size_t sp1 = status_line.find(' ');
    size_t sp2 = (sp1 != std::string::npos) ? status_line.find(' ', sp1 + 1) : std::string::npos;
    if (sp1 != std::string::npos) {
        try {
            response.status_code = std::stoi(
                status_line.substr(sp1 + 1, (sp2 != std::string::npos) ? sp2 - sp1 - 1 : std::string::npos));
        } catch (...) {}
    }

    // Split headers from body
    size_t header_end = raw_response.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        response.body = raw_response.substr(header_end + 4);

        // Parse response headers
        std::string headers_section = raw_response.substr(crlf1 + 2, header_end - crlf1 - 2);
        std::istringstream hs(headers_section);
        std::string line;
        while (std::getline(hs, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string val = line.substr(colon + 1);
                // Trim leading space
                if (!val.empty() && val[0] == ' ') val = val.substr(1);
                response.headers[key] = val;
            }
        }
    } else {
        response.body = raw_response;
    }

    response.success = (response.status_code >= 200 && response.status_code < 300);
    if (!response.success && response.error.empty()) {
        response.error = "HTTP " + std::to_string(response.status_code);
    }

    return response;
}

std::string RestApiAdapter::encodeBase64(const std::string& input) const
{
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i < input.size()) {
        unsigned char b0 = static_cast<unsigned char>(input[i++]);
        bool has_b1      = (i < input.size());
        unsigned char b1 = has_b1 ? static_cast<unsigned char>(input[i++]) : 0;
        bool has_b2      = (i < input.size());
        unsigned char b2 = has_b2 ? static_cast<unsigned char>(input[i++]) : 0;

        output += table[(b0 >> 2) & 0x3F];
        output += table[((b0 & 0x03) << 4) | ((b1 >> 4) & 0x0F)];
        output += (!has_b1) ? '=' : table[((b1 & 0x0F) << 2) | ((b2 >> 6) & 0x03)];
        output += (!has_b2) ? '=' : table[b2 & 0x3F];
    }

    return output;
}

std::string RestApiAdapter::urlEncode(const std::string& value) const
{
    std::ostringstream encoded;
    encoded << std::hex << std::uppercase;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << static_cast<char>(c);
        } else {
            encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }
    return encoded.str();
}

void RestApiAdapter::updateStats(const HttpResponse& response, double elapsed_ms)
{
    if (response.success) {
        ++_success_count;
    } else {
        ++_failure_count;
    }
    _total_elapsed_ms += elapsed_ms;
}
