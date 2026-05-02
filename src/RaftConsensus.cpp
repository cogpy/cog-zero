/*
 * standalone/src/RaftConsensus.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Raft leader-election and log-replication — pure C++17, in-process.
 */

#include "cog0/RaftConsensus.h"
#include "cog0/Logger.h"

#include <algorithm>
#include <cassert>
#include <sstream>

namespace cog0 {

// =========================================================================
// Helpers
// =========================================================================

std::string raftRoleName(RaftRole r) {
    switch (r) {
        case RaftRole::FOLLOWER:  return "FOLLOWER";
        case RaftRole::CANDIDATE: return "CANDIDATE";
        case RaftRole::LEADER:    return "LEADER";
    }
    return "UNKNOWN";
}

// =========================================================================
// RaftCluster — in-process message bus
// =========================================================================

void RaftCluster::registerVoteHandler(
    const std::string& nodeId,
    std::function<RequestVoteReply(const RequestVoteRPC&)> handler)
{
    std::lock_guard<std::mutex> lk(_mutex);
    _voteHandlers[nodeId] = std::move(handler);
    _partitioned[nodeId]  = false;
}

void RaftCluster::registerAppendHandler(
    const std::string& nodeId,
    std::function<AppendEntriesReply(const AppendEntriesRPC&)> handler)
{
    std::lock_guard<std::mutex> lk(_mutex);
    _appendHandlers[nodeId] = std::move(handler);
}

void RaftCluster::unregister(const std::string& nodeId)
{
    std::lock_guard<std::mutex> lk(_mutex);
    _voteHandlers.erase(nodeId);
    _appendHandlers.erase(nodeId);
    _partitioned.erase(nodeId);
}

std::optional<RequestVoteReply>
RaftCluster::sendVote(const std::string& target, const RequestVoteRPC& rpc)
{
    std::function<RequestVoteReply(const RequestVoteRPC&)> handler;
    {
        std::lock_guard<std::mutex> lk(_mutex);
        auto pit = _partitioned.find(target);
        if (pit != _partitioned.end() && pit->second) return std::nullopt;
        auto it = _voteHandlers.find(target);
        if (it == _voteHandlers.end()) return std::nullopt;
        handler = it->second;
    }
    // Call outside lock to avoid deadlock
    return handler(rpc);
}

std::optional<AppendEntriesReply>
RaftCluster::sendAppend(const std::string& target, const AppendEntriesRPC& rpc)
{
    std::function<AppendEntriesReply(const AppendEntriesRPC&)> handler;
    {
        std::lock_guard<std::mutex> lk(_mutex);
        auto pit = _partitioned.find(target);
        if (pit != _partitioned.end() && pit->second) return std::nullopt;
        auto it = _appendHandlers.find(target);
        if (it == _appendHandlers.end()) return std::nullopt;
        handler = it->second;
    }
    return handler(rpc);
}

void RaftCluster::partition(const std::string& nodeId)
{
    std::lock_guard<std::mutex> lk(_mutex);
    _partitioned[nodeId] = true;
}

void RaftCluster::heal(const std::string& nodeId)
{
    std::lock_guard<std::mutex> lk(_mutex);
    _partitioned[nodeId] = false;
}

// =========================================================================
// RaftNode
// =========================================================================

RaftNode::RaftNode(RaftNodeConfig cfg, std::shared_ptr<RaftCluster> cluster)
    : _cfg(std::move(cfg))
    , _cluster(std::move(cluster))
    , _rng(std::random_device{}())
{
    // Sentinel entry at index 0 (Raft log is 1-indexed conceptually)
    _log.push_back(LogEntry{0, 0, ""});
}

RaftNode::~RaftNode()
{
    stop();
}

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

void RaftNode::start()
{
    if (_running.exchange(true)) return;

    // Register RPC handlers with the cluster bus
    _cluster->registerVoteHandler(
        _cfg.nodeId,
        [this](const RequestVoteRPC& rpc) { return handleRequestVote(rpc); });

    _cluster->registerAppendHandler(
        _cfg.nodeId,
        [this](const AppendEntriesRPC& rpc) { return handleAppendEntries(rpc); });

    _resetElectionTimer();

    _thread = std::thread([this] { _runLoop(); });

    logger().info("RaftNode " + _cfg.nodeId + " started");
}

void RaftNode::stop()
{
    if (!_running.exchange(false)) return;

    {
        std::lock_guard<std::mutex> lk(_cvMu);
        _cv.notify_all();
    }

    if (_thread.joinable()) _thread.join();

    _cluster->unregister(_cfg.nodeId);

    logger().info("RaftNode " + _cfg.nodeId + " stopped");
}

// -------------------------------------------------------------------------
// State queries
// -------------------------------------------------------------------------

RaftRole RaftNode::role() const
{
    std::lock_guard<std::mutex> lk(_mu);
    return _role;
}

uint64_t RaftNode::currentTerm() const
{
    std::lock_guard<std::mutex> lk(_mu);
    return _currentTerm;
}

std::string RaftNode::leaderId() const
{
    std::lock_guard<std::mutex> lk(_mu);
    return _leaderId;
}

uint64_t RaftNode::commitIndex() const
{
    std::lock_guard<std::mutex> lk(_mu);
    return _commitIndex;
}

size_t RaftNode::logSize() const
{
    std::lock_guard<std::mutex> lk(_mu);
    return _log.size();
}

bool RaftNode::submitCommand(const std::string& command)
{
    std::lock_guard<std::mutex> lk(_mu);
    if (_role != RaftRole::LEADER) return false;

    LogEntry e;
    e.term    = _currentTerm;
    e.index   = static_cast<uint64_t>(_log.size());
    e.command = command;
    _log.push_back(e);

    // Immediately send AppendEntries to replicate
    // (The heartbeat loop will also pick this up)
    return true;
}

std::vector<LogEntry> RaftNode::committedLog() const
{
    std::lock_guard<std::mutex> lk(_mu);
    std::vector<LogEntry> result;
    for (size_t i = 1; i <= _commitIndex && i < _log.size(); ++i)
        result.push_back(_log[i]);
    return result;
}

// -------------------------------------------------------------------------
// RPC handlers
// -------------------------------------------------------------------------

RequestVoteReply RaftNode::handleRequestVote(const RequestVoteRPC& rpc)
{
    std::lock_guard<std::mutex> lk(_mu);

    RequestVoteReply reply;
    reply.voterId    = _cfg.nodeId;
    reply.term       = _currentTerm;
    reply.voteGranted = false;

    // If RPC term > our term, convert to follower
    if (rpc.term > _currentTerm) {
        _currentTerm = rpc.term;
        _role        = RaftRole::FOLLOWER;
        _votedFor.clear();
        _leaderId.clear();
    }

    if (rpc.term < _currentTerm) {
        reply.term = _currentTerm;
        return reply;  // stale request
    }

    bool votedForOk = _votedFor.empty() || _votedFor == rpc.candidateId;
    bool logOk = (rpc.lastLogTerm > _lastLogTerm()) ||
                 (rpc.lastLogTerm == _lastLogTerm() &&
                  rpc.lastLogIndex >= _lastLogIndex());

    if (votedForOk && logOk) {
        _votedFor    = rpc.candidateId;
        reply.voteGranted = true;
        _resetElectionTimer();
    }

    reply.term = _currentTerm;
    return reply;
}

AppendEntriesReply RaftNode::handleAppendEntries(const AppendEntriesRPC& rpc)
{
    std::lock_guard<std::mutex> lk(_mu);

    AppendEntriesReply reply;
    reply.followerId = _cfg.nodeId;
    reply.term       = _currentTerm;
    reply.success    = false;
    reply.matchIndex = 0;

    if (rpc.term > _currentTerm) {
        _currentTerm = rpc.term;
        _role        = RaftRole::FOLLOWER;
        _votedFor.clear();
    }

    if (rpc.term < _currentTerm) {
        reply.term = _currentTerm;
        return reply;  // stale
    }

    // Valid heartbeat / AppendEntries from current leader
    _leaderId = rpc.leaderId;
    _resetElectionTimer();
    if (_role != RaftRole::FOLLOWER) {
        _role = RaftRole::FOLLOWER;
    }

    // Check prevLog consistency
    if (rpc.prevLogIndex >= _log.size() ||
        _log[rpc.prevLogIndex].term != rpc.prevLogTerm) {
        reply.term = _currentTerm;
        return reply;
    }

    // Append new entries (truncate conflicting)
    size_t insertPos = rpc.prevLogIndex + 1;
    for (size_t i = 0; i < rpc.entries.size(); ++i) {
        size_t pos = insertPos + i;
        if (pos < _log.size()) {
            if (_log[pos].term != rpc.entries[i].term) {
                _log.erase(_log.begin() + static_cast<long>(pos), _log.end());
                _log.push_back(rpc.entries[i]);
            }
            // else already present — no-op
        } else {
            _log.push_back(rpc.entries[i]);
        }
    }

    // Advance commit index
    if (rpc.leaderCommit > _commitIndex) {
        _commitIndex = std::min(rpc.leaderCommit,
                                static_cast<uint64_t>(_log.size() - 1));
    }

    reply.success    = true;
    reply.term       = _currentTerm;
    reply.matchIndex = rpc.prevLogIndex + static_cast<uint64_t>(rpc.entries.size());
    return reply;
}

// -------------------------------------------------------------------------
// Background run loop
// -------------------------------------------------------------------------

void RaftNode::_runLoop()
{
    while (_running.load()) {
        {
            std::unique_lock<std::mutex> lk(_cvMu);
            // Wake up either after a short sleep or on explicit notify
            _cv.wait_for(lk, std::chrono::milliseconds(10));
        }

        if (!_running.load()) break;

        RaftRole myRole;
        {
            std::lock_guard<std::mutex> lk(_mu);
            myRole = _role;
        }

        switch (myRole) {
            case RaftRole::FOLLOWER:
            case RaftRole::CANDIDATE:
                if (_electionTimeoutExpired())
                    _runElection();
                break;

            case RaftRole::LEADER:
                _sendHeartbeats();
                break;
        }
    }
}

void RaftNode::_runElection()
{
    _becomeCandidate();

    uint64_t     electionTerm;
    uint64_t     lastIdx;
    uint64_t     lastTerm;
    {
        std::lock_guard<std::mutex> lk(_mu);
        electionTerm = _currentTerm;
        lastIdx      = _lastLogIndex();
        lastTerm     = _lastLogTerm();
    }

    RequestVoteRPC rpc;
    rpc.term         = electionTerm;
    rpc.candidateId  = _cfg.nodeId;
    rpc.lastLogIndex = lastIdx;
    rpc.lastLogTerm  = lastTerm;

    int votes = 1;  // vote for self
    int majority = static_cast<int>(_cfg.peers.size() / 2) + 1;

    for (const auto& peer : _cfg.peers) {
        auto replyOpt = _cluster->sendVote(peer, rpc);
        if (!replyOpt) continue;

        const auto& reply = *replyOpt;

        std::lock_guard<std::mutex> lk(_mu);
        if (reply.term > _currentTerm) {
            _becomeFollower(reply.term);
            return;
        }
        if (_role != RaftRole::CANDIDATE) return;  // lost candidacy
        if (reply.voteGranted) ++votes;
    }

    {
        std::lock_guard<std::mutex> lk(_mu);
        if (_role == RaftRole::CANDIDATE && votes >= majority) {
            _becomeLeader();
        } else if (_role == RaftRole::CANDIDATE) {
            _resetElectionTimer();  // back to follower implicitly via timeout
        }
    }
}

void RaftNode::_sendHeartbeats()
{
    uint64_t     myTerm;
    uint64_t     commitIdx;
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (_role != RaftRole::LEADER) return;
        myTerm    = _currentTerm;
        commitIdx = _commitIndex;
    }

