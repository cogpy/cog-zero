/**
 * CoordinationProtocol.h
 *
 * Multi-Agent Coordination Protocols for Distributed Agent-Zero
 * Part of the AGENT-ZERO-GENESIS project (AZ-SCALE-001)
 *
 * Implements low-level distributed coordination protocols including agent
 * discovery, heartbeat/liveness detection, leader election, distributed
 * task hand-off, and a lightweight consensus mechanism, all integrated
 * with OpenCog's AtomSpace.
 */

#ifndef _OPENCOG_AGENTZERO_COORDINATION_PROTOCOL_H
#define _OPENCOG_AGENTZERO_COORDINATION_PROTOCOL_H

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

namespace opencog { namespace agentzero {

// -----------------------------------------------------------------------
// Supporting data types
// -----------------------------------------------------------------------

/**
 * Liveness state of a protocol peer
 */
enum class PeerState {
    ACTIVE,      ///< Peer is responsive
    SUSPECTED,   ///< Peer missed recent heartbeat(s)
    DEAD         ///< Peer declared unreachable
};

/**
 * Information about a peer participating in coordination
 */
struct ProtocolPeer {
    std::string peer_id;
    std::string address;       ///< hostname or IP
    int         port;
    PeerState   state;
    std::chrono::system_clock::time_point last_seen;
    int         missed_heartbeats;

    ProtocolPeer()
        : port(0), state(PeerState::ACTIVE), missed_heartbeats(0),
          last_seen(std::chrono::system_clock::now()) {}

    ProtocolPeer(const std::string& id,
                 const std::string& addr,
                 int p)
        : peer_id(id), address(addr), port(p),
          state(PeerState::ACTIVE), missed_heartbeats(0),
          last_seen(std::chrono::system_clock::now()) {}
};

/**
 * Outcome of a leader-election round
 */
struct ElectionResult {
    std::string leader_id;   ///< ID of the elected leader (empty = no quorum)
    bool        success;     ///< True when a stable leader was elected
    int         round;       ///< Election round number

    ElectionResult() : success(false), round(0) {}
};

/**
 * Result of a consensus vote
 */
struct ConsensusResult {
    std::string proposal_id;
    bool        accepted;        ///< True if quorum voted yes
    int         yes_votes;
    int         no_votes;
    double      acceptance_ratio;

    ConsensusResult()
        : accepted(false), yes_votes(0), no_votes(0), acceptance_ratio(0.0) {}
};

/**
 * A task hand-off request between agents
 */
struct HandoffRequest {
    std::string request_id;
    std::string task_id;
    std::string source_peer;   ///< Peer giving up the task
    std::string target_peer;   ///< Peer taking on the task (empty = auto-select)
    std::string reason;        ///< Reason for hand-off (e.g. "overloaded", "shutdown")
    bool        accepted;

    HandoffRequest() : accepted(false) {}
};

// -----------------------------------------------------------------------
// CoordinationProtocol
// -----------------------------------------------------------------------

/**
 * CoordinationProtocol
 *
 * Implements the low-level distributed coordination protocols required by
 * multi-agent clusters:
 *
 *  - **Agent Discovery** – peers announce themselves and can be queried.
 *  - **Heartbeat / Liveness** – each peer periodically signals it is alive;
 *    missed heartbeats escalate a peer's state to SUSPECTED then DEAD.
 *  - **Leader Election** – a simple bully-style election selects one peer as
 *    the coordinator for a given epoch.
 *  - **Task Hand-off** – a peer that can no longer execute a task delegates
 *    it to another capable peer.
 *  - **Lightweight Consensus** – majority-voting on binary proposals.
 *
 * All state changes are mirrored into the shared AtomSpace so that other
 * OpenCog modules can reason about cluster topology.
 */
class CoordinationProtocol {
public:
    /**
     * Constructor
     * @param atomspace Shared AtomSpace for cluster-state representation
     * @param local_peer_id Unique identifier of the local peer (this node)
     */
    CoordinationProtocol(AtomSpacePtr atomspace,
                         const std::string& local_peer_id);

    /** Destructor – calls shutdown() */
    ~CoordinationProtocol();

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    /**
     * Initialise the protocol and register the local peer.
     * @return true on success
     */
    bool initialize();

    /**
     * Cleanly shut down, marking the local peer as DEAD in AtomSpace.
     */
    void shutdown();

    // ------------------------------------------------------------------
    // Agent Discovery
    // ------------------------------------------------------------------

    /**
     * Register a remote peer so it participates in coordination.
     * @param peer_id   Unique peer identifier
     * @param address   Network address (hostname or IP)
     * @param port      Port number
     * @return true if registered (false if duplicate)
     */
    bool registerPeer(const std::string& peer_id,
                      const std::string& address,
                      int port);

    /**
     * Remove a peer from the known-peer set.
     * @param peer_id Peer to remove
     * @return true if removed
     */
    bool deregisterPeer(const std::string& peer_id);

    /**
     * Get all known peers.
     */
    std::vector<ProtocolPeer> getKnownPeers() const;

    /**
     * Get peers currently in ACTIVE state.
     */
    std::vector<std::string> getActivePeers() const;

    /**
     * Query the state of a specific peer.
     * @param peer_id Peer identifier
     * @return Current PeerState (DEAD if unknown)
     */
    PeerState getPeerState(const std::string& peer_id) const;

    // ------------------------------------------------------------------
    // Heartbeat / Liveness
    // ------------------------------------------------------------------

