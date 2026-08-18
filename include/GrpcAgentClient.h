/*
 * include/GrpcAgentClient.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Phase 14 Feature 2.2 — AgentService client (JSON-over-TCP fallback for gRPC).
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "GrpcAgentServer.h"  // GrpcCycleStatus, GrpcAgentStatus

namespace cog0 {

class GrpcAgentClient {
public:
    /// Connect to host:port.  Does not throw; check connected().
    GrpcAgentClient(const std::string& host, uint16_t port);
    ~GrpcAgentClient();

    GrpcAgentClient(const GrpcAgentClient&) = delete;
    GrpcAgentClient& operator=(const GrpcAgentClient&) = delete;

    [[nodiscard]] bool connected() const { return _fd >= 0; }
    [[nodiscard]] const std::string& lastError() const { return _lastError; }

    /// Close the underlying socket.
    void close();

    // ---- RPCs (return false on transport/protocol error) ----

    bool setGoal(const std::string& name,
                 const std::string& description,
                 double priority,
                 std::string& outGoalId,
                 std::string& outMessage);

    bool injectPercept(const std::string& source,
                       const std::string& content,
                       double salience,
                       std::string& outMessage);

    bool runCycles(uint32_t cycles, std::vector<GrpcCycleStatus>& outStatuses);

    bool getStatus(GrpcAgentStatus& out);

    bool queryAtoms(const std::string& namePrefix,
                    const std::string& typeFilter,
                    uint32_t limit,
                    std::string& outAtomListJson);

private:
    bool _call(const std::string& method,
               const std::string& paramsJson,
               std::string& outResponseJson);
    static bool _sendFrame(int fd, const std::string& payload);
    static bool _recvFrame(int fd, std::string& out);

    int _fd = -1;
    std::string _lastError;
};

} // namespace cog0