    for (const auto& peer : _cfg.peers) {
        uint64_t nextIdx;
        uint64_t prevLogIndex;
        uint64_t prevLogTerm;
        std::vector<LogEntry> entries;
        {
            std::lock_guard<std::mutex> lk(_mu);
            if (_role != RaftRole::LEADER) return;
            nextIdx      = _nextIndex.count(peer) ? _nextIndex[peer] : 1;
            prevLogIndex = nextIdx > 0 ? nextIdx - 1 : 0;
            prevLogTerm  = prevLogIndex < _log.size()
                               ? _log[prevLogIndex].term : 0;

            // Send any un-replicated entries
            for (size_t i = nextIdx; i < _log.size(); ++i)
                entries.push_back(_log[i]);
        }

        AppendEntriesRPC rpc;
        rpc.term         = myTerm;
        rpc.leaderId     = _cfg.nodeId;
        rpc.prevLogIndex = prevLogIndex;
        rpc.prevLogTerm  = prevLogTerm;
        rpc.entries      = entries;
        rpc.leaderCommit = commitIdx;

        auto replyOpt = _cluster->sendAppend(peer, rpc);
        if (!replyOpt) continue;

        const auto& reply = *replyOpt;

        std::lock_guard<std::mutex> lk(_mu);
        if (reply.term > _currentTerm) {
            _becomeFollower(reply.term);
            return;
        }
        if (_role != RaftRole::LEADER) return;

        if (reply.success) {
            _matchIndex[peer] = reply.matchIndex;
            _nextIndex[peer]  = reply.matchIndex + 1;
            _advanceCommitIndex();
        } else {
            // Decrement nextIndex and retry on next heartbeat
            if (_nextIndex[peer] > 1) --_nextIndex[peer];
        }
    }

