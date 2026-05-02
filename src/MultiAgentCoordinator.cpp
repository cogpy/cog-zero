/*
 * opencog/agentzero/communication/MultiAgentCoordinator.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Multi-Agent Coordination Protocols Implementation
 * Part of the AGENT-ZERO-GENESIS project - AZ-MULTI-001
 */

#include <algorithm>
#include <sstream>
#include <random>

#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

#include "MultiAgentCoordinator.h"

using namespace opencog;
using namespace opencog::agentzero::communication;

// Utility functions
namespace {
    std::string generateId(const std::string& prefix) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        std::stringstream ss;
        ss << prefix << "_";
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        ss << time_t << "_";
        
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << dis(gen);
        }
        
        return ss.str();
    }
}

// ==============================================================
// Constructor and Destructor
// ==============================================================

MultiAgentCoordinator::MultiAgentCoordinator(
    AtomSpacePtr atomspace,
    std::shared_ptr<AgentComms> comms,
    const AgentId& coordinator_id)
    : _atomspace(atomspace)
    , _comms(comms)
    , _coordinator_id(coordinator_id)
    , _initialized(false)
    , _heartbeat_timeout(std::chrono::seconds(30))
    , _task_timeout(std::chrono::seconds(300))
{
    logger().info("[MultiAgentCoordinator] Created coordinator: %s", 
                  coordinator_id.toString().c_str());
}

MultiAgentCoordinator::~MultiAgentCoordinator() {
    shutdown();
}

// ==============================================================
// Initialization and Shutdown
// ==============================================================

bool MultiAgentCoordinator::initialize() {
    if (_initialized) {
        logger().warn("[MultiAgentCoordinator] Already initialized");
        return false;
    }
    
    if (!_atomspace) {
        logger().error("[MultiAgentCoordinator] AtomSpace not available");
        return false;
    }
    
    try {
        initializeAtomSpace();
        createCoordinationContextAtoms();
        
        _initialized = true;
        logger().info("[MultiAgentCoordinator] Initialized successfully");
        return true;
    }
    catch (const std::exception& e) {
        logger().error("[MultiAgentCoordinator] Initialization failed: %s", e.what());
        return false;
    }
}

void MultiAgentCoordinator::shutdown() {
    if (!_initialized) {
        return;
    }
    
    logger().info("[MultiAgentCoordinator] Shutting down");
    
    // Clear all data structures
    {
        std::lock_guard<std::mutex> lock(_agents_mutex);
        _registered_agents.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(_tasks_mutex);
        _tasks.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(_proposals_mutex);
        _proposals.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(_resources_mutex);
        _resource_requests.clear();
        _available_resources.clear();
    }
    
    _initialized = false;
    logger().info("[MultiAgentCoordinator] Shutdown complete");
}

// ==============================================================
// Internal Methods
// ==============================================================

void MultiAgentCoordinator::initializeAtomSpace() {
    // Create root coordination context node
    Handle coord_root = _atomspace->add_node(CONCEPT_NODE, "CoordinationContext");
    logger().debug("[MultiAgentCoordinator] Created coordination context in AtomSpace");
}

void MultiAgentCoordinator::createCoordinationContextAtoms() {
    // Create coordinator agent node
    std::string coord_name = "Coordinator:" + _coordinator_id.toString();
    Handle coord_node = _atomspace->add_node(CONCEPT_NODE, coord_name);
    
    // Create context nodes
    Handle agents_node = _atomspace->add_node(CONCEPT_NODE, "RegisteredAgents");
    Handle tasks_node = _atomspace->add_node(CONCEPT_NODE, "CoordinationTasks");
    Handle proposals_node = _atomspace->add_node(CONCEPT_NODE, "ConsensusProposals");
    
    logger().debug("[MultiAgentCoordinator] Created coordination context atoms");
}

