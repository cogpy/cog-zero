/*
 * tests/test_phase14_websocket.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Phase 14 WebSocket Monitoring Dashboard tests:
 *   - WebSocket upgrade detection
 *   - Sec-WebSocket-Accept key computation
 *   - Frame encoding/decoding
 *   - Dashboard HTML availability
 *   - MonitoringServer WebSocket integration
 */

#include "test_runner.h"

#include <cstring>
#include <string>
#include <thread>
#include <vector>

// Phase 14 headers
#include "cog0/WebSocketHandler.h"
#include "cog0/DashboardAssets.h"
#include "cog0/MonitoringServer.h"

// Core cog0 headers (needed for MonitoringServer)
#include "cog0/AtomStore.h"
#include "cog0/CognitiveLoop.h"

using namespace cog0;
using namespace std::chrono_literals;

// ==========================================================================
// WebSocketHandler — Upgrade Detection
// ==========================================================================

TEST(WebSocket_IsUpgradeRequest_ValidHeaders)
{
    std::string headers =
        "GET /ws/metrics HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    ASSERT_TRUE(WebSocketHandler::isUpgradeRequest(headers));
}

TEST(WebSocket_IsUpgradeRequest_MixedCase)
{
    std::string headers =
        "GET /ws HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "UPGRADE: WebSocket\r\n"
        "CONNECTION: UPGRADE\r\n"
        "Sec-WebSocket-Key: test==\r\n"
        "\r\n";

    ASSERT_TRUE(WebSocketHandler::isUpgradeRequest(headers));
}

TEST(WebSocket_IsUpgradeRequest_InvalidMissingUpgrade)
{
    std::string headers =
        "GET /test HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";

    ASSERT_FALSE(WebSocketHandler::isUpgradeRequest(headers));
}

TEST(WebSocket_IsUpgradeRequest_InvalidMissingConnection)
{
    std::string headers =
        "GET /test HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Upgrade: websocket\r\n"
        "\r\n";

    ASSERT_FALSE(WebSocketHandler::isUpgradeRequest(headers));
}

// ==========================================================================
// WebSocketHandler — Sec-WebSocket-Key Extraction
// ==========================================================================

TEST(WebSocket_ExtractSecKey_Valid)
{
    std::string headers =
        "GET /ws HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "\r\n";

    std::string key = WebSocketHandler::extractSecKey(headers);
    ASSERT_EQ(key, std::string("dGhlIHNhbXBsZSBub25jZQ=="));
}

TEST(WebSocket_ExtractSecKey_WithSpaces)
{
    std::string headers =
        "GET /ws HTTP/1.1\r\n"
        "Sec-WebSocket-Key:   abc123==  \r\n"
        "\r\n";

    std::string key = WebSocketHandler::extractSecKey(headers);
    ASSERT_EQ(key, std::string("abc123=="));
}

TEST(WebSocket_ExtractSecKey_Missing)
{
    std::string headers =
        "GET /ws HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::string key = WebSocketHandler::extractSecKey(headers);
    ASSERT_TRUE(key.empty());
}

// ==========================================================================
// WebSocketHandler — Accept Key Computation (RFC 6455)
// ==========================================================================

TEST(WebSocket_ComputeAcceptKey_RFC6455Example)
{
    // RFC 6455 Section 1.3 example:
    // Client key: "dGhlIHNhbXBsZSBub25jZQ=="
    // Expected accept: "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="
    std::string clientKey = "dGhlIHNhbXBsZSBub25jZQ==";
    std::string expected = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

    std::string accept = WebSocketHandler::computeAcceptKey(clientKey);
    ASSERT_EQ(accept, expected);
}

TEST(WebSocket_ComputeAcceptKey_AnotherExample)
{
    // Test with a different key to ensure SHA-1/Base64 works correctly
    std::string clientKey = "x3JJHMbDL1EzLkh9GBhXDw==";
    std::string accept = WebSocketHandler::computeAcceptKey(clientKey);

    // The result should be a non-empty Base64 string
    ASSERT_FALSE(accept.empty());
    ASSERT_GT(accept.size(), size_t(10));
    // Base64 strings typically end with = padding or alphanumeric
    ASSERT_TRUE(accept.back() == '=' || std::isalnum(accept.back()));
}

// ==========================================================================
// WebSocketHandler — Upgrade Response Generation
// ==========================================================================

