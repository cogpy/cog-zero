#include "test_runner.h"

#include <opencog/agentzero/distributed/CoordinationProtocol.h>
#include <opencog/atomspace/AtomSpace.h>

using namespace opencog;
using namespace opencog::agentzero;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(CoordinationProtocol_Discovery)
{
    auto as = make_as();
    CoordinationProtocol proto(as, "local-peer");
    ASSERT_TRUE(proto.initialize());
    ASSERT_TRUE(proto.initialize()); // idempotent
    ASSERT_EQ(proto.getLocalPeerId(), std::string("local-peer"));

    ASSERT_TRUE(proto.registerPeer("peer-a", "10.0.0.2", 9001));
    ASSERT_FALSE(proto.registerPeer("peer-a", "10.0.0.2", 9001));
    ASSERT_EQ(proto.getKnownPeers().size(), static_cast<size_t>(2)); // local + peer-a

    ASSERT_TRUE(proto.deregisterPeer("peer-a"));
    ASSERT_FALSE(proto.deregisterPeer("missing"));
    ASSERT_EQ(proto.getKnownPeers().size(), static_cast<size_t>(1));
}

TEST(CoordinationProtocol_HeartbeatAndLiveness)
{
    auto as = make_as();
    CoordinationProtocol proto(as, "local-peer");
    proto.initialize();
    proto.registerPeer("peer-a", "10.0.0.2", 9001);
    proto.registerPeer("peer-b", "10.0.0.3", 9002);

    ASSERT_EQ(proto.getActivePeers().size(), static_cast<size_t>(3));
    ASSERT_EQ(proto.getPeerState("peer-a"), PeerState::ACTIVE);
    ASSERT_EQ(proto.getPeerState("unknown"), PeerState::DEAD);

    ASSERT_TRUE(proto.recordHeartbeat("peer-a"));
    ASSERT_FALSE(proto.recordHeartbeat("ghost"));

    std::vector<std::pair<std::string, PeerState>> events;
    proto.setLivenessCallback([&](const std::string& id, PeerState st) {
        events.emplace_back(id, st);
    });

    // miss enough heartbeats to SUSPECT then DEAD (thresholds 2 / 5)
    // tick 1: missed=1 still ACTIVE; tick 2: missed=2 -> SUSPECTED
    proto.tickHeartbeat(2, 5);
    auto newly_dead = proto.tickHeartbeat(2, 5);
    ASSERT_EQ(proto.getPeerState("peer-a"), PeerState::SUSPECTED);
    ASSERT_TRUE(newly_dead.empty());
    ASSERT_FALSE(events.empty());

    // ticks 3-4: still SUSPECTED; tick 5: missed=5 -> DEAD
    proto.tickHeartbeat(2, 5);
    proto.tickHeartbeat(2, 5);
    newly_dead = proto.tickHeartbeat(2, 5);
    ASSERT_EQ(proto.getPeerState("peer-a"), PeerState::DEAD);
    ASSERT_FALSE(newly_dead.empty());
    bool saw_a = false;
    for (const auto& id : newly_dead) {
        if (id == "peer-a") saw_a = true;
    }
    ASSERT_TRUE(saw_a);

    // Recovery path for peer-b (also escalated without heartbeats)
    ASSERT_TRUE(proto.recordHeartbeat("peer-b"));
    ASSERT_EQ(proto.getPeerState("peer-b"), PeerState::ACTIVE);
}

TEST(CoordinationProtocol_LeaderElection)
{
    auto as = make_as();
    CoordinationProtocol proto(as, "local-peer");
    proto.initialize();
    proto.registerPeer("peer-z", "10.0.0.9", 1);

    auto r1 = proto.electLeader();
    ASSERT_TRUE(r1.success);
    ASSERT_FALSE(r1.leader_id.empty());
    ASSERT_GT(r1.round, 0);
    // lexicographically smallest active id wins
    ASSERT_EQ(r1.leader_id, std::string("local-peer"));
    ASSERT_TRUE(proto.isLocalLeader());
    ASSERT_EQ(proto.getCurrentLeader(), std::string("local-peer"));

    proto.setLeader("peer-z");
    ASSERT_EQ(proto.getCurrentLeader(), std::string("peer-z"));
    ASSERT_FALSE(proto.isLocalLeader());

    auto r2 = proto.electLeader();
    ASSERT_GT(r2.round, r1.round);
}

TEST(CoordinationProtocol_HandoffAndConsensus)
{
    auto as = make_as();
    CoordinationProtocol proto(as, "local-peer");
    proto.initialize();
    proto.registerPeer("peer-b", "10.0.0.3", 9002);

    auto req = proto.initiateHandoff("task-1", "overloaded", "peer-b");
    ASSERT_FALSE(req.request_id.empty());
    ASSERT_EQ(req.task_id, std::string("task-1"));
    ASSERT_EQ(req.source_peer, std::string("local-peer"));
    ASSERT_EQ(req.target_peer, std::string("peer-b"));
    ASSERT_EQ(req.reason, std::string("overloaded"));
    ASSERT_FALSE(req.accepted);

    ASSERT_TRUE(proto.respondToHandoff(req.request_id, true));
    auto pending = proto.getPendingHandoffs();
    // accepted handoff should not remain pending
    for (const auto& p : pending) {
        ASSERT_NE(p.request_id, req.request_id);
    }

    auto auto_req = proto.initiateHandoff("task-2", "shutdown");
    ASSERT_FALSE(auto_req.request_id.empty());
    ASSERT_FALSE(auto_req.target_peer.empty());

    auto pid = proto.createProposal("scale-out", 0.5);
    ASSERT_FALSE(pid.empty());
    ASSERT_TRUE(proto.castVote(pid, "local-peer", true));
    ASSERT_TRUE(proto.castVote(pid, "peer-b", true));
    auto result = proto.tallyVotes(pid);
    ASSERT_TRUE(result.accepted);
    ASSERT_GE(result.yes_votes, 2);

    auto metrics = proto.getMetrics();
    ASSERT_GT(metrics["total_peers"], 0);
    ASSERT_GE(metrics["active_peers"], 1);

    proto.shutdown();
}
