/**
 * CoordinationProtocol.cpp
 *
 * Implementation of multi-agent coordination protocols for Agent-Zero.
 * Part of the AGENT-ZERO-GENESIS project (AZ-SCALE-001)
 */

#include <opencog/agentzero/distributed/CoordinationProtocol.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>

#include <algorithm>
#include <chrono>
#include <random>
#include <sstream>

using namespace opencog;
using namespace opencog::agentzero;

// -----------------------------------------------------------------------
// Constructor / Destructor
// -----------------------------------------------------------------------

CoordinationProtocol::CoordinationProtocol(AtomSpacePtr atomspace,
                                           const std::string& local_peer_id)
    : atomspace_(atomspace),
      local_peer_id_(local_peer_id),
      initialized_(false),
      election_round_(0),
      logger_("CoordinationProtocol")
{
    logger_.info("CoordinationProtocol created for peer: %s",
                 local_peer_id.c_str());
}

CoordinationProtocol::~CoordinationProtocol()
{
    shutdown();
}

// -----------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------

bool CoordinationProtocol::initialize()
{
    if (initialized_) {
        logger_.warn("CoordinationProtocol already initialized");
        return true;
    }

    logger_.info("Initializing CoordinationProtocol for peer: %s",
                 local_peer_id_.c_str());

    // Register the local peer
    ProtocolPeer local(local_peer_id_, "localhost", 0);
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        peers_[local_peer_id_] = local;
        storePeerInAtomSpace(local);
    }

    // Seed AtomSpace with coordination context
    Handle ctx = atomspace_->add_node(CONCEPT_NODE,
                                      "CoordProtocol:" + local_peer_id_);
    Handle role = atomspace_->add_node(CONCEPT_NODE, "DistributedCoordinationProtocol");
    atomspace_->add_link(INHERITANCE_LINK, {ctx, role});

    initialized_ = true;
    logger_.info("CoordinationProtocol initialized");
    return true;
}

void CoordinationProtocol::shutdown()
{
    if (!initialized_) return;

    logger_.info("Shutting down CoordinationProtocol");

    // Mark local peer as DEAD in AtomSpace
    updatePeerStateInAtomSpace(local_peer_id_, PeerState::DEAD);

    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        peers_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(handoffs_mutex_);
        handoffs_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(proposals_mutex_);
        proposals_.clear();
    }

    initialized_ = false;
}

// -----------------------------------------------------------------------
// Agent Discovery
// -----------------------------------------------------------------------

bool CoordinationProtocol::registerPeer(const std::string& peer_id,
                                        const std::string& address,
                                        int port)
{
    std::lock_guard<std::mutex> lock(peers_mutex_);

    if (peers_.find(peer_id) != peers_.end()) {
        logger_.warn("Peer %s already registered", peer_id.c_str());
        return false;
    }

    ProtocolPeer peer(peer_id, address, port);
    peers_[peer_id] = peer;
    storePeerInAtomSpace(peer);

    logger_.info("Registered peer %s at %s:%d",
                 peer_id.c_str(), address.c_str(), port);
    return true;
}

bool CoordinationProtocol::deregisterPeer(const std::string& peer_id)
{
    std::lock_guard<std::mutex> lock(peers_mutex_);

    auto it = peers_.find(peer_id);
    if (it == peers_.end()) {
        logger_.warn("Peer %s not found for deregistration", peer_id.c_str());
        return false;
    }

    updatePeerStateInAtomSpace(peer_id, PeerState::DEAD);
    peers_.erase(it);

    logger_.info("Deregistered peer %s", peer_id.c_str());
    return true;
}

std::vector<ProtocolPeer> CoordinationProtocol::getKnownPeers() const
{
    std::lock_guard<std::mutex> lock(peers_mutex_);

    std::vector<ProtocolPeer> result;
    result.reserve(peers_.size());
    for (const auto& kv : peers_) {
        result.push_back(kv.second);
    }
    return result;
}

std::vector<std::string> CoordinationProtocol::getActivePeers() const
{
    std::lock_guard<std::mutex> lock(peers_mutex_);

    std::vector<std::string> active;
    for (const auto& kv : peers_) {
        if (kv.second.state == PeerState::ACTIVE) {
            active.push_back(kv.first);
        }
    }
    return active;
}

PeerState CoordinationProtocol::getPeerState(const std::string& peer_id) const
{
    std::lock_guard<std::mutex> lock(peers_mutex_);

    auto it = peers_.find(peer_id);
    if (it == peers_.end()) {
        return PeerState::DEAD;
    }
    return it->second.state;
}