void MultiAgentCoordinator::updateAgentAtom(const AgentRegistration& agent) {
    std::string agent_name = "Agent:" + agent.agent_id.toString();
    Handle agent_node = _atomspace->add_node(CONCEPT_NODE, agent_name);
    
    // Store agent status as an evaluation
    Handle status_pred = _atomspace->add_node(PREDICATE_NODE, "status");
    Handle status_value = _atomspace->add_node(CONCEPT_NODE, agent.status);
    
    HandleSeq list_seq = {agent_node, status_value};
    Handle list_link = _atomspace->add_link(LIST_LINK, list_seq);
    
    HandleSeq eval_seq = {status_pred, list_link};
    _atomspace->add_link(EVALUATION_LINK, eval_seq);
}

void MultiAgentCoordinator::updateTaskAtom(const CoordinationTask& task) {
    std::string task_name = "Task:" + task.task_id;
    Handle task_node = _atomspace->add_node(CONCEPT_NODE, task_name);
    
    // Store task in AtomSpace
    task.task_atom = task_node;
    
    // Add task status
    Handle status_pred = _atomspace->add_node(PREDICATE_NODE, "task_status");
    Handle status_value = _atomspace->add_node(CONCEPT_NODE, task.status);
    
    HandleSeq list_seq = {task_node, status_value};
    Handle list_link = _atomspace->add_link(LIST_LINK, list_seq);
    
    HandleSeq eval_seq = {status_pred, list_link};
    _atomspace->add_link(EVALUATION_LINK, eval_seq);
}

void MultiAgentCoordinator::updateProposalAtom(const ConsensusProposal& proposal) {
    std::string proposal_name = "Proposal:" + proposal.proposal_id;
    Handle proposal_node = _atomspace->add_node(CONCEPT_NODE, proposal_name);
    
    // Store proposal status
    Handle status_pred = _atomspace->add_node(PREDICATE_NODE, "proposal_status");
    Handle status_value = _atomspace->add_node(CONCEPT_NODE, proposal.status);
    
    HandleSeq list_seq = {proposal_node, status_value};
    Handle list_link = _atomspace->add_link(LIST_LINK, list_seq);
    
    HandleSeq eval_seq = {status_pred, list_link};
    _atomspace->add_link(EVALUATION_LINK, eval_seq);
}

void MultiAgentCoordinator::checkAgentHeartbeats() {
    std::lock_guard<std::mutex> lock(_agents_mutex);
    auto now = std::chrono::system_clock::now();
    
    for (auto& pair : _registered_agents) {
        auto& agent = pair.second;
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - agent.last_heartbeat);
        
        if (elapsed > _heartbeat_timeout && agent.status != "offline") {
            logger().warn("[MultiAgentCoordinator] Agent %s heartbeat timeout",
                         agent.agent_id.toString().c_str());
            agent.status = "offline";
            updateAgentAtom(agent);
        }
    }
}

void MultiAgentCoordinator::checkTaskDeadlines() {
    std::lock_guard<std::mutex> lock(_tasks_mutex);
    auto now = std::chrono::system_clock::now();
    
    for (auto& pair : _tasks) {
        auto& task = pair.second;
        if (task.status == "in_progress" && 
            task.deadline != std::chrono::system_clock::time_point() &&
            now > task.deadline) {
            logger().warn("[MultiAgentCoordinator] Task %s deadline exceeded",
                         task.task_id.c_str());
            task.status = "failed";
            updateTaskAtom(task);
        }
    }
}

void MultiAgentCoordinator::checkVotingDeadlines() {
    std::lock_guard<std::mutex> lock(_proposals_mutex);
    auto now = std::chrono::system_clock::now();
    
    for (auto& pair : _proposals) {
        auto& proposal = pair.second;
        if (proposal.status == "voting" && now > proposal.voting_deadline) {
            // Tally votes
            size_t agree_count = 0;
            for (const auto& vote : proposal.votes) {
                if (vote.second) agree_count++;
            }
            
            double vote_ratio = proposal.votes.empty() ? 0.0 :
                static_cast<double>(agree_count) / proposal.votes.size();
            
            proposal.status = (vote_ratio >= proposal.threshold) ? "accepted" : "rejected";
            updateProposalAtom(proposal);
            
            logger().info("[MultiAgentCoordinator] Proposal %s %s (%.2f%% agreement)",
                         proposal.proposal_id.c_str(), proposal.status.c_str(),
                         vote_ratio * 100.0);
        }
    }
}

