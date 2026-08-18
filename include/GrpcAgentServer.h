/*
 * include/GrpcAgentServer.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Phase 14 Feature 2.2 — AgentService server.
 *
 * Mirrors the service defined in proto/agent.proto.  When real gRPC is not
 * linked (the default standalone build), a length-prefixed JSON transport is
 * used over TCP.  The wire format is documented in docs/GRPC_GUIDE.md.
 *
 * RPCs:
 *   SetGoal, InjectPercept, RunCycles (streamed), GetStatus, QueryAtoms
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Agent.h"

namespace cog0 {

/// Result of a single streamed cycle status update.
struct GrpcCycleStatus {
    uint64_t cycle = 0;
    uint64_t total = 0;
    std::string phase;
    bool done = false;
    std::string detail;
};

/// Snapshot returned by GetStatus.
struct GrpcAgentStatus {
    std::string name;
    bool running = false;
    uint64_t cycleCount = 0;
    uint64_t atomCount = 0;
    uint64_t pendingTasks = 0;
    std::string report;
};

/// Lightweight AgentService server (JSON-over-TCP fallback for gRPC).
class GrpcAgentServer {
public:
    /// Bind to an existing Agent.  The agent must outlive the server.
    explicit GrpcAgentServer(Agent& agent, uint16_t port = 50051);

    ~GrpcAgentServer();

    GrpcAgentServer(const GrpcAgentServer&) = delete;
    GrpcAgentServer& operator=(const GrpcAgentServer&) = delete;

    /// Start the background accept loop.
    bool start();

    /// Stop the server and join the listener thread.
    void stop();

    [[nodiscard]] bool running() const { return _running.load(); }
    [[nodiscard]] uint16_t port() const { return _port; }

    /// True when built/linked against real gRPC (always false in fallback mode).
    [[nodiscard]] static bool usingRealGrpc();

    /// Direct (in-process) RPC handlers — also used by the network path and tests.
    std::string handleSetGoal(const std::string& name,
                              const std::string& description,
                              double priority);
    std::string handleInjectPercept(const std::string& source,
                                    const std::string& content,
                                    double salience);
    std::vector<GrpcCycleStatus> handleRunCycles(uint32_t cycles);
    GrpcAgentStatus handleGetStatus() const;
    std::string handleQueryAtoms(const std::string& namePrefix,
                                 const std::string& typeFilter,
                                 uint32_t limit);

    /// Dispatch a JSON request object and return a JSON response object body.
    /// Request shape: {"method":"...","params":{...}}
    /// Response shape: {"ok":true/false,"result":{...},"error":"..."}
    std::string dispatchJson(const std::string& requestJson);

private:
    void _listenLoop();
    void _handleClient(int fd);
    static bool _sendFrame(int fd, const std::string& payload);
    static bool _recvFrame(int fd, std::string& out);

    Agent& _agent;
    uint16_t _port;
    std::atomic<bool> _running{false};
    int _serverFd = -1;
    std::thread _thread;
    mutable std::mutex _mutex;
};

} // namespace cog0
