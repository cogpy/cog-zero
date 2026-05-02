/**
 * CoordinationProtocolTest.cpp
 *
 * Unit tests for CoordinationProtocol
 * Part of the AGENT-ZERO-GENESIS project (AZ-SCALE-001)
 */

#include <opencog/agentzero/distributed/CoordinationProtocol.h>
#include <opencog/atomspace/AtomSpace.h>

#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace opencog;
using namespace opencog::agentzero;

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class CoordinationProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        atomspace = std::make_shared<AtomSpace>();
        proto = std::make_unique<CoordinationProtocol>(atomspace, "local-peer");
        proto->initialize();
    }

    void TearDown() override {
        proto.reset();
        atomspace.reset();
    }

    AtomSpacePtr atomspace;
    std::unique_ptr<CoordinationProtocol> proto;
};

// ---------------------------------------------------------------------------
// Construction & lifecycle
// ---------------------------------------------------------------------------

TEST_F(CoordinationProtocolTest, ConstructorTest) {
    EXPECT_NE(proto, nullptr);
    EXPECT_EQ(proto->getLocalPeerId(), "local-peer");
}

TEST_F(CoordinationProtocolTest, InitializeIdempotentTest) {
    // A second initialize() on the same instance should succeed (no crash)
    bool result = proto->initialize();
    EXPECT_TRUE(result);
}

// ---------------------------------------------------------------------------
// Agent Discovery
// ---------------------------------------------------------------------------

TEST_F(CoordinationProtocolTest, RegisterPeerTest) {
    bool result = proto->registerPeer("peer-a", "192.168.0.2", 9000);
    EXPECT_TRUE(result);

    auto peers = proto->getKnownPeers();
    // local-peer + peer-a
    EXPECT_EQ(peers.size(), 2u);
}

TEST_F(CoordinationProtocolTest, RegisterDuplicatePeerTest) {
    proto->registerPeer("peer-a", "192.168.0.2", 9000);
    bool result = proto->registerPeer("peer-a", "192.168.0.2", 9000);
    EXPECT_FALSE(result);
}

TEST_F(CoordinationProtocolTest, DeregisterPeerTest) {
    proto->registerPeer("peer-a", "192.168.0.2", 9000);
    bool result = proto->deregisterPeer("peer-a");
    EXPECT_TRUE(result);

    auto peers = proto->getKnownPeers();
    // Only local-peer remains
    EXPECT_EQ(peers.size(), 1u);
}

TEST_F(CoordinationProtocolTest, DeregisterNonexistentPeerTest) {
    bool result = proto->deregisterPeer("ghost-peer");
    EXPECT_FALSE(result);
}

TEST_F(CoordinationProtocolTest, GetActivePeersTest) {
    proto->registerPeer("peer-a", "10.0.0.1", 9001);
    proto->registerPeer("peer-b", "10.0.0.2", 9002);

    auto active = proto->getActivePeers();
    // local-peer + peer-a + peer-b all start ACTIVE
    EXPECT_EQ(active.size(), 3u);
}

TEST_F(CoordinationProtocolTest, GetPeerStateTest) {
    proto->registerPeer("peer-a", "10.0.0.1", 9001);
    EXPECT_EQ(proto->getPeerState("peer-a"), PeerState::ACTIVE);
    EXPECT_EQ(proto->getPeerState("unknown"),  PeerState::DEAD);
}

// ---------------------------------------------------------------------------
// Heartbeat / Liveness
// ---------------------------------------------------------------------------

TEST_F(CoordinationProtocolTest, RecordHeartbeatTest) {
    proto->registerPeer("peer-a", "10.0.0.1", 9001);

    bool result = proto->recordHeartbeat("peer-a");
    EXPECT_TRUE(result);
    EXPECT_EQ(proto->getPeerState("peer-a"), PeerState::ACTIVE);
}

TEST_F(CoordinationProtocolTest, RecordHeartbeatUnknownPeerTest) {
    bool result = proto->recordHeartbeat("ghost");
    EXPECT_FALSE(result);
}

TEST_F(CoordinationProtocolTest, TickHeartbeatSuspectedTest) {
    proto->registerPeer("peer-a", "10.0.0.1", 9001);

    // missed_threshold=2, so after 2 ticks without a heartbeat peer-a is SUSPECTED
    proto->tickHeartbeat(2, 5);
    proto->tickHeartbeat(2, 5);

    EXPECT_EQ(proto->getPeerState("peer-a"), PeerState::SUSPECTED);
}

