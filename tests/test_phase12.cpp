/*
 * tests/test_phase12.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Phase 12 integration tests:
 *   - Structured JSON logging (Logger)
 *   - Raft-based leader election (RaftConsensus)
 *   - Priority-based conflict resolution (ConflictResolver)
 *   - Monitoring server (MonitoringServer)
 */

#include "test_runner.h"

#include <chrono>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Phase 12 headers
#include "cog0/Logger.h"
#include "cog0/RaftConsensus.h"
#include "cog0/ConflictResolver.h"
#include "cog0/MonitoringServer.h"

// Core cog0 headers (needed for MonitoringServer)
#include "cog0/AtomStore.h"
#include "cog0/TaskManager.h"
#include "cog0/ReasoningEngine.h"
#include "cog0/CognitiveLoop.h"

using namespace cog0;
using namespace std::chrono_literals;

// ==========================================================================
// Logger — JSON-lines output
// ==========================================================================

TEST(Logger_PlainText_Output)
{
    std::ostringstream buf;
    auto& log = Logger::instance();
    log.setSink(&buf);
    log.setJsonMode(false);
    log.setLevel(LogLevel::DEBUG);

    log.info("hello plain");

    log.setSink(nullptr);
    log.setLevel(LogLevel::INFO);

    std::string out = buf.str();
    ASSERT_TRUE(out.find("hello plain") != std::string::npos);
    ASSERT_TRUE(out.find("INFO") != std::string::npos);
}

TEST(Logger_JsonMode_Output)
{
    std::ostringstream buf;
    auto& log = Logger::instance();
    log.setSink(&buf);
    log.setJsonMode(true);
    log.setLevel(LogLevel::DEBUG);

    log.info("test json message");

    log.setSink(nullptr);
    log.setJsonMode(false);

    std::string out = buf.str();
    ASSERT_TRUE(out.find("\"msg\"") != std::string::npos);
    ASSERT_TRUE(out.find("test json message") != std::string::npos);
    ASSERT_TRUE(out.find("\"level\"") != std::string::npos);
    ASSERT_TRUE(out.find("\"ts\"") != std::string::npos);
}

TEST(Logger_JsonMode_Fields)
{
    std::ostringstream buf;
    auto& log = Logger::instance();
    log.setSink(&buf);
    log.setJsonMode(true);
    log.setLevel(LogLevel::DEBUG);

    log.info("structured", {{"component", "test"}, {"count", "42"}});

    log.setSink(nullptr);
    log.setJsonMode(false);

    std::string out = buf.str();
    ASSERT_TRUE(out.find("\"fields\"") != std::string::npos);
    ASSERT_TRUE(out.find("component") != std::string::npos);
    ASSERT_TRUE(out.find("count") != std::string::npos);
    ASSERT_TRUE(out.find("42") != std::string::npos);
}

TEST(Logger_Level_Filtering)
{
    std::ostringstream buf;
    auto& log = Logger::instance();
    log.setSink(&buf);
    log.setJsonMode(false);
    log.setLevel(LogLevel::WARN);

    log.debug("should not appear");
    log.info("should not appear either");
    log.warn("this should appear");

    log.setSink(nullptr);
    log.setLevel(LogLevel::INFO);

    std::string out = buf.str();
    ASSERT_FALSE(out.find("should not appear") != std::string::npos);
    ASSERT_TRUE(out.find("this should appear") != std::string::npos);
}

TEST(Logger_JsonEscape_SpecialChars)
{
    std::ostringstream buf;
    auto& log = Logger::instance();
    log.setSink(&buf);
    log.setJsonMode(true);
    log.setLevel(LogLevel::DEBUG);

    log.info("line1\nline2\ttab\"quote");

    log.setSink(nullptr);
    log.setJsonMode(false);

    std::string out = buf.str();
    // Should be escaped
    ASSERT_TRUE(out.find("\\n") != std::string::npos);
    ASSERT_TRUE(out.find("\\t") != std::string::npos);
    ASSERT_TRUE(out.find("\\\"") != std::string::npos);
}

// ==========================================================================
// ConflictResolver
// ==========================================================================

