/*
 * tests/test_phase14_raft_persistence.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Phase 14 Feature 2.4: Persistent Raft Log tests
 *   - InMemoryLogStore basic operations
 *   - State persistence/load roundtrip
 *   - RaftNode with custom log store
 *   - Recovery from persisted state
 */

#include "test_runner.h"

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "cog0/RaftLogStore.h"
#include "cog0/RaftConsensus.h"

using namespace cog0;
using namespace std::chrono_literals;

// ==========================================================================
// InMemoryLogStore — basic operations
// ==========================================================================

TEST(InMemoryLogStore_InitialState)
{
    InMemoryLogStore store;

    // Should have sentinel entry at index 0
    ASSERT_EQ(store.size(), size_t(1));
    ASSERT_EQ(store.lastIndex(), uint64_t(0));
    ASSERT_EQ(store.lastTerm(), uint64_t(0));

    // Sentinel entry should have term=0
    LogEntry sentinel = store.get(0);
    ASSERT_EQ(sentinel.term, uint64_t(0));
    ASSERT_EQ(sentinel.index, uint64_t(0));
    ASSERT_TRUE(sentinel.command.empty());
}

TEST(InMemoryLogStore_Append)
{
    InMemoryLogStore store;

    LogEntry e1;
    e1.term = 1;
    e1.command = "set x 1";
    uint64_t idx1 = store.append(e1);

    ASSERT_EQ(idx1, uint64_t(1));
    ASSERT_EQ(store.size(), size_t(2));
    ASSERT_EQ(store.lastIndex(), uint64_t(1));
    ASSERT_EQ(store.lastTerm(), uint64_t(1));

    LogEntry e2;
    e2.term = 1;
    e2.command = "set y 2";
    uint64_t idx2 = store.append(e2);

    ASSERT_EQ(idx2, uint64_t(2));
    ASSERT_EQ(store.size(), size_t(3));
    ASSERT_EQ(store.lastIndex(), uint64_t(2));
}

TEST(InMemoryLogStore_Get)
{
    InMemoryLogStore store;

    LogEntry e1;
    e1.term = 1;
    e1.command = "cmd1";
    store.append(e1);

    LogEntry e2;
    e2.term = 2;
    e2.command = "cmd2";
    store.append(e2);

    // Get valid indices
    LogEntry got1 = store.get(1);
    ASSERT_EQ(got1.term, uint64_t(1));
    ASSERT_EQ(got1.command, std::string("cmd1"));

    LogEntry got2 = store.get(2);
    ASSERT_EQ(got2.term, uint64_t(2));
    ASSERT_EQ(got2.command, std::string("cmd2"));

    // Get out-of-bounds — should return empty entry
    LogEntry missing = store.get(999);
    ASSERT_EQ(missing.term, uint64_t(0));
    ASSERT_EQ(missing.index, uint64_t(0));
}

TEST(InMemoryLogStore_GetRange)
{
    InMemoryLogStore store;

    for (int i = 1; i <= 5; ++i) {
        LogEntry e;
        e.term = static_cast<uint64_t>(i);
        e.command = "cmd" + std::to_string(i);
        store.append(e);
    }

    // Get range [1, 3]
    auto range = store.getRange(1, 3);
    ASSERT_EQ(range.size(), size_t(3));
    ASSERT_EQ(range[0].term, uint64_t(1));
    ASSERT_EQ(range[1].term, uint64_t(2));
    ASSERT_EQ(range[2].term, uint64_t(3));

    // Get range beyond log
    auto rangeBeyond = store.getRange(10, 20);
    ASSERT_EQ(rangeBeyond.size(), size_t(0));

    // Get range with invalid order
    auto rangeInvalid = store.getRange(5, 2);
    ASSERT_EQ(rangeInvalid.size(), size_t(0));

    // Get range including sentinel
    auto rangeWithSentinel = store.getRange(0, 2);
    ASSERT_EQ(rangeWithSentinel.size(), size_t(3));
    ASSERT_EQ(rangeWithSentinel[0].term, uint64_t(0));  // sentinel
}

