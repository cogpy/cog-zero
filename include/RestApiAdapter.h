/*
 * opencog/agentzero/tools/RestApiAdapter.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * RestApiAdapter - REST API tool adapter
 * Part of the AGENT-ZERO-GENESIS project - Phase 8: Tool Integration
 */

#ifndef _OPENCOG_AGENTZERO_RESTAPIADAPTER_H
#define _OPENCOG_AGENTZERO_RESTAPIADAPTER_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#include <opencog/agentzero/tools/ToolWrapper.h>

namespace opencog {
namespace agentzero {
namespace tools {

/**
 * HttpMethod - Supported HTTP request methods
 */
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE_,
    PATCH
};

/**
 * HttpResponse - Result of an HTTP request
 */
struct HttpResponse {
    int status_code{0};
    std::string body;
    std::map<std::string, std::string> headers;
    double elapsed_ms{0.0};
    bool success{false};
    std::string error;

    /** Serialise to a JSON string */
    std::string toJSON() const;
};

/**
 * RestApiAdapter - Adapter that exposes REST API endpoints as Agent-Zero tools
 *
 * Wraps HTTP REST calls as ToolWrapper-compatible tools. Supports:
 *  - HTTP GET, POST, PUT, DELETE, PATCH methods
 *  - JSON request/response serialisation
 *  - Basic and Bearer-token authentication
 *  - Per-request timeout enforcement
 *  - Conversion of HTTP responses to AtomSpace representations
 *
 * The adapter uses POSIX socket APIs so no additional library dependency
 * (libcurl, Boost.Beast, etc.) is required. HTTPS is not supported in this
 * baseline implementation; it can be layered on top with OpenSSL if needed.
 *
 * Usage example:
 * @code
 *   RestApiAdapter adapter("http://localhost:8080", atomspace);
 *   adapter.setDefaultHeader("Accept", "application/json");
 *
 *   HttpResponse r = adapter.get("/api/v1/status");
 *   if (r.success) {
 *       std::cout << r.body << std::endl;
 *   }
 *
 *   // Or as a ToolWrapper
 *   auto tool = adapter.createToolWrapper("query_tool", "/api/v1/query");
 * @endcode
 */
class RestApiAdapter
{
public:
    /**
     * Constructor
     * @param base_url   Base URL for the REST API (e.g. "http://localhost:8080")
     * @param atomspace  Optional AtomSpace for result integration
     */
    explicit RestApiAdapter(const std::string& base_url = "",
                            AtomSpacePtr atomspace = nullptr);

    /** Destructor */
    ~RestApiAdapter();

    // ----------- Configuration -----------

    /**
     * Set the base URL for all requests
     * @param url Base URL string (scheme + host + optional port)
     */
    void setBaseUrl(const std::string& url) { _base_url = url; }

    /** Get the current base URL */
    const std::string& getBaseUrl() const { return _base_url; }

    /**
     * Set the default request timeout
     * @param timeout_ms Timeout in milliseconds (default: 30 000)
     */
    void setTimeout(double timeout_ms) { _timeout_ms = timeout_ms; }

    /**
     * Set a default HTTP header sent with every request
     * @param key   Header name
     * @param value Header value
     */
    void setDefaultHeader(const std::string& key, const std::string& value);

    /**
     * Configure Basic-Auth credentials
     * @param username Username
     * @param password Password
     */
    void setBasicAuth(const std::string& username, const std::string& password);

    /**
     * Set Bearer-token for the Authorization header
     * @param token Bearer token string
     */
    void setBearerToken(const std::string& token);

    /**
     * Set the AtomSpace used to convert responses to atoms
     * @param atomspace AtomSpace instance
     */
    void setAtomSpace(AtomSpacePtr atomspace) { _atomspace = atomspace; }

    // ----------- HTTP methods -----------

    /**
     * Perform an HTTP GET request
     * @param path    Path relative to the base URL (e.g. "/api/v1/status")
     * @param params  Optional query-string parameters
     * @return HTTP response
     */
    HttpResponse get(const std::string& path,
                     const std::map<std::string, std::string>& params = {});

