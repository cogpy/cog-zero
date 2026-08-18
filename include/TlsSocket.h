/*
 * include/TlsSocket.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Phase 14 Feature 2.5 — Optional TLS wrapper around POSIX sockets.
 *
 * When OpenSSL is available at build time (COG0_HAVE_OPENSSL), TlsContext
 * and TlsSocket provide certificate loading, server-side handshake, and
 * encrypted send/recv.  Without OpenSSL the types still compile but
 * TlsContext::load() returns false and isAvailable() is false.
 */

#pragma once

#include <cstddef>
#include <memory>
#include <string>

namespace cog0 {

/// Returns true when this build was compiled with OpenSSL support.
[[nodiscard]] bool tlsSupportAvailable();

/// Server-side TLS context (certificate + private key).
class TlsContext {
public:
    TlsContext();
    ~TlsContext();

    TlsContext(const TlsContext&) = delete;
    TlsContext& operator=(const TlsContext&) = delete;

    /// Load PEM certificate and private key. Returns false on failure or
    /// when OpenSSL support is not compiled in.
    bool load(const std::string& certPath, const std::string& keyPath);

    [[nodiscard]] bool ready() const { return _ready; }
    [[nodiscard]] const std::string& lastError() const { return _lastError; }
    [[nodiscard]] const std::string& certPath() const { return _certPath; }
    [[nodiscard]] const std::string& keyPath() const { return _keyPath; }

    /// Opaque native SSL_CTX* (nullptr when unavailable).
    [[nodiscard]] void* nativeHandle() const { return _ctx; }

private:
    void* _ctx = nullptr;  // SSL_CTX*
    bool _ready = false;
    std::string _certPath;
    std::string _keyPath;
    std::string _lastError;
};

/// TLS session bound to an accepted client socket (server side).
/// Owns the SSL session; does NOT own/close the underlying fd by default.
class TlsSocket {
public:
    /// Perform server-side TLS handshake on an already-accepted fd.
    /// Returns nullptr on failure.
    static std::unique_ptr<TlsSocket> acceptClient(int fd, TlsContext& ctx);

    ~TlsSocket();

    TlsSocket(const TlsSocket&) = delete;
    TlsSocket& operator=(const TlsSocket&) = delete;

    /// Encrypted send. Returns bytes written, or <=0 on error/EOF.
    int send(const void* data, size_t len);

    /// Encrypted recv. Returns bytes read, or <=0 on error/EOF.
    int recv(void* data, size_t len);

    /// Graceful TLS shutdown (does not close fd).
    void shutdown();

    [[nodiscard]] int fd() const { return _fd; }
    [[nodiscard]] bool ok() const { return _ssl != nullptr; }

private:
    TlsSocket(int fd, void* ssl);

    int _fd = -1;
    void* _ssl = nullptr;  // SSL*
};

} // namespace cog0