std::vector<AgentId> MultiAgentCoordinator::findCapableAgents(
    const std::vector<std::string>& capabilities) {
    std::lock_guard<std::mutex> lock(_agents_mutex);
    std::vector<AgentId> capable_agents;
    
    for (const auto& pair : _registered_agents) {
        const auto& agent = pair.second;
        if (agent.status != "active") continue;
        
        // Check if agent has all required capabilities
        bool has_all = true;
        for (const auto& required_cap : capabilities) {
            bool has_cap = false;
            for (const auto& agent_cap : agent.capabilities) {
                if (agent_cap.capability_name == required_cap) {
                    has_cap = true;
                    break;
                }
            }
            if (!has_cap) {
                has_all = false;
                break;
            }
        }
        
        if (has_all) {
            capable_agents.push_back(agent.agent_id);
        }
    }
    
    return capable_agents;
}

AgentId MultiAgentCoordinator::selectBestAgent(
    const std::vector<AgentId>& candidates,
    const CoordinationTask& task) {
    if (candidates.empty()) {
        return AgentId();
    }
    
    std::lock_guard<std::mutex> lock(_agents_mutex);
    
    AgentId best_agent;
    double best_score = -1.0;
    
    for (const auto& candidate : candidates) {
        auto it = _registered_agents.find(candidate.toString());
        if (it == _registered_agents.end()) continue;
        
        double score = calculateAgentScore(it->second, task);
        if (score > best_score) {
            best_score = score;
            best_agent = candidate;
        }
    }
    
    return best_agent;
}

double MultiAgentCoordinator::calculateAgentScore(
    const AgentRegistration& agent,
    const CoordinationTask& task) {
    // Simple scoring: lower load is better, higher proficiency is better
    double load_penalty = agent.load_factor;
    double proficiency_bonus = 0.0;
    
    for (const auto& required_cap : task.required_capabilities) {
        for (const auto& agent_cap : agent.capabilities) {
            if (agent_cap.capability_name == required_cap) {
                proficiency_bonus += agent_cap.proficiency;
                break;
            }
        }
    }
    
    if (!task.required_capabilities.empty()) {
        proficiency_bonus /= task.required_capabilities.size();
    }
    
    return proficiency_bonus * (1.0 - load_penalty);
}

void MultiAgentCoordinator::handleConflict(
    const std::string& conflict_type,
    const std::vector<AgentId>& involved_agents) {
    logger().info("[MultiAgentCoordinator] Handling conflict: %s", 
                  conflict_type.c_str());
    
    std::lock_guard<std::mutex> lock(_stats_mutex);
    _stats.total_conflicts++;
    
    // Simple conflict resolution: notify all involved agents
    if (_comms) {
        for (const auto& agent : involved_agents) {
            _comms->sendMessage(
                agent,
                MessageType::NOTIFICATION,
                "Conflict detected: " + conflict_type,
                MessagePriority::HIGH,
                ProtocolType::LOCAL
            );
        }
    }
    
    _stats.resolved_conflicts++;
}

// ==============================================================
// Agent Registration and Discovery
// ==============================================================

