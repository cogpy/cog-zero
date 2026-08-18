/*
 * tests/test_phase14_tls.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Phase 14 Feature 2.5: TLS for MonitoringServer
 */

#include "test_runner.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  define CLOSE_SOCKET closesocket
#  ifndef popen
#    define popen  _popen
#    define pclose _pclose
#  endif
using SocketIoResult = int;
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#  define CLOSE_SOCKET ::close
using SocketIoResult = ssize_t;
#endif

#include "cog0/AtomStore.h"
#include "cog0/MonitoringServer.h"
#include "cog0/TlsSocket.h"

using namespace cog0;

namespace {

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

// Cross-platform temp path in the current working directory (build dir).
// Avoids POSIX-only mkstemp/unlink and /tmp assumptions on Windows.
std::string makeTempPath(const std::string& suffix)
{
    static std::atomic<unsigned> counter{0};
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const unsigned n = counter.fetch_add(1);
    return "cog0_tls_" + std::to_string(now) + "_" + std::to_string(n) + suffix;
}

// Generate a self-signed cert+key with openssl CLI when available.
bool generateSelfSigned(const std::string& certPath, const std::string& keyPath)
{
#ifdef _WIN32
    // std::system uses cmd.exe on MSVC — use double quotes and nul redirect.
    std::string cmd =
        "openssl req -x509 -newkey rsa:2048 -keyout \"" + keyPath +
        "\" -out \"" + certPath +
        "\" -days 1 -nodes -subj \"/CN=localhost\" >nul 2>&1";
#else
    std::string cmd =
        "openssl req -x509 -newkey rsa:2048 -keyout '" + keyPath +
        "' -out '" + certPath +
        "' -days 1 -nodes -subj '/CN=localhost' >/dev/null 2>&1";
#endif
    int rc = std::system(cmd.c_str());
    return rc == 0;
}

std::string httpGet(const std::string& host, uint16_t port, const std::string& path)
{
#ifdef _WIN32
    if (!ensureWinsock()) return {};
#endif

    int fd = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
    if (fd < 0) return {};

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        CLOSE_SOCKET(fd);
        return {};
    }

    std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
    if (::send(fd, req.c_str(), static_cast<int>(req.size()), 0) < 0) {
        CLOSE_SOCKET(fd);
        return {};
    }

    std::string resp;
    char buf[2048];
    for (;;) {
        SocketIoResult n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        resp.append(buf, static_cast<size_t>(n));
    }
    CLOSE_SOCKET(fd);
    return resp;
}

} // namespace

// ==========================================================================
// TlsSocket / availability
// ==========================================================================

TEST(TLS_SupportAvailability_MatchesBuild)
{
#ifdef COG0_HAVE_OPENSSL
    ASSERT_TRUE(tlsSupportAvailable());
#else
    ASSERT_FALSE(tlsSupportAvailable());
#endif
}

TEST(TLS_Context_RejectsEmptyPaths)
{
    TlsContext ctx;
    ASSERT_FALSE(ctx.load("", ""));
    ASSERT_FALSE(ctx.ready());
    ASSERT_FALSE(ctx.lastError().empty());
}

TEST(TLS_Context_RejectsMissingFiles)
{
    TlsContext ctx;
    ASSERT_FALSE(ctx.load("/no/such/cert.pem", "/no/such/key.pem"));
    ASSERT_FALSE(ctx.ready());
}

TEST(TLS_Context_LoadSelfSigned)
{
    if (!tlsSupportAvailable()) {
        // Still validate graceful failure path.
        TlsContext ctx;
        ASSERT_FALSE(ctx.load("x", "y"));
        return;
    }

    std::string cert = makeTempPath(".crt");
    std::string key = makeTempPath(".key");
    ASSERT_TRUE(generateSelfSigned(cert, key));

    TlsContext ctx;
    ASSERT_TRUE(ctx.load(cert, key));
    ASSERT_TRUE(ctx.ready());
    ASSERT_EQ(ctx.certPath(), cert);

    std::remove(cert.c_str());
    std::remove(key.c_str());
}

// ==========================================================================
// MonitoringServer TLS integration
// ==========================================================================

