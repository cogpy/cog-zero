/*
 * standalone/src/WebSocketHandler.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * WebSocket protocol handler implementation.
 * Pure C++17 implementation with embedded SHA-1 and Base64.
 */

#include "cog0/WebSocketHandler.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <random>
#include <sstream>

namespace cog0 {

// ==========================================================================
// Constants
// ==========================================================================

// RFC 6455 GUID used in Sec-WebSocket-Accept computation
static constexpr const char* WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// Base64 encoding table
static constexpr const char* BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// ==========================================================================
// Handshake utilities
// ==========================================================================

bool WebSocketHandler::isUpgradeRequest(const std::string& headers)
{
    // Look for case-insensitive headers
    std::string lower = headers;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    bool hasUpgrade = lower.find("upgrade:") != std::string::npos &&
                      lower.find("websocket") != std::string::npos;
    bool hasConnection = lower.find("connection:") != std::string::npos &&
                         lower.find("upgrade") != std::string::npos;

    return hasUpgrade && hasConnection;
}

std::string WebSocketHandler::extractSecKey(const std::string& headers)
{
    // Find "Sec-WebSocket-Key:" header (case-insensitive)
    std::string lower = headers;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    size_t pos = lower.find("sec-websocket-key:");
    if (pos == std::string::npos)
        return "";

    // Find the value (after colon, before CRLF)
    size_t start = headers.find(':', pos);
    if (start == std::string::npos)
        return "";
    ++start;

    // Skip whitespace
    while (start < headers.size() &&
           (headers[start] == ' ' || headers[start] == '\t'))
        ++start;

    // Find end of line
    size_t end = headers.find("\r\n", start);
    if (end == std::string::npos)
        end = headers.find('\n', start);
    if (end == std::string::npos)
        end = headers.size();

    // Trim trailing whitespace
    while (end > start &&
           (headers[end - 1] == ' ' || headers[end - 1] == '\t'))
        --end;

    return headers.substr(start, end - start);
}

std::string WebSocketHandler::computeAcceptKey(const std::string& secWebSocketKey)
{
    // Concatenate key with GUID
    std::string input = secWebSocketKey + WS_GUID;

    // SHA-1 hash
    std::vector<uint8_t> hash = sha1(input);

    // Base64 encode
    return base64Encode(hash);
}

std::string WebSocketHandler::upgradeResponse(const std::string& secWebSocketKey)
{
    std::string acceptKey = computeAcceptKey(secWebSocketKey);

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << acceptKey << "\r\n"
             << "\r\n";

    return response.str();
}

// ==========================================================================
// Frame encoding
// ==========================================================================

std::vector<uint8_t> WebSocketHandler::encodeFrame(const std::string& payload,
                                                   Opcode op,
                                                   bool mask)
{
    std::vector<uint8_t> frame;
    size_t payloadLen = payload.size();

    // First byte: FIN bit (1) + opcode
    frame.push_back(0x80 | static_cast<uint8_t>(op));

    // Second byte: MASK bit + payload length
    uint8_t maskBit = mask ? 0x80 : 0x00;

    if (payloadLen < 126) {
        frame.push_back(maskBit | static_cast<uint8_t>(payloadLen));
    } else if (payloadLen <= 0xFFFF) {
        frame.push_back(maskBit | 126);
        frame.push_back(static_cast<uint8_t>((payloadLen >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(payloadLen & 0xFF));
    } else {
        frame.push_back(maskBit | 127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((payloadLen >> (i * 8)) & 0xFF));
        }
    }

    // Generate mask key if masking
    std::array<uint8_t, 4> maskKey = {0, 0, 0, 0};
    if (mask) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis(0, 255);
        for (int i = 0; i < 4; ++i) {
            maskKey[static_cast<size_t>(i)] = static_cast<uint8_t>(dis(gen));
        }
        frame.insert(frame.end(), maskKey.begin(), maskKey.end());
    }

    // Append payload (masked if required)
    size_t payloadStart = frame.size();
    frame.insert(frame.end(), payload.begin(), payload.end());

    if (mask) {
        applyMask(frame, maskKey.data(), payloadStart);
    }

    return frame;
}

// ==========================================================================
// Frame decoding
// ==========================================================================

std::string WebSocketHandler::decodeFrame(const std::vector<uint8_t>& frame,
                                          Opcode& outOp)
{
    std::string payload;
    decodeFrameFromBuffer(frame.data(), frame.size(), payload, outOp);
    return payload;
}

size_t WebSocketHandler::decodeFrameFromBuffer(const uint8_t* data, size_t len,
                                               std::string& outPayload,
                                               Opcode& outOp)
{
    outPayload.clear();
    outOp = TEXT;

    if (len < 2)
        return 0;  // Incomplete frame

    // First byte: FIN + opcode
    // bool fin = (data[0] & 0x80) != 0;  // Not used currently
    outOp = static_cast<Opcode>(data[0] & 0x0F);

    // Second byte: MASK + payload length
    bool masked = (data[1] & 0x80) != 0;
    size_t payloadLen = data[1] & 0x7F;
    size_t headerLen = 2;

    if (payloadLen == 126) {
        if (len < 4)
            return 0;
        payloadLen = (static_cast<size_t>(data[2]) << 8) |
                     static_cast<size_t>(data[3]);
        headerLen = 4;
    } else if (payloadLen == 127) {
        if (len < 10)
            return 0;
        payloadLen = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLen = (payloadLen << 8) | data[2 + static_cast<size_t>(i)];
        }
        headerLen = 10;
    }