    // Sleep heartbeat interval before next round
    std::this_thread::sleep_for(_cfg.heartbeatInterval);
}

// -------------------------------------------------------------------------
// State transitions
// -------------------------------------------------------------------------

void RaftNode::_becomeFollower(uint64_t term, const std::string& leaderId)
{
    // Must be called with _mu held
    _currentTerm = term;
    _role        = RaftRole::FOLLOWER;
    _votedFor.clear();
    _leaderId    = leaderId;
    _resetElectionTimer();
    logger().debug("RaftNode " + _cfg.nodeId + " → FOLLOWER term=" +
                   std::to_string(term));
}

void RaftNode::_becomeCandidate()
{
    std::lock_guard<std::mutex> lk(_mu);
    ++_currentTerm;
    _role     = RaftRole::CANDIDATE;
    _votedFor = _cfg.nodeId;
    _leaderId.clear();
    _resetElectionTimer();
    logger().debug("RaftNode " + _cfg.nodeId + " → CANDIDATE term=" +
                   std::to_string(_currentTerm));
}

void RaftNode::_becomeLeader()
{
    // Must be called with _mu held
    _role     = RaftRole::LEADER;
    _leaderId = _cfg.nodeId;

    // Initialise nextIndex and matchIndex for all peers
    uint64_t lastIdx = _lastLogIndex();
    for (const auto& peer : _cfg.peers) {
        _nextIndex[peer]  = lastIdx + 1;
        _matchIndex[peer] = 0;
    }

    logger().info("RaftNode " + _cfg.nodeId + " → LEADER term=" +
                  std::to_string(_currentTerm));
}

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