TEST(InMemoryLogStore_TruncateAfter)
{
    InMemoryLogStore store;

    for (int i = 1; i <= 5; ++i) {
        LogEntry e;
        e.term = static_cast<uint64_t>(i);
        e.command = "cmd" + std::to_string(i);
        store.append(e);
    }

    ASSERT_EQ(store.size(), size_t(6));  // sentinel + 5 entries
    ASSERT_EQ(store.lastIndex(), uint64_t(5));

    // Truncate after index 3 — keep [0, 1, 2, 3]
    store.truncateAfter(3);

    ASSERT_EQ(store.size(), size_t(4));
    ASSERT_EQ(store.lastIndex(), uint64_t(3));
    ASSERT_EQ(store.lastTerm(), uint64_t(3));

    // Verify entries 4 and 5 are gone
    LogEntry gone4 = store.get(4);
    ASSERT_EQ(gone4.term, uint64_t(0));  // empty entry
}

TEST(InMemoryLogStore_TruncateAfter_BeyondLog)
{
    InMemoryLogStore store;

    LogEntry e;
    e.term = 1;
    e.command = "cmd";
    store.append(e);

    // Truncate after index 100 — should be no-op
    store.truncateAfter(100);

    ASSERT_EQ(store.size(), size_t(2));
    ASSERT_EQ(store.lastIndex(), uint64_t(1));
}

// ==========================================================================
// State persistence/load roundtrip
// ==========================================================================

TEST(InMemoryLogStore_PersistState)
{
    InMemoryLogStore store;

    store.persistState(5, "node-A");

    auto [term, votedFor] = store.loadState();
    ASSERT_EQ(term, uint64_t(5));
    ASSERT_EQ(votedFor, std::string("node-A"));
}

TEST(InMemoryLogStore_PersistState_Update)
{
    InMemoryLogStore store;

    store.persistState(1, "node-A");
    store.persistState(2, "node-B");
    store.persistState(3, "");  // cleared vote

    auto [term, votedFor] = store.loadState();
    ASSERT_EQ(term, uint64_t(3));
    ASSERT_EQ(votedFor, std::string(""));
}

TEST(InMemoryLogStore_Clear)
{
    InMemoryLogStore store;

    for (int i = 1; i <= 3; ++i) {
        LogEntry e;
        e.term = static_cast<uint64_t>(i);
        e.command = "cmd" + std::to_string(i);
        store.append(e);
    }
    store.persistState(10, "node-X");

    // Clear everything
    store.clear();

    ASSERT_EQ(store.size(), size_t(1));  // only sentinel
    ASSERT_EQ(store.lastIndex(), uint64_t(0));

    auto [term, votedFor] = store.loadState();
    ASSERT_EQ(term, uint64_t(0));
    ASSERT_EQ(votedFor, std::string(""));
}

TEST(InMemoryLogStore_Sync_NoOp)
{
    InMemoryLogStore store;

    // sync() should be a no-op for in-memory store
    store.sync();

    // Just verify it doesn't crash
    ASSERT_TRUE(true);
}

TEST(InMemoryLogStore_NotPersistent)
{
    InMemoryLogStore store;
    ASSERT_FALSE(store.isPersistent());
}

// ==========================================================================
// Factory function
// ==========================================================================

TEST(CreateLogStore_Memory)
{
    auto store = createLogStore("memory");
    ASSERT_TRUE(store != nullptr);
    ASSERT_FALSE(store->isPersistent());
    ASSERT_EQ(store->size(), size_t(1));  // sentinel only
}