    /**
     * Perform an HTTP POST request
     * @param path Path relative to the base URL
     * @param body Request body (typically JSON)
     * @return HTTP response
     */
    HttpResponse post(const std::string& path, const std::string& body = "");

    /**
     * Perform an HTTP PUT request
     * @param path Path relative to the base URL
     * @param body Request body
     * @return HTTP response
     */
    HttpResponse put(const std::string& path, const std::string& body = "");

    /**
     * Perform an HTTP DELETE request
     * @param path Path relative to the base URL
     * @return HTTP response
     */
    HttpResponse del(const std::string& path);

    /**
     * Generic HTTP request
     *
     * @param method        HTTP method
     * @param path          Path relative to the base URL
     * @param body          Optional request body
     * @param extra_headers Additional headers merged with the defaults
     * @return HTTP response
     */
    HttpResponse request(HttpMethod method,
                         const std::string& path,
                         const std::string& body = "",
                         const std::map<std::string, std::string>& extra_headers = {});

    // ----------- ToolWrapper integration -----------

    /**
     * Create a ToolWrapper that POSTs to a REST endpoint
     *
     * The ToolWrapper's execute() serialises the ToolExecutionContext
     * parameters as a flat JSON object and POSTs it to @p path.
     *
     * @param tool_name  Name for the resulting ToolWrapper
     * @param path       API path to call
     * @param atomspace  AtomSpace for result atoms (falls back to adapter's)
     * @return Configured ToolWrapper
     */
    std::shared_ptr<ToolWrapper> createToolWrapper(
        const std::string& tool_name,
        const std::string& path,
        AtomSpacePtr atomspace = nullptr);

    /**
     * Execute a ToolExecutionContext as a REST call
     *
     * Parameters from the context are serialised to JSON and POSTed.
     *
     * @param path    API path
     * @param context Execution context
     * @return ToolResult wrapping the HTTP response
     */
    ToolResult callTool(const std::string& path,
                        const ToolExecutionContext& context);

    // ----------- AtomSpace conversion -----------

    /**
     * Convert an HTTP response body to an AtomSpace atom
     *
     * Creates a ConceptNode whose name encodes the status code and whose
     * PredicateNode children hold the response fields.
     *
     * @param response  HTTP response to convert
     * @param atomspace Target AtomSpace (uses adapter's if nullptr)
     * @return Handle to root result node; Handle::UNDEFINED on failure
     */
    Handle responseToAtom(const HttpResponse& response,
                          AtomSpacePtr atomspace = nullptr) const;

    // ----------- Statistics -----------

    /** Number of requests that received a 2xx response */
    int getSuccessCount() const { return _success_count; }

    /** Number of requests that failed (connection error or non-2xx) */
    int getFailureCount() const { return _failure_count; }

    /**
     * Return aggregated statistics as a JSON string
     */
    std::string getStatistics() const;

private:
    std::string _base_url;
    double _timeout_ms{30000.0};
    std::map<std::string, std::string> _default_headers;
    AtomSpacePtr _atomspace;

    int _success_count{0};
    int _failure_count{0};
    double _total_elapsed_ms{0.0};

    // Internal representation of a parsed URL
    struct ParsedUrl {
        std::string scheme;
        std::string host;
        int port{80};
        std::string path;
    };

    ParsedUrl parseUrl(const std::string& url) const;
    std::string buildQueryString(const std::map<std::string, std::string>& params) const;
    std::string methodToString(HttpMethod method) const;
    HttpResponse sendRequest(const ParsedUrl& parsed,
                             const std::string& raw_request,
                             double timeout_ms);
    std::string encodeBase64(const std::string& input) const;
    std::string urlEncode(const std::string& value) const;
    void updateStats(const HttpResponse& response, double elapsed_ms);
};

} // namespace tools
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_RESTAPIADAPTER_H