// -----------------------------------------------------------------------
// Heartbeat / Liveness
// -----------------------------------------------------------------------

bool CoordinationProtocol::recordHeartbeat(const std::string& peer_id)
{
    bool fire_callback = false;
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);

        auto it = peers_.find(peer_id);
        if (it == peers_.end()) {
            logger_.warn("Heartbeat from unknown peer %s", peer_id.c_str());
            return false;
        }

        ProtocolPeer& peer = it->second;
        peer.last_seen        = std::chrono::system_clock::now();
        peer.missed_heartbeats = 0;

        if (peer.state != PeerState::ACTIVE) {
            logger_.info("Peer %s recovered (state -> ACTIVE)", peer_id.c_str());
            peer.state   = PeerState::ACTIVE;
            fire_callback = true;
            updatePeerStateInAtomSpace(peer_id, PeerState::ACTIVE);
        }
    }

    if (fire_callback) {
        std::function<void(const std::string&, PeerState)> cb;
        {
            std::lock_guard<std::mutex> lock(cb_mutex_);
            cb = liveness_cb_;
        }
        if (cb) cb(peer_id, PeerState::ACTIVE);
    }

    return true;
}

std::vector<std::string> CoordinationProtocol::tickHeartbeat(int missed_threshold,
                                                              int dead_threshold)
{
    // Collect state changes under the peer lock, then fire callbacks outside it
    struct StateChange { std::string id; PeerState new_state; };
    std::vector<StateChange> changes;
    std::vector<std::string> newly_dead;

    {
        std::lock_guard<std::mutex> lock(peers_mutex_);

        for (auto& kv : peers_) {
            const std::string& id  = kv.first;
            ProtocolPeer&      peer = kv.second;

            // Local peer never times out
            if (id == local_peer_id_) continue;

            peer.missed_heartbeats++;

            PeerState old_state = peer.state;
            PeerState new_state = peer.state;

            if (peer.missed_heartbeats >= dead_threshold) {
                new_state = PeerState::DEAD;
            } else if (peer.missed_heartbeats >= missed_threshold) {
                new_state = PeerState::SUSPECTED;
            }

            if (new_state != old_state) {
                peer.state = new_state;
                updatePeerStateInAtomSpace(id, new_state);

                logger_.info("Peer %s state: %d -> %d (missed=%d)",
                             id.c_str(),
                             static_cast<int>(old_state),
                             static_cast<int>(new_state),
                             peer.missed_heartbeats);

                changes.push_back({id, new_state});

                if (new_state == PeerState::DEAD) {
                    newly_dead.push_back(id);
                }
            }
        }
    }

    // Fire callbacks outside the peer lock to prevent deadlocks
    if (!changes.empty()) {
        std::function<void(const std::string&, PeerState)> cb;
        {
            std::lock_guard<std::mutex> lock(cb_mutex_);
            cb = liveness_cb_;
        }
        if (cb) {
            for (const auto& change : changes) {
                cb(change.id, change.new_state);
            }
        }
    }

    return newly_dead;
}

void CoordinationProtocol::setLivenessCallback(
    std::function<void(const std::string&, PeerState)> cb)
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    liveness_cb_ = std::move(cb);
}

// -----------------------------------------------------------------------
// Leader Election  (bully-style, smallest ID wins)
// -----------------------------------------------------------------------

ElectionResult CoordinationProtocol::electLeader()
{
    ElectionResult result;

    std::vector<std::string> candidates;
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        for (const auto& kv : peers_) {
            if (kv.second.state == PeerState::ACTIVE) {
                candidates.push_back(kv.first);
            }
        }
    }

    if (candidates.empty()) {
        logger_.warn("No active peers for leader election");
        return result;
    }

    // Smallest lexicographic ID wins
    std::sort(candidates.begin(), candidates.end());
    const std::string& winner = candidates.front();

    {
        std::lock_guard<std::mutex> lock(leader_mutex_);
        election_round_++;
        current_leader_ = winner;
        result.leader_id = winner;
        result.success   = true;
        result.round     = election_round_;
    }

    storeLeaderInAtomSpace(winner);

    logger_.info("Leader elected: %s (round %d)", winner.c_str(), result.round);
    return result;
}

std::string CoordinationProtocol::getCurrentLeader() const
{
    std::lock_guard<std::mutex> lock(leader_mutex_);
    return current_leader_;
}

