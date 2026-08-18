/*
 * src/TlsSocket.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Optional OpenSSL-backed TLS socket wrapper.
 */

#include "cog0/TlsSocket.h"

#include <cstring>

#ifdef COG0_HAVE_OPENSSL
#  include <openssl/err.h>
#  include <openssl/ssl.h>
#endif

namespace cog0 {

// =========================================================================
// Availability
// =========================================================================

bool tlsSupportAvailable()
{
#ifdef COG0_HAVE_OPENSSL
    return true;
#else
    return false;
#endif
}

// =========================================================================
// TlsContext
// =========================================================================

TlsContext::TlsContext() = default;

TlsContext::~TlsContext()
{
#ifdef COG0_HAVE_OPENSSL
    if (_ctx) {
        SSL_CTX_free(static_cast<SSL_CTX*>(_ctx));
        _ctx = nullptr;
    }
#endif
}

bool TlsContext::load(const std::string& certPath, const std::string& keyPath)
{
    _certPath = certPath;
    _keyPath = keyPath;
    _ready = false;
    _lastError.clear();

#ifndef COG0_HAVE_OPENSSL
    _lastError = "OpenSSL support not compiled in (rebuild with OpenSSL)";
    return false;
#else
    if (certPath.empty() || keyPath.empty()) {
        _lastError = "certificate and key paths are required";
        return false;
    }

    // Ensure OpenSSL is initialized (idempotent on modern OpenSSL).
    OPENSSL_init_ssl(0, nullptr);

    if (_ctx) {
        SSL_CTX_free(static_cast<SSL_CTX*>(_ctx));
        _ctx = nullptr;
    }

    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        _lastError = "SSL_CTX_new failed";
        return false;
    }

    // Prefer TLS 1.2+
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    if (SSL_CTX_use_certificate_file(ctx, certPath.c_str(), SSL_FILETYPE_PEM) != 1) {
        _lastError = "failed to load certificate: " + certPath;
        SSL_CTX_free(ctx);
        return false;
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, keyPath.c_str(), SSL_FILETYPE_PEM) != 1) {
        _lastError = "failed to load private key: " + keyPath;
        SSL_CTX_free(ctx);
        return false;
    }

    if (SSL_CTX_check_private_key(ctx) != 1) {
        _lastError = "private key does not match certificate";
        SSL_CTX_free(ctx);
        return false;
    }

    _ctx = ctx;
    _ready = true;
    return true;
#endif
}

// =========================================================================
// TlsSocket
// =========================================================================

TlsSocket::TlsSocket(int fd, void* ssl)
    : _fd(fd)
    , _ssl(ssl)
{}

TlsSocket::~TlsSocket()
{
    shutdown();
#ifdef COG0_HAVE_OPENSSL
    if (_ssl) {
        SSL_free(static_cast<SSL*>(_ssl));
        _ssl = nullptr;
    }
#endif
}

std::unique_ptr<TlsSocket> TlsSocket::acceptClient(int fd, TlsContext& ctx)
{
#ifndef COG0_HAVE_OPENSSL
    (void)fd;
    (void)ctx;
    return nullptr;
#else
    if (!ctx.ready() || !ctx.nativeHandle() || fd < 0)
        return nullptr;

    SSL* ssl = SSL_new(static_cast<SSL_CTX*>(ctx.nativeHandle()));
    if (!ssl)
        return nullptr;

    SSL_set_fd(ssl, fd);

    if (SSL_accept(ssl) != 1) {
        SSL_free(ssl);
        return nullptr;
    }

    return std::unique_ptr<TlsSocket>(new TlsSocket(fd, ssl));
#endif
}

int TlsSocket::send(const void* data, size_t len)
{
#ifdef COG0_HAVE_OPENSSL
    if (!_ssl || !data || len == 0) return 0;
    int n = SSL_write(static_cast<SSL*>(_ssl), data, static_cast<int>(len));
    return n;
#else
    (void)data;
    (void)len;
    return -1;
#endif
}

int TlsSocket::recv(void* data, size_t len)
{
#ifdef COG0_HAVE_OPENSSL
    if (!_ssl || !data || len == 0) return 0;
    int n = SSL_read(static_cast<SSL*>(_ssl), data, static_cast<int>(len));
    return n;
#else
    (void)data;
    (void)len;
    return -1;
#endif
}

void TlsSocket::shutdown()
{
#ifdef COG0_HAVE_OPENSSL
    if (_ssl) {
        SSL_shutdown(static_cast<SSL*>(_ssl));
    }
#endif
}

} // namespace cog0