void RaftNode::_resetElectionTimer()
{
    // Randomise timeout in [min, max]
    auto minMs = _cfg.electionTimeoutMin.count();
    auto maxMs = _cfg.electionTimeoutMax.count();
    std::uniform_int_distribution<long> dist(minMs, maxMs);
    long timeoutMs = dist(_rng);
    _electionDeadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeoutMs);
}

bool RaftNode::_electionTimeoutExpired() const
{
    return std::chrono::steady_clock::now() >= _electionDeadline;
}

uint64_t RaftNode::_lastLogIndex() const
{
    // Must hold _mu
    return _log.empty() ? 0 : static_cast<uint64_t>(_log.size() - 1);
}

uint64_t RaftNode::_lastLogTerm() const
{
    // Must hold _mu
    return _log.empty() ? 0 : _log.back().term;
}

void RaftNode::_advanceCommitIndex()
{
    // Must hold _mu
    // A log entry at index N is committed when a majority of matchIndex[i] >= N
    size_t logLen = _log.size();
    for (uint64_t n = logLen - 1; n > _commitIndex; --n) {
        if (_log[n].term != _currentTerm) continue;
        int count = 1;  // count self
        for (const auto& peer : _cfg.peers) {
            auto it = _matchIndex.find(peer);
            if (it != _matchIndex.end() && it->second >= n) ++count;
        }
        int majority = static_cast<int>((_cfg.peers.size() + 1) / 2) + 1;
        if (count >= majority) {
            _commitIndex = n;
            break;
        }
    }
}

} // namespace cog0