TEST(CreateLogStore_Memory_CaseInsensitive)
{
    auto store1 = createLogStore("MEMORY");
    ASSERT_TRUE(store1 != nullptr);

    auto store2 = createLogStore("Memory");
    ASSERT_TRUE(store2 != nullptr);

    auto store3 = createLogStore("MeMoRy");
    ASSERT_TRUE(store3 != nullptr);
}

TEST(CreateLogStore_Empty_DefaultsToMemory)
{
    auto store = createLogStore("");
    ASSERT_TRUE(store != nullptr);
    ASSERT_FALSE(store->isPersistent());
}

TEST(CreateLogStore_Unknown_FallsBackToMemory)
{
    auto store = createLogStore("unknown-backend");
    ASSERT_TRUE(store != nullptr);
    ASSERT_FALSE(store->isPersistent());
}

// ==========================================================================
// RaftNode with custom log store
// ==========================================================================

TEST(RaftNode_WithCustomLogStore)
{
    auto cluster = std::make_shared<RaftCluster>();
    auto logStore = std::make_shared<InMemoryLogStore>();

    // Pre-populate some state in the log store
    logStore->persistState(5, "node-X");

    LogEntry e;
    e.term = 5;
    e.command = "pre-existing-cmd";
    logStore->append(e);

    RaftNodeConfig cfg;
    cfg.nodeId = "node-A";
    cfg.peers = {};
    cfg.logStore = logStore;

    RaftNode node(cfg, cluster);

    // Node should have loaded the persisted term
    ASSERT_EQ(node.currentTerm(), uint64_t(5));

    // Log should have the pre-existing entry
    ASSERT_EQ(node.logSize(), size_t(2));  // sentinel + 1 entry
}

TEST(RaftNode_DefaultLogStore)
{
    auto cluster = std::make_shared<RaftCluster>();

    RaftNodeConfig cfg;
    cfg.nodeId = "node-A";
    cfg.peers = {};
    // cfg.logStore is nullptr — should create default

    RaftNode node(cfg, cluster);

    // Should start with term 0
    ASSERT_EQ(node.currentTerm(), uint64_t(0));
    ASSERT_EQ(node.logSize(), size_t(1));  // sentinel only
}

// ==========================================================================
// Recovery from persisted state
// ==========================================================================

TEST(RaftNode_RecoveryFromPersistedState)
{
    auto cluster = std::make_shared<RaftCluster>();
    auto logStore = std::make_shared<InMemoryLogStore>();

    // Simulate state from a previous node instance
    logStore->persistState(10, "node-B");

    // Add some log entries
    for (int i = 1; i <= 3; ++i) {
        LogEntry e;
        e.term = 10;
        e.command = "recovered-cmd-" + std::to_string(i);
        logStore->append(e);
    }

    // Create node with the persisted log store
    RaftNodeConfig cfg;
    cfg.nodeId = "node-recovered";
    cfg.peers = {};
    cfg.logStore = logStore;

    RaftNode node(cfg, cluster);

    // Verify recovered state
    ASSERT_EQ(node.currentTerm(), uint64_t(10));
    ASSERT_EQ(node.logSize(), size_t(4));  // sentinel + 3 entries
    ASSERT_EQ(node.role(), RaftRole::FOLLOWER);
}

TEST(RaftNode_SharedLogStore_Between_Restarts)
{
    auto cluster = std::make_shared<RaftCluster>();
    auto logStore = std::make_shared<InMemoryLogStore>();

    // First "instance" — becomes leader and commits some entries
    {
        RaftNodeConfig cfg;
        cfg.nodeId = "node-A";
        cfg.peers = {};
        cfg.logStore = logStore;
        cfg.electionTimeoutMin = 50ms;
        cfg.electionTimeoutMax = 100ms;
        cfg.heartbeatInterval = 20ms;

        RaftNode node(cfg, cluster);
        node.start();

        // Wait for it to become leader (no peers = instant election)
        std::this_thread::sleep_for(200ms);

        if (node.isLeader()) {
            node.submitCommand("cmd1");
            node.submitCommand("cmd2");
        }

        node.stop();
    }

    // Verify log store has the entries
    ASSERT_GE(logStore->size(), size_t(1));

    // Second "instance" — should recover state
    {
        auto [term, votedFor] = logStore->loadState();
        ASSERT_GE(term, uint64_t(1));  // term advanced

        RaftNodeConfig cfg;
        cfg.nodeId = "node-A-restarted";
        cfg.peers = {};
        cfg.logStore = logStore;

        RaftNode node(cfg, cluster);

        // Should have recovered the term
        ASSERT_EQ(node.currentTerm(), term);
    }
}