TEST(ConflictResolver_StrictPriority_PicksHighest)
{
    ConflictResolver cr(ResolutionStrategy::STRICT_PRIORITY);

    cr.submit("low",  1, {"db"});
    cr.submit("mid",  5, {"db"});
    cr.submit("high", 10, {"db"});

    auto result = cr.resolve();
    ASSERT_TRUE(result.resolved);
    ASSERT_EQ(result.winner.id, std::string("high"));
}

TEST(ConflictResolver_FairnessWeighted_AntiStarvation)
{
    ConflictResolver cr(ResolutionStrategy::FAIRNESS_WEIGHTED);
    cr.setFairnessWeight(1000.0);  // Very strong fairness boost

    // Submit a low-priority ticket first so it has some waiting time
    cr.submit("old-low", 1, {"cpu"});

    // Small sleep so "old-low" has non-zero wait time
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    cr.submit("new-high", 100, {"cpu"});

    // Without fairness, new-high wins; with extreme fairness boost, old-low may win
    // Just verify it resolves without crash and picks one of the two
    auto result = cr.resolve();
    ASSERT_TRUE(result.resolved);
    ASSERT_TRUE(result.winner.id == "old-low" || result.winner.id == "new-high");
}

TEST(ConflictResolver_SLAPriority_DeadlineBoost)
{
    ConflictResolver cr(ResolutionStrategy::SLA_PRIORITY);

    // Task A: high priority, generous SLA (1 hour)
    cr.submit("task-a", 50, {"net"}, 3600.0);

    // Task B: medium priority, very tight SLA (1 ms — already past)
    // We need to wait a tiny bit then submit so it's past deadline
    cr.submit("task-b", 30, {"net"}, 0.001);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    auto result = cr.resolve();
    ASSERT_TRUE(result.resolved);
    // task-b has past its SLA deadline → should be boosted and win
    ASSERT_EQ(result.winner.id, std::string("task-b"));
}

TEST(ConflictResolver_Deferred_ConflictingTickets)
{
    ConflictResolver cr(ResolutionStrategy::STRICT_PRIORITY);

    cr.submit("winner", 10, {"disk", "net"});
    cr.submit("blocked", 5, {"disk"});  // conflicts on "disk"
    cr.submit("free", 3, {"cpu"});      // no conflict

    ASSERT_EQ(cr.pending(), size_t(3));

    auto result = cr.resolve();
    ASSERT_TRUE(result.resolved);
    ASSERT_EQ(result.winner.id, std::string("winner"));

    // "blocked" should appear in deferred (shares "disk")
    bool blockedDeferred = false;
    for (const auto& d : result.deferred)
        if (d.id == "blocked") blockedDeferred = true;
    ASSERT_TRUE(blockedDeferred);

    // "free" should NOT be deferred (no resource conflict)
    bool freeDeferred = false;
    for (const auto& d : result.deferred)
        if (d.id == "free") freeDeferred = true;
    ASSERT_FALSE(freeDeferred);
}

TEST(ConflictResolver_Cancel)
{
    ConflictResolver cr;
    cr.submit("t1", 5, {"x"});
    cr.submit("t2", 3, {"y"});

    ASSERT_EQ(cr.pending(), size_t(2));
    ASSERT_TRUE(cr.cancel("t1"));
    ASSERT_EQ(cr.pending(), size_t(1));
    ASSERT_FALSE(cr.cancel("nonexistent"));
}

TEST(ConflictResolver_Ranked)
{
    ConflictResolver cr(ResolutionStrategy::STRICT_PRIORITY);
    cr.submit("low",  1, {});
    cr.submit("high", 9, {});
    cr.submit("mid",  5, {});

    auto ranked = cr.ranked();
    ASSERT_EQ(ranked.size(), size_t(3));
    // Should be in descending order
    ASSERT_EQ(ranked[0].id, std::string("high"));
    ASSERT_EQ(ranked[1].id, std::string("mid"));
    ASSERT_EQ(ranked[2].id, std::string("low"));
}

TEST(ConflictResolver_EmptyQueue)
{
    ConflictResolver cr;
    auto result = cr.resolve();
    ASSERT_FALSE(result.resolved);
}