bool MultiAgentCoordinator::registerAgent(
    const AgentId& agent_id,
    const std::vector<AgentCapability>& capabilities) {
    if (!_initialized) {
        logger().error("[MultiAgentCoordinator] Not initialized");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(_agents_mutex);
    
    std::string key = agent_id.toString();
    if (_registered_agents.find(key) != _registered_agents.end()) {
        logger().warn("[MultiAgentCoordinator] Agent %s already registered", key.c_str());
        return false;
    }
    
    AgentRegistration registration;
    registration.agent_id = agent_id;
    registration.capabilities = capabilities;
    registration.status = "active";
    registration.load_factor = 0.0;
    registration.last_heartbeat = std::chrono::system_clock::now();
    
    _registered_agents[key] = registration;
    updateAgentAtom(registration);
    
    {
        std::lock_guard<std::mutex> stats_lock(_stats_mutex);
        _stats.total_agents++;
        _stats.active_agents++;
    }
    
    logger().info("[MultiAgentCoordinator] Registered agent: %s", key.c_str());
    return true;
}

bool MultiAgentCoordinator::unregisterAgent(const AgentId& agent_id) {
    if (!_initialized) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(_agents_mutex);
    
    std::string key = agent_id.toString();
    auto it = _registered_agents.find(key);
    if (it == _registered_agents.end()) {
        return false;
    }
    
    _registered_agents.erase(it);
    
    {
        std::lock_guard<std::mutex> stats_lock(_stats_mutex);
        _stats.total_agents--;
        if (_stats.active_agents > 0) {
            _stats.active_agents--;
        }
    }
    
    logger().info("[MultiAgentCoordinator] Unregistered agent: %s", key.c_str());
    return true;
}

bool MultiAgentCoordinator::updateAgentStatus(
    const AgentId& agent_id,
    const std::string& status,
    double load_factor) {
    if (!_initialized) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(_agents_mutex);
    
    std::string key = agent_id.toString();
    auto it = _registered_agents.find(key);
    if (it == _registered_agents.end()) {
        return false;
    }
    
    bool was_active = (it->second.status == "active");
    it->second.status = status;
    it->second.load_factor = load_factor;
    updateAgentAtom(it->second);
    
    bool is_active = (status == "active");
    if (was_active && !is_active) {
        std::lock_guard<std::mutex> stats_lock(_stats_mutex);
        if (_stats.active_agents > 0) {
            _stats.active_agents--;
        }
    } else if (!was_active && is_active) {
        std::lock_guard<std::mutex> stats_lock(_stats_mutex);
        _stats.active_agents++;
    }
    
    return true;
}

bool MultiAgentCoordinator::recordHeartbeat(const AgentId& agent_id) {
    if (!_initialized) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(_agents_mutex);
    
    std::string key = agent_id.toString();
    auto it = _registered_agents.find(key);
    if (it == _registered_agents.end()) {
        return false;
    }
    
    it->second.last_heartbeat = std::chrono::system_clock::now();
    return true;
}

std::vector<AgentId> MultiAgentCoordinator::getRegisteredAgents() const {
    std::lock_guard<std::mutex> lock(_agents_mutex);
    std::vector<AgentId> agents;
    
    for (const auto& pair : _registered_agents) {
        agents.push_back(pair.second.agent_id);
    }
    
    return agents;
}

std::shared_ptr<AgentRegistration> MultiAgentCoordinator::getAgentInfo(
    const AgentId& agent_id) const {
    std::lock_guard<std::mutex> lock(_agents_mutex);
    
    std::string key = agent_id.toString();
    auto it = _registered_agents.find(key);
    if (it == _registered_agents.end()) {
        return nullptr;
    }
    
    return std::make_shared<AgentRegistration>(it->second);
}

std::vector<AgentId> MultiAgentCoordinator::findAgentsByCapability(
    const std::string& capability) const {
    std::lock_guard<std::mutex> lock(_agents_mutex);
    std::vector<AgentId> agents;
    
    for (const auto& pair : _registered_agents) {
        const auto& agent = pair.second;
        for (const auto& cap : agent.capabilities) {
            if (cap.capability_name == capability) {
                agents.push_back(agent.agent_id);
                break;
            }
        }
    }
    
    return agents;
}

// ==============================================================
// Task Delegation and Distribution
// ==============================================================

std::string MultiAgentCoordinator::createTask(
    const std::string& description,
    const std::vector<std::string>& required_capabilities,
    MessagePriority priority) {
    if (!_initialized) {
        return "";
    }
    
    std::string task_id = generateId("task");
    
    CoordinationTask task;
    task.task_id = task_id;
    task.description = description;
    task.required_capabilities = required_capabilities;
    task.priority = priority;
    task.status = "pending";
    task.created = std::chrono::system_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(_tasks_mutex);
        _tasks[task_id] = task;
        updateTaskAtom(task);
    }
    
    {
        std::lock_guard<std::mutex> lock(_stats_mutex);
        _stats.total_tasks++;
    }
    
    logger().info("[MultiAgentCoordinator] Created task: %s", task_id.c_str());
    return task_id;
}