void CoordinationProtocol::setLeader(const std::string& leader_id)
{
    {
        std::lock_guard<std::mutex> lock(leader_mutex_);
        current_leader_ = leader_id;
    }
    storeLeaderInAtomSpace(leader_id);
    logger_.info("Leader set externally: %s", leader_id.c_str());
}

bool CoordinationProtocol::isLocalLeader() const
{
    std::lock_guard<std::mutex> lock(leader_mutex_);
    return !current_leader_.empty() && current_leader_ == local_peer_id_;
}

// -----------------------------------------------------------------------
// Task Hand-off
// -----------------------------------------------------------------------

HandoffRequest CoordinationProtocol::initiateHandoff(const std::string& task_id,
                                                      const std::string& reason,
                                                      const std::string& target_peer)
{
    HandoffRequest req;
    req.request_id  = generateId("handoff");
    req.task_id     = task_id;
    req.source_peer = local_peer_id_;
    req.reason      = reason;
    req.accepted    = false;

    // Select target: use provided ID, or pick first other active peer
    if (!target_peer.empty()) {
        req.target_peer = target_peer;
    } else {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        for (const auto& kv : peers_) {
            if (kv.first != local_peer_id_ &&
                kv.second.state == PeerState::ACTIVE)
            {
                req.target_peer = kv.first;
                break;
            }
        }
    }

    if (req.target_peer.empty()) {
        logger_.warn("No available peer for hand-off of task %s", task_id.c_str());
        return req;
    }

    {
        std::lock_guard<std::mutex> lock(handoffs_mutex_);
        handoffs_[req.request_id] = req;
    }

    logger_.info("Initiated hand-off %s: task %s -> peer %s (%s)",
                 req.request_id.c_str(), task_id.c_str(),
                 req.target_peer.c_str(), reason.c_str());

    return req;
}

bool CoordinationProtocol::respondToHandoff(const std::string& request_id,
                                             bool accept)
{
    std::lock_guard<std::mutex> lock(handoffs_mutex_);

    auto it = handoffs_.find(request_id);
    if (it == handoffs_.end()) {
        logger_.warn("Hand-off request %s not found", request_id.c_str());
        return false;
    }

    if (accept) {
        // Remove accepted handoffs immediately — they are complete
        handoffs_.erase(it);
        logger_.info("Hand-off %s accepted and removed", request_id.c_str());
    } else {
        it->second.accepted = false;
        logger_.info("Hand-off %s rejected", request_id.c_str());
    }

    return true;
}

std::vector<HandoffRequest> CoordinationProtocol::getPendingHandoffs() const
{
    std::lock_guard<std::mutex> lock(handoffs_mutex_);

    std::vector<HandoffRequest> pending;
    for (const auto& kv : handoffs_) {
        if (!kv.second.accepted) {
            pending.push_back(kv.second);
        }
    }
    return pending;
}

// -----------------------------------------------------------------------
// Lightweight Consensus
// -----------------------------------------------------------------------

std::string CoordinationProtocol::createProposal(const std::string& description,
                                                  double threshold)
{
    if (threshold < 0.0 || threshold > 1.0) {
        logger_.warn("Invalid threshold %.2f – clamping to [0,1]", threshold);
        threshold = std::max(0.0, std::min(1.0, threshold));
    }

    std::string id = generateId("proposal");
    Proposal p;
    p.description = description;
    p.threshold   = threshold;

    {
        std::lock_guard<std::mutex> lock(proposals_mutex_);
        proposals_[id] = std::move(p);
    }

    // Mirror in AtomSpace
    Handle prop_atom = atomspace_->add_node(CONCEPT_NODE, "Proposal:" + id);
    Handle desc_atom = atomspace_->add_node(CONCEPT_NODE, description);
    atomspace_->add_link(EVALUATION_LINK, {prop_atom, desc_atom});

    logger_.info("Created proposal %s: \"%s\" (threshold=%.2f)",
                 id.c_str(), description.c_str(), threshold);
    return id;
}

bool CoordinationProtocol::castVote(const std::string& proposal_id,
                                    const std::string& peer_id,
                                    bool vote)
{
    std::lock_guard<std::mutex> lock(proposals_mutex_);

    auto it = proposals_.find(proposal_id);
    if (it == proposals_.end()) {
        logger_.warn("Proposal %s not found", proposal_id.c_str());
        return false;
    }

    it->second.votes[peer_id] = vote;

    logger_.info("Peer %s voted %s on proposal %s",
                 peer_id.c_str(), vote ? "yes" : "no", proposal_id.c_str());
    return true;
}