TEST_F(CoordinationProtocolTest, TickHeartbeatDeadTest) {
    proto->registerPeer("peer-a", "10.0.0.1", 9001);

    // dead_threshold=5, so after 5 ticks peer-a becomes DEAD
    std::vector<std::string> newly_dead;
    for (int i = 0; i < 5; ++i) {
        newly_dead = proto->tickHeartbeat(2, 5);
    }

    EXPECT_EQ(proto->getPeerState("peer-a"), PeerState::DEAD);
    EXPECT_EQ(newly_dead.size(), 1u);
    EXPECT_EQ(newly_dead[0], "peer-a");
}

TEST_F(CoordinationProtocolTest, HeartbeatRecoveryTest) {
    proto->registerPeer("peer-a", "10.0.0.1", 9001);

    // Suspect the peer
    proto->tickHeartbeat(2, 5);
    proto->tickHeartbeat(2, 5);
    EXPECT_EQ(proto->getPeerState("peer-a"), PeerState::SUSPECTED);

    // Receive a heartbeat — peer should recover to ACTIVE
    proto->recordHeartbeat("peer-a");
    EXPECT_EQ(proto->getPeerState("peer-a"), PeerState::ACTIVE);
}

TEST_F(CoordinationProtocolTest, LivenessCallbackTest) {
    proto->registerPeer("peer-a", "10.0.0.1", 9001);

    std::vector<std::pair<std::string, PeerState>> events;
    proto->setLivenessCallback([&](const std::string& id, PeerState s) {
        events.emplace_back(id, s);
    });

    // Two ticks should move peer-a to SUSPECTED and fire the callback
    proto->tickHeartbeat(2, 5);
    proto->tickHeartbeat(2, 5);

    EXPECT_FALSE(events.empty());
    EXPECT_EQ(events.back().first, "peer-a");
    EXPECT_EQ(events.back().second, PeerState::SUSPECTED);
}

// ---------------------------------------------------------------------------
// Leader Election
// ---------------------------------------------------------------------------

TEST_F(CoordinationProtocolTest, ElectLeaderTest) {
    proto->registerPeer("peer-b", "10.0.0.2", 9002);
    proto->registerPeer("peer-c", "10.0.0.3", 9003);

    ElectionResult result = proto->electLeader();
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.leader_id.empty());
    EXPECT_GT(result.round, 0);
    // Lexicographically smallest wins; "local-peer" < "peer-b" < "peer-c"
    EXPECT_EQ(result.leader_id, "local-peer");
}

TEST_F(CoordinationProtocolTest, GetCurrentLeaderTest) {
    proto->registerPeer("peer-b", "10.0.0.2", 9002);
    proto->electLeader();

    std::string leader = proto->getCurrentLeader();
    EXPECT_FALSE(leader.empty());
}

TEST_F(CoordinationProtocolTest, SetLeaderTest) {
    proto->registerPeer("peer-b", "10.0.0.2", 9002);
    proto->setLeader("peer-b");

    EXPECT_EQ(proto->getCurrentLeader(), "peer-b");
    EXPECT_FALSE(proto->isLocalLeader());
}

TEST_F(CoordinationProtocolTest, IsLocalLeaderTest) {
    proto->registerPeer("peer-b", "10.0.0.2", 9002);
    proto->registerPeer("peer-c", "10.0.0.3", 9003);

    // local-peer is lex smallest, so it should win
    proto->electLeader();
    EXPECT_TRUE(proto->isLocalLeader());
}

TEST_F(CoordinationProtocolTest, ElectionRoundIncrementTest) {
    proto->registerPeer("peer-b", "10.0.0.2", 9002);

    auto r1 = proto->electLeader();
    auto r2 = proto->electLeader();
    EXPECT_GT(r2.round, r1.round);
}

// ---------------------------------------------------------------------------
// Task Hand-off
// ---------------------------------------------------------------------------

TEST_F(CoordinationProtocolTest, InitiateHandoffTest) {
    proto->registerPeer("peer-b", "10.0.0.2", 9002);

    HandoffRequest req = proto->initiateHandoff("task-1", "overloaded");
    EXPECT_FALSE(req.request_id.empty());
    EXPECT_EQ(req.task_id,     "task-1");
    EXPECT_EQ(req.source_peer, "local-peer");
    EXPECT_EQ(req.target_peer, "peer-b");
    EXPECT_EQ(req.reason,      "overloaded");
    EXPECT_FALSE(req.accepted);
}

TEST_F(CoordinationProtocolTest, InitiateHandoffNoTargetTest) {
    // No other active peers — request should return empty target_peer
    HandoffRequest req = proto->initiateHandoff("task-1", "overloaded");
    EXPECT_TRUE(req.target_peer.empty());
}