bool MultiAgentCoordinator::assignTask(
    const std::string& task_id,
    const AgentId& agent_id) {
    if (!_initialized) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(_tasks_mutex);
    
    auto it = _tasks.find(task_id);
    if (it == _tasks.end()) {
        return false;
    }
    
    AgentId target_agent = agent_id;
    
    // Auto-assign if no agent specified
    if (target_agent.name.empty()) {
        auto candidates = findCapableAgents(it->second.required_capabilities);
        target_agent = selectBestAgent(candidates, it->second);
        
        if (target_agent.name.empty()) {
            logger().warn("[MultiAgentCoordinator] No capable agent found for task %s",
                         task_id.c_str());
            return false;
        }
    }
    
    it->second.assigned_to = target_agent;
    it->second.status = "assigned";
    updateTaskAtom(it->second);
    
    // Notify agent via communication system
    if (_comms) {
        _comms->sendMessage(
            target_agent,
            MessageType::TASK_ASSIGNMENT,
            "Task assigned: " + it->second.description,
            it->second.priority,
            ProtocolType::LOCAL
        );
    }
    
    logger().info("[MultiAgentCoordinator] Assigned task %s to agent %s",
                 task_id.c_str(), target_agent.toString().c_str());
    return true;
}

bool MultiAgentCoordinator::updateTaskStatus(
    const std::string& task_id,
    const std::string& status) {
    if (!_initialized) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(_tasks_mutex);
    
    auto it = _tasks.find(task_id);
    if (it == _tasks.end()) {
        return false;
    }
    
    it->second.status = status;
    updateTaskAtom(it->second);
    
    return true;
}

bool MultiAgentCoordinator::completeTask(const std::string& task_id) {
    if (!_initialized) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(_tasks_mutex);
    
    auto it = _tasks.find(task_id);
    if (it == _tasks.end()) {
        return false;
    }
    
    it->second.status = "completed";
    updateTaskAtom(it->second);
    
    {
        std::lock_guard<std::mutex> stats_lock(_stats_mutex);
        _stats.completed_tasks++;
    }
    
    logger().info("[MultiAgentCoordinator] Completed task: %s", task_id.c_str());
    return true;
}

std::shared_ptr<CoordinationTask> MultiAgentCoordinator::getTaskInfo(
    const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(_tasks_mutex);
    
    auto it = _tasks.find(task_id);
    if (it == _tasks.end()) {
        return nullptr;
    }
    
    return std::make_shared<CoordinationTask>(it->second);
}

std::vector<std::string> MultiAgentCoordinator::getTasksByStatus(
    const std::string& status) const {
    std::lock_guard<std::mutex> lock(_tasks_mutex);
    std::vector<std::string> task_ids;
    
    for (const auto& pair : _tasks) {
        if (pair.second.status == status) {
            task_ids.push_back(pair.first);
        }
    }
    
    return task_ids;
}

std::vector<std::string> MultiAgentCoordinator::getAgentTasks(
    const AgentId& agent_id) const {
    std::lock_guard<std::mutex> lock(_tasks_mutex);
    std::vector<std::string> task_ids;
    
    for (const auto& pair : _tasks) {
        if (pair.second.assigned_to == agent_id) {
            task_ids.push_back(pair.first);
        }
    }
    
    return task_ids;
}

// ==============================================================
// Consensus Mechanisms
// ==============================================================

