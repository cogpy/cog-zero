/*
 * standalone/include/cog0/RaftConsensus.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Raft-based distributed consensus for the standalone cog0 agent.
 *
 * Implements the core Raft leader-election and log-replication protocol
 * (Ongaro & Ousterhout, 2014) using only C++17 standard library primitives.
 *
 * Design notes
 * ─────────────
 * • In-process simulation: "nodes" are RaftPeer objects that communicate
 *   through a shared RaftCluster message bus (no real sockets).  This
 *   lets the full protocol run deterministically inside a test suite while
 *   remaining a faithful implementation of the Raft state machine.
 *
 * • Each RaftNode runs its own background thread (election timer + message
 *   dispatch loop).
 *
 * • The public API is intentionally minimal so that it can be adopted in
 *   the production distributed layer (agentzero-distributed) without
 *   change; only the transport layer needs to be swapped out.
 *
 * Public types
 * ─────────────
 *   RaftRole      — FOLLOWER | CANDIDATE | LEADER
 *   LogEntry      — {term, index, command}
 *   RaftNodeConfig — per-node configuration (id, peer ids, timeouts)
 *   RaftCluster   — in-process message bus shared across all nodes
 *   RaftNode      — single Raft node (state machine + timer + I/O)
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace cog0 {

// =========================================================================
// Raft protocol roles
// =========================================================================

enum class RaftRole { FOLLOWER, CANDIDATE, LEADER };

std::string raftRoleName(RaftRole r);

// =========================================================================
// Log entry
// =========================================================================

struct LogEntry {
    uint64_t    term    = 0;
    uint64_t    index   = 0;
    std::string command;        // opaque client command string
};

// =========================================================================
// RPC message types
// =========================================================================

struct RequestVoteRPC {
    uint64_t    term;
    std::string candidateId;
    uint64_t    lastLogIndex;
    uint64_t    lastLogTerm;
};

struct RequestVoteReply {
    uint64_t term;
    bool     voteGranted;
    std::string voterId;
};

struct AppendEntriesRPC {
    uint64_t               term;
    std::string            leaderId;
    uint64_t               prevLogIndex;
    uint64_t               prevLogTerm;
    std::vector<LogEntry>  entries;
    uint64_t               leaderCommit;
};

struct AppendEntriesReply {
    uint64_t    term;
    bool        success;
    std::string followerId;
    uint64_t    matchIndex;
};

// =========================================================================
// RaftNodeConfig
// =========================================================================

struct RaftNodeConfig {
    std::string              nodeId;
    std::vector<std::string> peers;

    // Election timeout range in milliseconds (randomized within range)
    std::chrono::milliseconds electionTimeoutMin{150};
    std::chrono::milliseconds electionTimeoutMax{300};

    // Heartbeat interval (leader → followers)
    std::chrono::milliseconds heartbeatInterval{50};
};

// =========================================================================
// RaftCluster — in-process message bus
// =========================================================================

/// Shared message bus.  All nodes call deliver*() to route messages to
/// their peers without real network I/O.
class RaftCluster {
public:
    // Register a node callback for incoming RequestVote RPCs
    void registerVoteHandler(
        const std::string& nodeId,
        std::function<RequestVoteReply(const RequestVoteRPC&)> handler);

    // Register a node callback for incoming AppendEntries RPCs
    void registerAppendHandler(
        const std::string& nodeId,
        std::function<AppendEntriesReply(const AppendEntriesRPC&)> handler);

    // Unregister a node (called on node shutdown)
    void unregister(const std::string& nodeId);

    // Send RequestVote to a target node; returns reply (or nullopt if offline)
    std::optional<RequestVoteReply>    sendVote(const std::string& target,
                                                const RequestVoteRPC& rpc);

    // Send AppendEntries to a target node; returns reply (or nullopt if offline)
    std::optional<AppendEntriesReply>  sendAppend(const std::string& target,
                                                   const AppendEntriesRPC& rpc);

    // Simulate partition: isolate a node (it will not receive messages)
    void partition(const std::string& nodeId);
    void heal(const std::string& nodeId);

private:
    mutable std::mutex _mutex;

    std::unordered_map<std::string,
        std::function<RequestVoteReply(const RequestVoteRPC&)>> _voteHandlers;
    std::unordered_map<std::string,
        std::function<AppendEntriesReply(const AppendEntriesRPC&)>> _appendHandlers;

    std::unordered_map<std::string, bool> _partitioned;  // true → isolated
};

// =========================================================================
// RaftNode
// =========================================================================

class RaftNode {
public:
    explicit RaftNode(RaftNodeConfig cfg,
                      std::shared_ptr<RaftCluster> cluster);
    ~RaftNode();

    // Start/stop background thread
    void start();
    void stop();

    // ----------------------------------------------------------------
    // State queries

    [[nodiscard]] RaftRole    role()        const;
    [[nodiscard]] uint64_t    currentTerm() const;
    [[nodiscard]] std::string nodeId()      const { return _cfg.nodeId; }
    [[nodiscard]] std::string leaderId()    const;
    [[nodiscard]] bool        isLeader()    const { return role() == RaftRole::LEADER; }

    /// Index of last committed log entry.
    [[nodiscard]] uint64_t commitIndex() const;

    /// Number of entries in the log.
    [[nodiscard]] size_t logSize() const;

    // ----------------------------------------------------------------
    // Client API

    /// Submit a command (only meaningful on leader; returns false if not leader).
    bool submitCommand(const std::string& command);

    /// Read the log entries committed so far.
    std::vector<LogEntry> committedLog() const;

    // ----------------------------------------------------------------
    // RPC handlers (called by RaftCluster dispatch)

    RequestVoteReply    handleRequestVote(const RequestVoteRPC& rpc);
    AppendEntriesReply  handleAppendEntries(const AppendEntriesRPC& rpc);

private:
    // ---- Core state machine ----
    void     _runLoop();
    void     _runElection();
    void     _sendHeartbeats();
    void     _becomeFollower(uint64_t term, const std::string& leaderId = "");
    void     _becomeCandidate();
    void     _becomeLeader();
    void     _resetElectionTimer();
    bool     _electionTimeoutExpired() const;
    uint64_t _lastLogIndex() const;
    uint64_t _lastLogTerm()  const;

    // Append-entries helpers
    void _advanceCommitIndex();

    // ---- Configuration ----
    RaftNodeConfig               _cfg;
    std::shared_ptr<RaftCluster> _cluster;

    // ---- Persistent state (protected by _mu) ----
    mutable std::mutex _mu;
    uint64_t           _currentTerm  = 0;
    std::string        _votedFor;       // empty = none
    std::vector<LogEntry> _log;         // index 0 = sentinel (term=0, index=0)

    // ---- Volatile state ----
    RaftRole    _role     = RaftRole::FOLLOWER;
    std::string _leaderId;
    uint64_t    _commitIndex = 0;
    uint64_t    _lastApplied = 0;

    // ---- Leader volatile state ----
    std::unordered_map<std::string, uint64_t> _nextIndex;
    std::unordered_map<std::string, uint64_t> _matchIndex;

    // ---- Election timer ----
    std::chrono::steady_clock::time_point _electionDeadline;
    std::mt19937 _rng;

    // ---- Background thread ----
    std::thread             _thread;
    std::atomic<bool>       _running{false};
    std::condition_variable _cv;
    std::mutex              _cvMu;
};

} // namespace cog0