    // Mask key (if present)
    std::array<uint8_t, 4> maskKey = {0, 0, 0, 0};
    if (masked) {
        if (len < headerLen + 4)
            return 0;
        for (int i = 0; i < 4; ++i) {
            maskKey[static_cast<size_t>(i)] = data[headerLen + static_cast<size_t>(i)];
        }
        headerLen += 4;
    }

    // Check if we have the full payload
    if (len < headerLen + payloadLen)
        return 0;

    // Extract and unmask payload
    outPayload.resize(payloadLen);
    for (size_t i = 0; i < payloadLen; ++i) {
        uint8_t byte = data[headerLen + i];
        if (masked) {
            byte ^= maskKey[i % 4];
        }
        outPayload[i] = static_cast<char>(byte);
    }

    return headerLen + payloadLen;
}

// ==========================================================================
// Control frames
// ==========================================================================

std::vector<uint8_t> WebSocketHandler::pingFrame(const std::string& payload)
{
    return encodeFrame(payload, PING, false);
}

std::vector<uint8_t> WebSocketHandler::pongFrame(const std::string& payload)
{
    return encodeFrame(payload, PONG, false);
}

std::vector<uint8_t> WebSocketHandler::closeFrame(uint16_t statusCode,
                                                   const std::string& reason)
{
    std::string payload;
    payload.push_back(static_cast<char>((statusCode >> 8) & 0xFF));
    payload.push_back(static_cast<char>(statusCode & 0xFF));
    payload += reason;
    return encodeFrame(payload, CLOSE, false);
}

// ==========================================================================
// SHA-1 implementation (RFC 3174)
// ==========================================================================