std::string MultiAgentCoordinator::createProposal(
    const std::string& description,
    double threshold,
    std::chrono::seconds voting_duration) {
    if (!_initialized) {
        return "";
    }
    
    std::string proposal_id = generateId("proposal");
    
    ConsensusProposal proposal;
    proposal.proposal_id = proposal_id;
    proposal.description = description;
    proposal.proposer = _coordinator_id;
    proposal.threshold = threshold;
    proposal.status = "voting";
    proposal.created = std::chrono::system_clock::now();
    proposal.voting_deadline = proposal.created + voting_duration;
    
    {
        std::lock_guard<std::mutex> lock(_proposals_mutex);
        _proposals[proposal_id] = proposal;
        updateProposalAtom(proposal);
    }
    
    {
        std::lock_guard<std::mutex> lock(_stats_mutex);
        _stats.total_proposals++;
    }
    
    logger().info("[MultiAgentCoordinator] Created proposal: %s", proposal_id.c_str());
    return proposal_id;
}

bool MultiAgentCoordinator::castVote(
    const std::string& proposal_id,
    const AgentId& agent_id,
    bool vote) {
    if (!_initialized) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(_proposals_mutex);
    
    auto it = _proposals.find(proposal_id);
    if (it == _proposals.end()) {
        return false;
    }
    
    if (it->second.status != "voting") {
        logger().warn("[MultiAgentCoordinator] Proposal %s not in voting state",
                     proposal_id.c_str());
        return false;
    }
    
    it->second.votes[agent_id] = vote;
    updateProposalAtom(it->second);
    
    logger().info("[MultiAgentCoordinator] Agent %s voted %s on proposal %s",
                 agent_id.toString().c_str(), vote ? "YES" : "NO", 
                 proposal_id.c_str());
    return true;
}

std::shared_ptr<ConsensusProposal> MultiAgentCoordinator::getProposalInfo(
    const std::string& proposal_id) const {
    std::lock_guard<std::mutex> lock(_proposals_mutex);
    
    auto it = _proposals.find(proposal_id);
    if (it == _proposals.end()) {
        return nullptr;
    }
    
    return std::make_shared<ConsensusProposal>(it->second);
}

bool MultiAgentCoordinator::hasReachedConsensus(
    const std::string& proposal_id) const {
    std::lock_guard<std::mutex> lock(_proposals_mutex);
    
    auto it = _proposals.find(proposal_id);
    if (it == _proposals.end()) {
        return false;
    }
    
    return it->second.status == "accepted";
}

// ==============================================================
// Conflict Resolution
// ==============================================================

std::vector<std::string> MultiAgentCoordinator::detectConflicts() const {
    std::vector<std::string> conflicts;
    
    // Simple conflict detection: check for task assignment conflicts
    std::lock_guard<std::mutex> lock(_tasks_mutex);
    
    std::map<std::string, int> agent_task_count;
    for (const auto& pair : _tasks) {
        if (pair.second.status == "assigned" || pair.second.status == "in_progress") {
            std::string agent_key = pair.second.assigned_to.toString();
            agent_task_count[agent_key]++;
        }
    }
    
    for (const auto& pair : agent_task_count) {
        if (pair.second > 5) { // Arbitrary threshold
            conflicts.push_back("Agent " + pair.first + " overloaded with " + 
                              std::to_string(pair.second) + " tasks");
        }
    }
    
    return conflicts;
}

bool MultiAgentCoordinator::resolveConflict(
    const std::string& conflict_id,
    const std::string& resolution) {
    if (!_initialized) {
        return false;
    }
    
    logger().info("[MultiAgentCoordinator] Resolving conflict %s: %s",
                 conflict_id.c_str(), resolution.c_str());
    
    std::lock_guard<std::mutex> lock(_stats_mutex);
    _stats.resolved_conflicts++;
    
    return true;
}

// ==============================================================
// Resource Allocation
// ==============================================================

bool MultiAgentCoordinator::registerResource(
    const std::string& resource_type,
    double amount) {
    if (!_initialized) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(_resources_mutex);
    _available_resources[resource_type] = amount;
    
    logger().info("[MultiAgentCoordinator] Registered resource: %s = %.2f",
                 resource_type.c_str(), amount);
    return true;
}

