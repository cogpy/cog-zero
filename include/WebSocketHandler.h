/*
 * standalone/include/cog0/WebSocketHandler.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * WebSocket protocol handler for real-time metrics push.
 *
 * Implements RFC 6455 WebSocket frame encoding/decoding and handshake
 * processing. Used by MonitoringServer to provide live metrics streaming
 * to dashboard clients.
 *
 * Features:
 *   - Upgrade request detection and response generation
 *   - Frame encoding/decoding for TEXT, BINARY, PING/PONG, CLOSE
 *   - SHA-1 and Base64 for Sec-WebSocket-Accept computation
 *
 * Zero external dependencies — pure C++17 implementation.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cog0 {

// ==========================================================================
// WebSocketHandler — RFC 6455 protocol utilities
// ==========================================================================

class WebSocketHandler {
public:
    // WebSocket frame opcodes (RFC 6455 Section 5.2)
    enum Opcode : uint8_t {
        CONTINUATION = 0x0,
        TEXT         = 0x1,
        BINARY       = 0x2,
        CLOSE        = 0x8,
        PING         = 0x9,
        PONG         = 0xA
    };

    // ----------------------------------------------------------------
    // Handshake utilities

    /// Check if HTTP headers indicate a WebSocket upgrade request.
    /// Looks for "Upgrade: websocket" and "Connection: Upgrade" headers.
    static bool isUpgradeRequest(const std::string& headers);

    /// Extract the Sec-WebSocket-Key header value from request headers.
    static std::string extractSecKey(const std::string& headers);

    /// Compute the Sec-WebSocket-Accept value per RFC 6455 Section 4.2.2.
    /// Returns base64(SHA1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))
    static std::string computeAcceptKey(const std::string& secWebSocketKey);

    /// Generate the HTTP 101 Switching Protocols response for WebSocket upgrade.
    static std::string upgradeResponse(const std::string& secWebSocketKey);

    // ----------------------------------------------------------------
    // Frame encoding/decoding

    /// Encode a payload into a WebSocket frame.
    /// @param payload  Data to send
    /// @param op       Opcode (default: TEXT)
    /// @param mask     Whether to mask payload (client -> server should mask)
    /// @return Complete WebSocket frame ready to send
    static std::vector<uint8_t> encodeFrame(const std::string& payload,
                                            Opcode op = TEXT,
                                            bool mask = false);

    /// Decode a WebSocket frame.
    /// @param frame   Raw frame data received from socket
    /// @param outOp   Output: opcode of the frame
    /// @return Decoded payload string (unmasked)
    static std::string decodeFrame(const std::vector<uint8_t>& frame,
                                   Opcode& outOp);

    /// Decode a frame from a buffer, returning bytes consumed.
    /// @param data    Buffer with potential frame data
    /// @param len     Length of buffer
    /// @param outPayload  Output: decoded payload
    /// @param outOp   Output: frame opcode
    /// @return Number of bytes consumed (0 if incomplete frame)
    static size_t decodeFrameFromBuffer(const uint8_t* data, size_t len,
                                        std::string& outPayload,
                                        Opcode& outOp);

    // ----------------------------------------------------------------
    // Control frames

    /// Create a PING frame with optional payload.
    static std::vector<uint8_t> pingFrame(const std::string& payload = "");

    /// Create a PONG frame (reply to PING).
    static std::vector<uint8_t> pongFrame(const std::string& payload = "");

    /// Create a CLOSE frame with optional status code and reason.
    static std::vector<uint8_t> closeFrame(uint16_t statusCode = 1000,
                                           const std::string& reason = "");

private:
    // ----------------------------------------------------------------
    // Cryptographic utilities (minimal implementations)

    /// SHA-1 hash (RFC 3174).
    /// @param input  Data to hash
    /// @return 20-byte SHA-1 digest
    static std::vector<uint8_t> sha1(const std::string& input);

    /// Base64 encode.
    /// @param data  Raw bytes to encode
    /// @return Base64-encoded string
    static std::string base64Encode(const std::vector<uint8_t>& data);

    /// Apply XOR mask to payload data.
    static void applyMask(std::vector<uint8_t>& data,
                          const uint8_t maskKey[4],
                          size_t offset = 0);
};

} // namespace cog0