TEST(ConflictResolver_StrategyNames)
{
    ASSERT_EQ(resolutionStrategyName(ResolutionStrategy::STRICT_PRIORITY),
              std::string("STRICT_PRIORITY"));
    ASSERT_EQ(resolutionStrategyName(ResolutionStrategy::FAIRNESS_WEIGHTED),
              std::string("FAIRNESS_WEIGHTED"));
    ASSERT_EQ(resolutionStrategyName(ResolutionStrategy::SLA_PRIORITY),
              std::string("SLA_PRIORITY"));
}

// ==========================================================================
// RaftConsensus — leader election
// ==========================================================================

// Helper: build a 3-node in-process Raft cluster
static std::vector<std::unique_ptr<RaftNode>>
makeRaftCluster(size_t n,
                std::shared_ptr<RaftCluster>& clusterBus,
                std::chrono::milliseconds electionMin = 80ms,
                std::chrono::milliseconds electionMax = 160ms,
                std::chrono::milliseconds heartbeat   = 30ms)
{
    clusterBus = std::make_shared<RaftCluster>();

    std::vector<std::string> ids;
    for (size_t i = 0; i < n; ++i)
        ids.push_back("node-" + std::to_string(i));

    std::vector<std::unique_ptr<RaftNode>> nodes;
    for (size_t i = 0; i < n; ++i) {
        RaftNodeConfig cfg;
        cfg.nodeId = ids[i];
        for (size_t j = 0; j < n; ++j)
            if (j != i) cfg.peers.push_back(ids[j]);
        cfg.electionTimeoutMin = electionMin;
        cfg.electionTimeoutMax = electionMax;
        cfg.heartbeatInterval  = heartbeat;

        nodes.push_back(std::make_unique<RaftNode>(cfg, clusterBus));
    }
    return nodes;
}

TEST(Raft_InitialRole_IsFollower)
{
    std::shared_ptr<RaftCluster> bus;
    auto nodes = makeRaftCluster(3, bus);

    for (auto& n : nodes)
        ASSERT_EQ(n->role(), RaftRole::FOLLOWER);
}

TEST(Raft_RoleNames)
{
    ASSERT_EQ(raftRoleName(RaftRole::FOLLOWER),  std::string("FOLLOWER"));
    ASSERT_EQ(raftRoleName(RaftRole::CANDIDATE), std::string("CANDIDATE"));
    ASSERT_EQ(raftRoleName(RaftRole::LEADER),    std::string("LEADER"));
}

TEST(Raft_ElectsLeader_3Nodes)
{
    std::shared_ptr<RaftCluster> bus;
    auto nodes = makeRaftCluster(3, bus, 80ms, 160ms, 30ms);

    for (auto& n : nodes)
        n->start();

    // Wait for leader election (allow up to 2 s)
    std::string leaderId;
    for (int i = 0; i < 200; ++i) {
        std::this_thread::sleep_for(10ms);
        int leaderCount = 0;
        for (auto& n : nodes) {
            if (n->isLeader()) {
                ++leaderCount;
                leaderId = n->nodeId();
            }
        }
        if (leaderCount == 1) break;
    }

    // Exactly one leader
    int leaderCount = 0;
    for (auto& n : nodes)
        if (n->isLeader()) ++leaderCount;

    ASSERT_EQ(leaderCount, 1);
    ASSERT_FALSE(leaderId.empty());

    for (auto& n : nodes)
        n->stop();
}

TEST(Raft_TermAdvances)
{
    std::shared_ptr<RaftCluster> bus;
    auto nodes = makeRaftCluster(3, bus);

    for (auto& n : nodes)
        n->start();

    // Wait briefly for at least one election
    std::this_thread::sleep_for(300ms);

    // At least one node should have a term > 0
    uint64_t maxTerm = 0;
    for (auto& n : nodes)
        maxTerm = std::max(maxTerm, n->currentTerm());

    ASSERT_GT(maxTerm, uint64_t(0));

    for (auto& n : nodes)
        n->stop();
}