std::string MultiAgentCoordinator::requestResource(
    const AgentId& requester,
    const std::string& resource_type,
    double amount,
    MessagePriority priority) {
    if (!_initialized) {
        return "";
    }
    
    std::lock_guard<std::mutex> lock(_resources_mutex);
    
    // Check if resource available
    auto it = _available_resources.find(resource_type);
    if (it == _available_resources.end() || it->second < amount) {
        logger().warn("[MultiAgentCoordinator] Insufficient resource: %s",
                     resource_type.c_str());
        return "";
    }
    
    std::string request_id = generateId("resource");
    
    ResourceRequest request;
    request.request_id = request_id;
    request.requester = requester;
    request.resource_type = resource_type;
    request.amount = amount;
    request.priority = priority;
    request.status = "granted";
    request.created = std::chrono::system_clock::now();
    
    _resource_requests[request_id] = request;
    it->second -= amount;
    
    logger().info("[MultiAgentCoordinator] Granted resource request %s: %s = %.2f",
                 request_id.c_str(), resource_type.c_str(), amount);
    return request_id;
}

bool MultiAgentCoordinator::releaseResource(const std::string& request_id) {
    if (!_initialized) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(_resources_mutex);
    
    auto it = _resource_requests.find(request_id);
    if (it == _resource_requests.end()) {
        return false;
    }
    
    // Return resource to available pool
    _available_resources[it->second.resource_type] += it->second.amount;
    _resource_requests.erase(it);
    
    logger().info("[MultiAgentCoordinator] Released resource request: %s",
                 request_id.c_str());
    return true;
}

double MultiAgentCoordinator::getAvailableResource(
    const std::string& resource_type) const {
    std::lock_guard<std::mutex> lock(_resources_mutex);
    
    auto it = _available_resources.find(resource_type);
    if (it == _available_resources.end()) {
        return 0.0;
    }
    
    return it->second;
}

// ==============================================================
// State Synchronization
// ==============================================================

bool MultiAgentCoordinator::synchronizeState(
    const std::vector<AgentId>& agents) {
    if (!_initialized || !_comms) {
        return false;
    }
    
    logger().info("[MultiAgentCoordinator] Synchronizing state for %zu agents",
                 agents.size());
    
    // Send synchronization message to all agents
    for (const auto& agent : agents) {
        _comms->sendMessage(
            agent,
            MessageType::NOTIFICATION,
            "State synchronization initiated",
            MessagePriority::NORMAL,
            ProtocolType::LOCAL
        );
    }
    
    return true;
}

size_t MultiAgentCoordinator::broadcastStateUpdate(
    const std::string& update_type,
    const std::string& update_data) {
    if (!_initialized || !_comms) {
        return 0;
    }
    
    auto agents = getRegisteredAgents();
    
    logger().info("[MultiAgentCoordinator] Broadcasting state update to %zu agents",
                 agents.size());
    
    return _comms->broadcastMessage(
        agents,
        MessageType::NOTIFICATION,
        update_type + ": " + update_data,
        MessagePriority::NORMAL
    );
}

// ==============================================================
// Monitoring and Statistics
// ==============================================================

CoordinationStats MultiAgentCoordinator::getStatistics() const {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    return _stats;
}

size_t MultiAgentCoordinator::getActiveAgentsCount() const {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    return _stats.active_agents;
}

size_t MultiAgentCoordinator::getPendingTasksCount() const {
    std::lock_guard<std::mutex> lock(_tasks_mutex);
    size_t count = 0;
    
    for (const auto& pair : _tasks) {
        if (pair.second.status == "pending") {
            count++;
        }
    }
    
    return count;
}

bool MultiAgentCoordinator::isHealthy() const {
    if (!_initialized) {
        return false;
    }
    
    // System is healthy if we have at least one active agent
    std::lock_guard<std::mutex> lock(_stats_mutex);
    return _stats.active_agents > 0;
}