std::vector<uint8_t> WebSocketHandler::sha1(const std::string& input)
{
    // SHA-1 constants
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    // Pre-processing: adding padding bits
    std::vector<uint8_t> message(input.begin(), input.end());
    uint64_t originalBits = message.size() * 8;

    // Append bit '1' (as 0x80 byte)
    message.push_back(0x80);

    // Append zeros until message length ≡ 448 (mod 512)
    while ((message.size() % 64) != 56) {
        message.push_back(0x00);
    }

    // Append original length in bits as 64-bit big-endian
    for (int i = 7; i >= 0; --i) {
        message.push_back(static_cast<uint8_t>((originalBits >> (i * 8)) & 0xFF));
    }

    // Process each 512-bit chunk
    auto leftRotate = [](uint32_t x, int n) -> uint32_t {
        return (x << n) | (x >> (32 - n));
    };

    for (size_t chunk = 0; chunk < message.size(); chunk += 64) {
        // Break chunk into sixteen 32-bit big-endian words
        std::array<uint32_t, 80> w{};
        for (int i = 0; i < 16; ++i) {
            size_t base = chunk + static_cast<size_t>(i) * 4;
            w[static_cast<size_t>(i)] =
                (static_cast<uint32_t>(message[base])     << 24) |
                (static_cast<uint32_t>(message[base + 1]) << 16) |
                (static_cast<uint32_t>(message[base + 2]) << 8)  |
                static_cast<uint32_t>(message[base + 3]);
        }

        // Extend the sixteen 32-bit words into eighty 32-bit words
        for (int i = 16; i < 80; ++i) {
            w[static_cast<size_t>(i)] = leftRotate(
                w[static_cast<size_t>(i - 3)] ^
                w[static_cast<size_t>(i - 8)] ^
                w[static_cast<size_t>(i - 14)] ^
                w[static_cast<size_t>(i - 16)], 1);
        }

        // Initialize hash values for this chunk
        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;
        uint32_t e = h4;

        // Main loop
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = leftRotate(a, 5) + f + e + k + w[static_cast<size_t>(i)];
            e = d;
            d = c;
            c = leftRotate(b, 30);
            b = a;
            a = temp;
        }

        // Add this chunk's hash to result
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    // Produce the final hash value (big-endian)
    std::vector<uint8_t> digest(20);
    for (int i = 0; i < 4; ++i) {
        digest[static_cast<size_t>(i)]      = static_cast<uint8_t>((h0 >> (24 - i * 8)) & 0xFF);
        digest[static_cast<size_t>(i) + 4]  = static_cast<uint8_t>((h1 >> (24 - i * 8)) & 0xFF);
        digest[static_cast<size_t>(i) + 8]  = static_cast<uint8_t>((h2 >> (24 - i * 8)) & 0xFF);
        digest[static_cast<size_t>(i) + 12] = static_cast<uint8_t>((h3 >> (24 - i * 8)) & 0xFF);
        digest[static_cast<size_t>(i) + 16] = static_cast<uint8_t>((h4 >> (24 - i * 8)) & 0xFF);
    }

    return digest;
}

// ==========================================================================
// Base64 encoding
// ==========================================================================

std::string WebSocketHandler::base64Encode(const std::vector<uint8_t>& data)
{
    std::string encoded;
    encoded.reserve(((data.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i < data.size()) {
        uint32_t octet_a = i < data.size() ? data[i++] : 0;
        uint32_t octet_b = i < data.size() ? data[i++] : 0;
        uint32_t octet_c = i < data.size() ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        encoded.push_back(BASE64_CHARS[(triple >> 18) & 0x3F]);
        encoded.push_back(BASE64_CHARS[(triple >> 12) & 0x3F]);
        encoded.push_back(BASE64_CHARS[(triple >> 6) & 0x3F]);
        encoded.push_back(BASE64_CHARS[triple & 0x3F]);
    }

    // Add padding
    size_t mod = data.size() % 3;
    if (mod == 1) {
        encoded[encoded.size() - 2] = '=';
        encoded[encoded.size() - 1] = '=';
    } else if (mod == 2) {
        encoded[encoded.size() - 1] = '=';
    }

    return encoded;
}

// ==========================================================================
// Masking
// ==========================================================================

void WebSocketHandler::applyMask(std::vector<uint8_t>& data,
                                 const uint8_t maskKey[4],
                                 size_t offset)
{
    for (size_t i = offset; i < data.size(); ++i) {
        data[i] ^= maskKey[(i - offset) % 4];
    }
}

} // namespace cog0