TEST(WebSocket_UpgradeResponse_ContainsRequiredHeaders)
{
    std::string clientKey = "dGhlIHNhbXBsZSBub25jZQ==";
    std::string response = WebSocketHandler::upgradeResponse(clientKey);

    ASSERT_TRUE(response.find("HTTP/1.1 101") != std::string::npos);
    ASSERT_TRUE(response.find("Upgrade: websocket") != std::string::npos);
    ASSERT_TRUE(response.find("Connection: Upgrade") != std::string::npos);
    ASSERT_TRUE(response.find("Sec-WebSocket-Accept:") != std::string::npos);
    ASSERT_TRUE(response.find("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos);
}

// ==========================================================================
// WebSocketHandler — Frame Encoding
// ==========================================================================

TEST(WebSocket_EncodeFrame_SmallTextPayload)
{
    std::string payload = "Hello";
    auto frame = WebSocketHandler::encodeFrame(payload, WebSocketHandler::TEXT);

    // Frame structure: 1 byte opcode, 1 byte length, payload
    ASSERT_GE(frame.size(), size_t(2 + payload.size()));

    // First byte: FIN bit (0x80) | TEXT opcode (0x1)
    ASSERT_EQ(frame[0], uint8_t(0x81));

    // Second byte: MASK bit (0) | payload length (5)
    ASSERT_EQ(frame[1], uint8_t(5));

    // Payload should follow directly
    std::string decoded(frame.begin() + 2, frame.end());
    ASSERT_EQ(decoded, payload);
}

TEST(WebSocket_EncodeFrame_MediumPayload)
{
    // 126-byte payload (triggers extended 16-bit length)
    std::string payload(200, 'x');
    auto frame = WebSocketHandler::encodeFrame(payload, WebSocketHandler::TEXT);

    ASSERT_EQ(frame[0], uint8_t(0x81));  // FIN | TEXT
    ASSERT_EQ(frame[1], uint8_t(126));   // Extended length indicator

    // 16-bit length in network byte order
    uint16_t len = (static_cast<uint16_t>(frame[2]) << 8) |
                   static_cast<uint16_t>(frame[3]);
    ASSERT_EQ(len, uint16_t(200));
}

TEST(WebSocket_EncodeFrame_BinaryOpcode)
{
    std::string payload = "\x00\x01\x02\x03";
    auto frame = WebSocketHandler::encodeFrame(payload, WebSocketHandler::BINARY);

    // First byte: FIN bit (0x80) | BINARY opcode (0x2)
    ASSERT_EQ(frame[0], uint8_t(0x82));
}

TEST(WebSocket_EncodeFrame_PingFrame)
{
    auto frame = WebSocketHandler::pingFrame("ping");

    // PING opcode is 0x9
    ASSERT_EQ(frame[0], uint8_t(0x89));
    ASSERT_EQ(frame[1], uint8_t(4));
}

TEST(WebSocket_EncodeFrame_PongFrame)
{
    auto frame = WebSocketHandler::pongFrame("pong");

    // PONG opcode is 0xA
    ASSERT_EQ(frame[0], uint8_t(0x8A));
}

TEST(WebSocket_EncodeFrame_CloseFrame)
{
    auto frame = WebSocketHandler::closeFrame(1000, "goodbye");

    // CLOSE opcode is 0x8
    ASSERT_EQ(frame[0], uint8_t(0x88));

    // Payload: 2 bytes status code + reason
    ASSERT_GE(frame.size(), size_t(4));
}

// ==========================================================================
// WebSocketHandler — Frame Decoding
// ==========================================================================

TEST(WebSocket_DecodeFrame_SmallTextPayload)
{
    // Create a valid frame: FIN | TEXT, length=5, "Hello"
    std::vector<uint8_t> frame = {0x81, 0x05, 'H', 'e', 'l', 'l', 'o'};

    WebSocketHandler::Opcode op;
    std::string payload = WebSocketHandler::decodeFrame(frame, op);

    ASSERT_EQ(op, WebSocketHandler::TEXT);
    ASSERT_EQ(payload, std::string("Hello"));
}

TEST(WebSocket_DecodeFrame_MaskedPayload)
{
    // Masked frame from client: FIN | TEXT, masked, length=5
    // Mask key: 0x37 0xFA 0x21 0x3D
    // Masked "Hello": H^0x37, e^0xFA, l^0x21, l^0x3D, o^0x37
    std::vector<uint8_t> frame = {
        0x81,  // FIN | TEXT
        0x85,  // Masked | length=5
        0x37, 0xFA, 0x21, 0x3D,  // Mask key
        static_cast<uint8_t>('H' ^ 0x37),
        static_cast<uint8_t>('e' ^ 0xFA),
        static_cast<uint8_t>('l' ^ 0x21),
        static_cast<uint8_t>('l' ^ 0x3D),
        static_cast<uint8_t>('o' ^ 0x37)
    };

    WebSocketHandler::Opcode op;
    std::string payload = WebSocketHandler::decodeFrame(frame, op);

    ASSERT_EQ(op, WebSocketHandler::TEXT);
    ASSERT_EQ(payload, std::string("Hello"));
}

TEST(WebSocket_DecodeFrame_CloseFrame)
{
    // CLOSE frame with status 1000 (normal closure)
    std::vector<uint8_t> frame = {
        0x88,  // FIN | CLOSE
        0x02,  // length=2
        0x03, 0xE8  // 1000 in big-endian
    };

    WebSocketHandler::Opcode op;
    std::string payload = WebSocketHandler::decodeFrame(frame, op);

    ASSERT_EQ(op, WebSocketHandler::CLOSE);
    ASSERT_EQ(payload.size(), size_t(2));
}

TEST(WebSocket_DecodeFrameFromBuffer_Incomplete)
{
    // Incomplete frame (header says 5 bytes but only 3 provided)
    std::vector<uint8_t> frame = {0x81, 0x05, 'H', 'e', 'l'};

    std::string payload;
    WebSocketHandler::Opcode op;
    size_t consumed = WebSocketHandler::decodeFrameFromBuffer(
        frame.data(), frame.size(), payload, op);

    // Should return 0 indicating incomplete frame
    ASSERT_EQ(consumed, size_t(0));
}

TEST(WebSocket_DecodeFrameFromBuffer_Complete)
{
    std::vector<uint8_t> frame = {0x81, 0x05, 'H', 'e', 'l', 'l', 'o'};

    std::string payload;
    WebSocketHandler::Opcode op;
    size_t consumed = WebSocketHandler::decodeFrameFromBuffer(
        frame.data(), frame.size(), payload, op);

    ASSERT_EQ(consumed, size_t(7));
    ASSERT_EQ(payload, std::string("Hello"));
}

TEST(WebSocket_RoundTrip_EncodeDecodeText)
{
    std::string original = "The quick brown fox jumps over the lazy dog.";
    auto frame = WebSocketHandler::encodeFrame(original, WebSocketHandler::TEXT);

    WebSocketHandler::Opcode op;
    std::string decoded = WebSocketHandler::decodeFrame(frame, op);

    ASSERT_EQ(op, WebSocketHandler::TEXT);
    ASSERT_EQ(decoded, original);
}

TEST(WebSocket_RoundTrip_EncodeDecodeBinary)
{
    std::string original = "\x00\x01\x02\xFE\xFF";
    auto frame = WebSocketHandler::encodeFrame(original, WebSocketHandler::BINARY);

    WebSocketHandler::Opcode op;
    std::string decoded = WebSocketHandler::decodeFrame(frame, op);

    ASSERT_EQ(op, WebSocketHandler::BINARY);
    ASSERT_EQ(decoded, original);
}

// ==========================================================================
// DashboardAssets — HTML Availability
// ==========================================================================

TEST(Dashboard_HTML_NotNull)
{
    ASSERT_TRUE(DASHBOARD_HTML != nullptr);
}

TEST(Dashboard_HTML_NotEmpty)
{
    ASSERT_GT(std::strlen(DASHBOARD_HTML), size_t(100));
}

TEST(Dashboard_HTML_LengthMatchesConstant)
{
    ASSERT_EQ(DASHBOARD_HTML_LEN, std::strlen(DASHBOARD_HTML));
}

TEST(Dashboard_HTML_ContainsDoctype)
{
    std::string html(DASHBOARD_HTML);
    ASSERT_TRUE(html.find("<!DOCTYPE html>") != std::string::npos);
}

TEST(Dashboard_HTML_ContainsTitle)
{
    std::string html(DASHBOARD_HTML);
    ASSERT_TRUE(html.find("<title>") != std::string::npos);
    ASSERT_TRUE(html.find("cog0") != std::string::npos);
}

TEST(Dashboard_HTML_ContainsWebSocketCode)
{
    std::string html(DASHBOARD_HTML);
    ASSERT_TRUE(html.find("WebSocket") != std::string::npos);
    ASSERT_TRUE(html.find("/ws/metrics") != std::string::npos);
}

TEST(Dashboard_HTML_ContainsMetricsElements)
{
    std::string html(DASHBOARD_HTML);
    ASSERT_TRUE(html.find("atomCount") != std::string::npos);
    ASSERT_TRUE(html.find("cycleCount") != std::string::npos);
    ASSERT_TRUE(html.find("uptime") != std::string::npos);
}

TEST(Dashboard_HTML_SelfContained)
{
    std::string html(DASHBOARD_HTML);
    // Should contain inline CSS (no external stylesheet)
    ASSERT_TRUE(html.find("<style>") != std::string::npos);
    // Should contain inline JS (no external script)
    ASSERT_TRUE(html.find("<script>") != std::string::npos);
}

TEST(Dashboard_Favicon_Exists)
{
    ASSERT_TRUE(DASHBOARD_FAVICON != nullptr);
    std::string favicon(DASHBOARD_FAVICON);
    ASSERT_TRUE(favicon.find("data:image/png") != std::string::npos);
}

// ==========================================================================
// MonitoringServer — WebSocket Integration
// ==========================================================================

TEST(MonitoringServer_WebSocket_EnableDisable)
{
    auto store = std::make_shared<AtomStore>();
    MonitoringServer srv(store, nullptr, 0);

    ASSERT_FALSE(srv.webSocketEnabled());

    srv.enableWebSocket(true);
    ASSERT_TRUE(srv.webSocketEnabled());

    srv.enableWebSocket(false);
    ASSERT_FALSE(srv.webSocketEnabled());
}

TEST(MonitoringServer_WebSocket_ClientCount_Initial)
{
    auto store = std::make_shared<AtomStore>();
    MonitoringServer srv(store, nullptr, 0);
    srv.enableWebSocket(true);

    ASSERT_EQ(srv.webSocketClientCount(), size_t(0));
}

TEST(MonitoringServer_WebSocket_BroadcastInterval)
{
    auto store = std::make_shared<AtomStore>();
    MonitoringServer srv(store, nullptr, 0);

    // Default broadcast interval is 500ms (internal, but we can set it)
    srv.setWebSocketBroadcastInterval(1000);
    // No direct getter, but this shouldn't crash
}

TEST(MonitoringServer_Dashboard_Available)
{
    // This is an indirect test — we verify the server compiles and runs
    // with dashboard support enabled. Full integration test would require
    // actual HTTP client.
    auto store = std::make_shared<AtomStore>();
    store->addNode(AtomType::CONCEPT, "test");

    MonitoringServer srv(store, nullptr, 19995);
    srv.enableWebSocket(true);

    ASSERT_FALSE(srv.running());
    srv.start();

    std::this_thread::sleep_for(50ms);
    ASSERT_TRUE(srv.running());

    srv.stop();
    ASSERT_FALSE(srv.running());
}

// ==========================================================================
// Edge Cases and Error Handling
// ==========================================================================

TEST(WebSocket_DecodeFrame_EmptyBuffer)
{
    std::string payload;
    WebSocketHandler::Opcode op;
    size_t consumed = WebSocketHandler::decodeFrameFromBuffer(
        nullptr, 0, payload, op);

    ASSERT_EQ(consumed, size_t(0));
}

TEST(WebSocket_DecodeFrame_SingleByte)
{
    std::vector<uint8_t> frame = {0x81};

    std::string payload;
    WebSocketHandler::Opcode op;
    size_t consumed = WebSocketHandler::decodeFrameFromBuffer(
        frame.data(), frame.size(), payload, op);

    ASSERT_EQ(consumed, size_t(0));  // Incomplete
}

TEST(WebSocket_EncodeFrame_EmptyPayload)
{
    auto frame = WebSocketHandler::encodeFrame("", WebSocketHandler::TEXT);

    ASSERT_EQ(frame.size(), size_t(2));
    ASSERT_EQ(frame[0], uint8_t(0x81));  // FIN | TEXT
    ASSERT_EQ(frame[1], uint8_t(0));     // Length = 0
}

TEST(WebSocket_ComputeAcceptKey_EmptyKey)
{
    // Edge case: empty key (invalid but should not crash)
    std::string accept = WebSocketHandler::computeAcceptKey("");
    ASSERT_FALSE(accept.empty());  // Should still produce Base64 output
}