ConsensusResult CoordinationProtocol::tallyVotes(
    const std::string& proposal_id) const
{
    std::lock_guard<std::mutex> lock(proposals_mutex_);

    ConsensusResult result;
    result.proposal_id = proposal_id;

    auto it = proposals_.find(proposal_id);
    if (it == proposals_.end()) {
        logger_.warn("Proposal %s not found for tallying", proposal_id.c_str());
        return result;
    }

    const Proposal& p = it->second;
    for (const auto& kv : p.votes) {
        if (kv.second) {
            result.yes_votes++;
        } else {
            result.no_votes++;
        }
    }

    int total = result.yes_votes + result.no_votes;
    if (total > 0) {
        result.acceptance_ratio =
            static_cast<double>(result.yes_votes) / static_cast<double>(total);
        result.accepted = result.acceptance_ratio >= p.threshold;
    }

    logger_.info("Proposal %s tally: %d yes / %d no (ratio=%.2f, accepted=%s)",
                 proposal_id.c_str(),
                 result.yes_votes, result.no_votes,
                 result.acceptance_ratio,
                 result.accepted ? "true" : "false");

    return result;
}

// -----------------------------------------------------------------------
// Diagnostics
// -----------------------------------------------------------------------

std::map<std::string, int> CoordinationProtocol::getMetrics() const
{
    std::map<std::string, int> metrics;

    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        int active = 0, suspected = 0, dead = 0;
        for (const auto& kv : peers_) {
            switch (kv.second.state) {
                case PeerState::ACTIVE:    active++;    break;
                case PeerState::SUSPECTED: suspected++; break;
                case PeerState::DEAD:      dead++;      break;
            }
        }
        metrics["total_peers"]    = static_cast<int>(peers_.size());
        metrics["active_peers"]   = active;
        metrics["suspected_peers"] = suspected;
        metrics["dead_peers"]     = dead;
    }

    {
        std::lock_guard<std::mutex> lock(handoffs_mutex_);
        int pending = 0;
        for (const auto& kv : handoffs_) {
            if (!kv.second.accepted) pending++;
        }
        metrics["pending_handoffs"] = pending;
    }

    {
        std::lock_guard<std::mutex> lock(proposals_mutex_);
        metrics["open_proposals"] = static_cast<int>(proposals_.size());
    }

    return metrics;
}

// -----------------------------------------------------------------------
// Private helpers
// -----------------------------------------------------------------------

void CoordinationProtocol::storePeerInAtomSpace(const ProtocolPeer& peer)
{
    Handle peer_atom = atomspace_->add_node(CONCEPT_NODE,
                                            "Peer:" + peer.peer_id);
    Handle addr_atom = atomspace_->add_node(CONCEPT_NODE, peer.address);
    Handle net_pred  = atomspace_->add_node(PREDICATE_NODE, "network_address");
    atomspace_->add_link(EVALUATION_LINK, {net_pred, peer_atom, addr_atom});
}

void CoordinationProtocol::updatePeerStateInAtomSpace(const std::string& peer_id,
                                                       PeerState state)
{
    static const std::map<PeerState, std::string> state_names = {
        {PeerState::ACTIVE,    "active"},
        {PeerState::SUSPECTED, "suspected"},
        {PeerState::DEAD,      "dead"},
    };

    Handle peer_atom  = atomspace_->add_node(CONCEPT_NODE, "Peer:" + peer_id);
    Handle state_pred = atomspace_->add_node(PREDICATE_NODE, "peer_state");
    Handle state_atom = atomspace_->add_node(CONCEPT_NODE,
                                             state_names.at(state));
    atomspace_->add_link(EVALUATION_LINK, {state_pred, peer_atom, state_atom});
}

void CoordinationProtocol::storeLeaderInAtomSpace(const std::string& leader_id)
{
    Handle leader_atom = atomspace_->add_node(CONCEPT_NODE, "Peer:" + leader_id);
    Handle role_pred   = atomspace_->add_node(PREDICATE_NODE, "cluster_leader");
    Handle true_atom   = atomspace_->add_node(CONCEPT_NODE, "true");
    atomspace_->add_link(EVALUATION_LINK, {role_pred, leader_atom, true_atom});
}

std::string CoordinationProtocol::generateId(const std::string& prefix) const
{
    static std::mt19937                    gen(std::random_device{}());
    static std::uniform_int_distribution<> dis(100000, 999999);

    int value;
    {
        std::lock_guard<std::mutex> lock(rng_mutex_);
        value = dis(gen);
    }

    std::ostringstream oss;
    oss << prefix << "-" << local_peer_id_ << "-" << value;
    return oss.str();
}