TEST_F(CoordinationProtocolTest, RespondToHandoffTest) {
    proto->registerPeer("peer-b", "10.0.0.2", 9002);
    HandoffRequest req = proto->initiateHandoff("task-1", "shutdown");

    bool ok = proto->respondToHandoff(req.request_id, true);
    EXPECT_TRUE(ok);

    // Accepted requests are removed from the map, so no longer pending
    auto pending = proto->getPendingHandoffs();
    EXPECT_TRUE(pending.empty());
}

TEST_F(CoordinationProtocolTest, RespondToNonexistentHandoffTest) {
    bool ok = proto->respondToHandoff("nonexistent-id", true);
    EXPECT_FALSE(ok);
}

TEST_F(CoordinationProtocolTest, GetPendingHandoffsTest) {
    proto->registerPeer("peer-b", "10.0.0.2", 9002);

    proto->initiateHandoff("task-1", "overloaded");
    proto->initiateHandoff("task-2", "overloaded");

    auto pending = proto->getPendingHandoffs();
    EXPECT_EQ(pending.size(), 2u);
}

TEST_F(CoordinationProtocolTest, HandoffExplicitTargetTest) {
    proto->registerPeer("peer-b", "10.0.0.2", 9002);
    proto->registerPeer("peer-c", "10.0.0.3", 9003);

    HandoffRequest req =
        proto->initiateHandoff("task-1", "rebalancing", "peer-c");
    EXPECT_EQ(req.target_peer, "peer-c");
}

// ---------------------------------------------------------------------------
// Lightweight Consensus
// ---------------------------------------------------------------------------

TEST_F(CoordinationProtocolTest, CreateProposalTest) {
    std::string pid = proto->createProposal("Enable feature X");
    EXPECT_FALSE(pid.empty());
}

TEST_F(CoordinationProtocolTest, CastVoteTest) {
    std::string pid = proto->createProposal("Enable feature X");

    bool ok = proto->castVote(pid, "local-peer", true);
    EXPECT_TRUE(ok);
}

TEST_F(CoordinationProtocolTest, CastVoteUnknownProposalTest) {
    bool ok = proto->castVote("nonexistent", "local-peer", true);
    EXPECT_FALSE(ok);
}

TEST_F(CoordinationProtocolTest, TallyVotesMajorityAcceptedTest) {
    proto->registerPeer("peer-b", "10.0.0.2", 9002);
    proto->registerPeer("peer-c", "10.0.0.3", 9003);

    std::string pid = proto->createProposal("Scale up cluster", 0.5);
    proto->castVote(pid, "local-peer", true);
    proto->castVote(pid, "peer-b",     true);
    proto->castVote(pid, "peer-c",     false);

    ConsensusResult result = proto->tallyVotes(pid);
    EXPECT_EQ(result.proposal_id, pid);
    EXPECT_TRUE(result.accepted);
    EXPECT_EQ(result.yes_votes, 2);
    EXPECT_EQ(result.no_votes,  1);
}

TEST_F(CoordinationProtocolTest, TallyVotesMajorityRejectedTest) {
    proto->registerPeer("peer-b", "10.0.0.2", 9002);
    proto->registerPeer("peer-c", "10.0.0.3", 9003);

    std::string pid = proto->createProposal("Reduce node count", 0.5);
    proto->castVote(pid, "local-peer", false);
    proto->castVote(pid, "peer-b",     false);
    proto->castVote(pid, "peer-c",     true);

    ConsensusResult result = proto->tallyVotes(pid);
    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.yes_votes, 1);
    EXPECT_EQ(result.no_votes,  2);
}

TEST_F(CoordinationProtocolTest, TallyVotesUnknownProposalTest) {
    ConsensusResult result = proto->tallyVotes("nonexistent");
    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.yes_votes, 0);
    EXPECT_EQ(result.no_votes,  0);
}

TEST_F(CoordinationProtocolTest, TallyVotesEmptyVotesTest) {
    std::string pid = proto->createProposal("No votes yet");
    ConsensusResult result = proto->tallyVotes(pid);
    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.acceptance_ratio, 0.0);
}

// ---------------------------------------------------------------------------
// Diagnostics / Metrics
// ---------------------------------------------------------------------------

TEST_F(CoordinationProtocolTest, GetMetricsTest) {
    proto->registerPeer("peer-b", "10.0.0.2", 9002);
    proto->registerPeer("peer-c", "10.0.0.3", 9003);
    proto->createProposal("A proposal");

    auto metrics = proto->getMetrics();

    // local-peer + peer-b + peer-c = 3
    EXPECT_EQ(metrics.at("total_peers"),    3);
    EXPECT_EQ(metrics.at("active_peers"),   3);
    EXPECT_EQ(metrics.at("open_proposals"), 1);
    EXPECT_EQ(metrics.at("pending_handoffs"), 0);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