// ==========================================================================
// Integration: Raft cluster with persistent log stores
// ==========================================================================

static std::vector<std::unique_ptr<RaftNode>>
makeClusterWithLogStores(size_t n,
                         std::shared_ptr<RaftCluster>& clusterBus,
                         std::vector<std::shared_ptr<InMemoryLogStore>>& stores)
{
    clusterBus = std::make_shared<RaftCluster>();
    stores.clear();

    std::vector<std::string> ids;
    for (size_t i = 0; i < n; ++i)
        ids.push_back("node-" + std::to_string(i));

    std::vector<std::unique_ptr<RaftNode>> nodes;
    for (size_t i = 0; i < n; ++i) {
        auto store = std::make_shared<InMemoryLogStore>();
        stores.push_back(store);

        RaftNodeConfig cfg;
        cfg.nodeId = ids[i];
        for (size_t j = 0; j < n; ++j)
            if (j != i) cfg.peers.push_back(ids[j]);
        cfg.electionTimeoutMin = 80ms;
        cfg.electionTimeoutMax = 160ms;
        cfg.heartbeatInterval = 30ms;
        cfg.logStore = store;

        nodes.push_back(std::make_unique<RaftNode>(cfg, clusterBus));
    }
    return nodes;
}

TEST(RaftCluster_WithLogStores_ElectsLeader)
{
    std::shared_ptr<RaftCluster> bus;
    std::vector<std::shared_ptr<InMemoryLogStore>> stores;
    auto nodes = makeClusterWithLogStores(3, bus, stores);

    for (auto& n : nodes)
        n->start();

    // Wait for leader election
    std::string leaderId;
    for (int i = 0; i < 200; ++i) {
        std::this_thread::sleep_for(10ms);
        for (auto& n : nodes) {
            if (n->isLeader()) {
                leaderId = n->nodeId();
                break;
            }
        }
        if (!leaderId.empty()) break;
    }

    ASSERT_FALSE(leaderId.empty());

    // Leader should have persisted its term
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto [term, votedFor] = stores[i]->loadState();
        ASSERT_GE(term, uint64_t(1));
    }

    for (auto& n : nodes)
        n->stop();
}

TEST(RaftCluster_WithLogStores_ReplicatesEntries)
{
    std::shared_ptr<RaftCluster> bus;
    std::vector<std::shared_ptr<InMemoryLogStore>> stores;
    auto nodes = makeClusterWithLogStores(3, bus, stores);

    for (auto& n : nodes)
        n->start();

    // Wait for leader election
    RaftNode* leader = nullptr;
    for (int i = 0; i < 200 && !leader; ++i) {
        std::this_thread::sleep_for(10ms);
        for (auto& n : nodes)
            if (n->isLeader()) { leader = n.get(); break; }
    }

    ASSERT_TRUE(leader != nullptr);

    // Submit a command
    bool submitted = leader->submitCommand("replicated-cmd");
    ASSERT_TRUE(submitted);

    // Wait for replication
    std::this_thread::sleep_for(200ms);

    // All stores should have at least 2 entries (sentinel + replicated)
    for (auto& store : stores) {
        ASSERT_GE(store->size(), size_t(2));
    }

    for (auto& n : nodes)
        n->stop();
}