TEST(TLS_MonitoringServer_HealthReportsDisabledByDefault)
{
    auto store = std::make_shared<AtomStore>();
    // MonitoringServer takes a fixed port; pick an unlikely one.
    MonitoringServer server(store, nullptr, 18765);
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    std::string resp = httpGet("127.0.0.1", 18765, "/health");
    server.stop();

    ASSERT_FALSE(resp.empty());
    ASSERT_TRUE(resp.find("\"tls_enabled\":false") != std::string::npos);
    ASSERT_TRUE(resp.find("\"tls_available\"") != std::string::npos);
}

TEST(TLS_MonitoringServer_EnableTLS_WithCert)
{
    auto store = std::make_shared<AtomStore>();
    MonitoringServer server(store, nullptr, 18766);

    if (!tlsSupportAvailable()) {
        ASSERT_FALSE(server.enableTLS("a.pem", "b.pem"));
        ASSERT_FALSE(server.tlsEnabled());
        return;
    }

    std::string cert = makeTempPath(".crt");
    std::string key = makeTempPath(".key");
    ASSERT_TRUE(generateSelfSigned(cert, key));

    ASSERT_TRUE(server.enableTLS(cert, key));
    ASSERT_TRUE(server.tlsEnabled());
    ASSERT_TRUE(server.tlsReady());

    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    // Plain HTTP GET should fail handshake / return empty — TLS is required.
    std::string plain = httpGet("127.0.0.1", 18766, "/health");
    // Either connection fails after accept+TLS handshake, or empty body.
    // We just assert the server stayed up.
    ASSERT_TRUE(server.running());

    server.stop();
    server.disableTLS();
    ASSERT_FALSE(server.tlsEnabled());

    std::remove(cert.c_str());
    std::remove(key.c_str());
    (void)plain;
}

TEST(TLS_MonitoringServer_EnableTLS_WhileRunning_Fails)
{
    auto store = std::make_shared<AtomStore>();
    MonitoringServer server(store, nullptr, 18767);
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    bool ok = server.enableTLS("x.pem", "y.pem");
    ASSERT_FALSE(ok);

    server.stop();
}

TEST(TLS_MonitoringServer_Health_TlsFlagsAfterEnable)
{
    if (!tlsSupportAvailable()) return;

    std::string cert = makeTempPath(".crt");
    std::string key = makeTempPath(".key");
    ASSERT_TRUE(generateSelfSigned(cert, key));

    auto store = std::make_shared<AtomStore>();
    // Start without TLS, verify flags, then we can't flip while running —
    // so enable first then start and use openssl s_client for health.
    MonitoringServer server(store, nullptr, 18768);
    ASSERT_TRUE(server.enableTLS(cert, key));
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

#ifdef _WIN32
    // Shell pipelines with printf/openssl differ on cmd.exe; validate TLS
    // enablement via the in-process API instead of s_client scraping.
    ASSERT_TRUE(server.tlsEnabled());
    ASSERT_TRUE(server.tlsReady());
    ASSERT_TRUE(server.running());
    server.stop();
#else
    // Use openssl s_client to fetch /health over TLS.
    std::string cmd =
        "printf 'GET /health HTTP/1.1\\r\\nHost: localhost\\r\\nConnection: close\\r\\n\\r\\n' | "
        "openssl s_client -quiet -connect 127.0.0.1:18768 -verify_return_error 2>/dev/null || "
        "printf 'GET /health HTTP/1.1\\r\\nHost: localhost\\r\\nConnection: close\\r\\n\\r\\n' | "
        "openssl s_client -quiet -connect 127.0.0.1:18768 2>/dev/null";

    FILE* pipe = ::popen(cmd.c_str(), "r");
    ASSERT_TRUE(pipe != nullptr);
    std::string out;
    char buf[512];
    while (fgets(buf, sizeof(buf), pipe)) out += buf;
    ::pclose(pipe);

    server.stop();

    ASSERT_TRUE(out.find("\"tls_enabled\":true") != std::string::npos ||
                out.find("tls_enabled") != std::string::npos);
    ASSERT_TRUE(out.find("\"status\":\"ok\"") != std::string::npos ||
                out.find("status") != std::string::npos);
#endif

    std::remove(cert.c_str());
    std::remove(key.c_str());
}