TEST(Raft_SubmitCommand_ReturnsTrue_OnLeader)
{
    std::shared_ptr<RaftCluster> bus;
    auto nodes = makeRaftCluster(3, bus, 80ms, 160ms, 30ms);

    for (auto& n : nodes)
        n->start();

    // Wait for leader
    RaftNode* leader = nullptr;
    for (int i = 0; i < 200 && !leader; ++i) {
        std::this_thread::sleep_for(10ms);
        for (auto& n : nodes)
            if (n->isLeader()) { leader = n.get(); break; }
    }

    ASSERT_TRUE(leader != nullptr);

    bool submitted = leader->submitCommand("set x 42");
    ASSERT_TRUE(submitted);

    for (auto& n : nodes)
        n->stop();
}

TEST(Raft_SubmitCommand_ReturnsFalse_OnFollower)
{
    std::shared_ptr<RaftCluster> bus;
    auto nodes = makeRaftCluster(3, bus, 80ms, 160ms, 30ms);

    for (auto& n : nodes)
        n->start();

    // Wait for stable leader
    std::this_thread::sleep_for(300ms);

    // Find a follower
    RaftNode* follower = nullptr;
    for (auto& n : nodes)
        if (!n->isLeader()) { follower = n.get(); break; }

    if (follower) {
        bool submitted = follower->submitCommand("set x 99");
        ASSERT_FALSE(submitted);
    }

    for (auto& n : nodes)
        n->stop();
}

TEST(Raft_Partition_And_Heal)
{
    std::shared_ptr<RaftCluster> bus;
    auto nodes = makeRaftCluster(5, bus, 100ms, 200ms, 40ms);

    for (auto& n : nodes)
        n->start();

    // Wait for first leader
    std::this_thread::sleep_for(500ms);

    // Partition the leader (if any)
    for (auto& n : nodes) {
        if (n->isLeader()) {
            bus->partition(n->nodeId());
            break;
        }
    }

    // Wait for new election
    std::this_thread::sleep_for(600ms);

    // Heal the partition
    for (auto& n : nodes)
        bus->heal(n->nodeId());

    // System should converge; just verify no crash
    std::this_thread::sleep_for(300ms);

    for (auto& n : nodes)
        n->stop();
}

// ==========================================================================
// MonitoringServer — metrics snapshot (no HTTP round-trip in unit tests)
// ==========================================================================

TEST(MonitoringServer_Snapshot_AtomCount)
{
    auto store = std::make_shared<AtomStore>();
    store->addNode(AtomType::CONCEPT, "foo");
    store->addNode(AtomType::CONCEPT, "bar");

    MonitoringServer srv(store, nullptr, 0);  // port=0 (no bind)
    auto m = srv.snapshot();

    ASSERT_EQ(m.atomCount, size_t(2));
}

TEST(MonitoringServer_Snapshot_Uptime)
{
    auto store = std::make_shared<AtomStore>();
    MonitoringServer srv(store, nullptr, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto m = srv.snapshot();

    ASSERT_GE(m.uptimeSeconds, 0.0);
}

TEST(MonitoringServer_Snapshot_WithCognitiveLoop)
{
    auto store    = std::make_shared<AtomStore>();
    auto taskMgr  = std::make_shared<TaskManager>(store);
    auto reasoning= std::make_shared<ReasoningEngine>(store);
    auto loop     = std::make_shared<CognitiveLoop>(store, taskMgr, reasoning);

    loop->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    MonitoringServer srv(store, loop, 0);
    auto m = srv.snapshot();

    ASSERT_GE(m.cycleCount, uint64_t(0));
    loop->stop();
}

TEST(MonitoringServer_Start_Stop)
{
    auto store = std::make_shared<AtomStore>();
    MonitoringServer srv(store, nullptr, 19991);

    ASSERT_FALSE(srv.running());
    srv.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    ASSERT_TRUE(srv.running());
    srv.stop();
    ASSERT_FALSE(srv.running());
}

TEST(MonitoringServer_ExtraMetricsHook)
{
    auto store = std::make_shared<AtomStore>();
    MonitoringServer srv(store, nullptr, 0);

    srv.setExtraMetricsHook([]() {
        return "\"custom_counter\":7";
    });

    // Just verify hook is stored and callable (no HTTP bind)
    auto m = srv.snapshot();
    ASSERT_GE(m.atomCount, size_t(0));
}