    /**
     * Record a heartbeat from a peer, resetting its missed-count.
     * @param peer_id Sending peer
     * @return true if peer is known
     */
    bool recordHeartbeat(const std::string& peer_id);

    /**
     * Advance the heartbeat epoch: increment missed counts and escalate
     * state for peers that have not been heard from.
     *
     * Call this periodically (e.g., once per heartbeat interval).
     *
     * @param missed_threshold   Missed beats before SUSPECTED (default 2)
     * @param dead_threshold     Missed beats before DEAD (default 5)
     * @return IDs of peers whose state changed to DEAD this round
     */
    std::vector<std::string> tickHeartbeat(int missed_threshold = 2,
                                           int dead_threshold   = 5);

    /**
     * Set a callback invoked when a peer's liveness state changes.
     */
    void setLivenessCallback(
        std::function<void(const std::string&, PeerState)> cb);

    // ------------------------------------------------------------------
    // Leader Election  (bully-style, by lexicographic peer ID)
    // ------------------------------------------------------------------

    /**
     * Run a leader-election round among the active peers.
     * The peer with the lexicographically smallest ID wins (simple bully).
     * @return ElectionResult with the winner's ID, or empty on failure.
     */
    ElectionResult electLeader();

    /**
     * Get the currently elected leader (empty string if none).
     */
    std::string getCurrentLeader() const;

    /**
     * Forcefully set the leader (used when receiving an external
     * election result from the network layer).
     * @param leader_id Peer ID of the new leader
     */
    void setLeader(const std::string& leader_id);

    /**
     * Return true if the local peer is currently the leader.
     */
    bool isLocalLeader() const;

    // ------------------------------------------------------------------
    // Task Hand-off
    // ------------------------------------------------------------------

    /**
     * Initiate a task hand-off from the local peer to another peer.
     * @param task_id      Task being transferred
     * @param reason       Reason for the transfer (e.g. "overloaded")
     * @param target_peer  Target peer ID (empty = auto-select least loaded)
     * @return HandoffRequest with selected target and accepted flag
     */
    HandoffRequest initiateHandoff(const std::string& task_id,
                                   const std::string& reason,
                                   const std::string& target_peer = "");

    /**
     * Respond to an incoming hand-off request.
     * @param request_id  Request to respond to
     * @param accept      True to accept the task
     * @return true if the request was found and updated
     */
    bool respondToHandoff(const std::string& request_id, bool accept);

    /**
     * Get all pending (unresolved) hand-off requests.
     */
    std::vector<HandoffRequest> getPendingHandoffs() const;

    // ------------------------------------------------------------------
    // Lightweight Consensus
    // ------------------------------------------------------------------

    /**
     * Create a new binary proposal that peers can vote on.
     * @param description  Human-readable description
     * @param threshold    Fraction of votes needed to accept (0.0–1.0)
     * @return Proposal ID (empty on failure)
     */
    std::string createProposal(const std::string& description,
                               double threshold = 0.5);

    /**
     * Cast a vote on an open proposal.
     * @param proposal_id Proposal identifier
     * @param peer_id     Voting peer
     * @param vote        true = yes, false = no
     * @return true if the vote was recorded
     */
    bool castVote(const std::string& proposal_id,
                  const std::string& peer_id,
                  bool vote);

    /**
     * Tally votes and determine the consensus result.
     * @param proposal_id Proposal to evaluate
     * @return ConsensusResult (not accepted if proposal not found)
     */
    ConsensusResult tallyVotes(const std::string& proposal_id) const;

    // ------------------------------------------------------------------
    // Diagnostics
    // ------------------------------------------------------------------

    /**
     * Return the local peer ID.
     */
    std::string getLocalPeerId() const { return local_peer_id_; }

    /**
     * Return a snapshot of protocol metrics.
     * Keys: total_peers, active_peers, suspected_peers, dead_peers,
     *       pending_handoffs, open_proposals.
     */
    std::map<std::string, int> getMetrics() const;

private:
    AtomSpacePtr atomspace_;
    std::string  local_peer_id_;
    bool         initialized_;

    // Peer management
    std::map<std::string, ProtocolPeer> peers_;
    mutable std::mutex peers_mutex_;

    // Leader election
    std::string current_leader_;
    int         election_round_;
    mutable std::mutex leader_mutex_;

    // Hand-off requests
    std::map<std::string, HandoffRequest> handoffs_;
    mutable std::mutex handoffs_mutex_;

    // Consensus proposals: proposal_id -> {description, threshold, votes}
    struct Proposal {
        std::string              description;
        double                   threshold;
        std::map<std::string, bool> votes;
    };
    std::map<std::string, Proposal> proposals_;
    mutable std::mutex proposals_mutex_;

    // Liveness callback (protected by its own mutex to avoid recursive locks)
    std::function<void(const std::string&, PeerState)> liveness_cb_;
    mutable std::mutex cb_mutex_;

    // Mutex protecting the shared random-number generator in generateId
    mutable std::mutex rng_mutex_;

    // Internal helpers
    void  storePeerInAtomSpace(const ProtocolPeer& peer);
    void  updatePeerStateInAtomSpace(const std::string& peer_id, PeerState state);
    void  storeLeaderInAtomSpace(const std::string& leader_id);
    std::string generateId(const std::string& prefix) const;

    Logger logger_;
};

}} // namespace opencog::agentzero

#endif // _OPENCOG_AGENTZERO_COORDINATION_PROTOCOL_H
