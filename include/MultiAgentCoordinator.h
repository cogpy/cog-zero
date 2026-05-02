/*
 * opencog/agentzero/communication/MultiAgentCoordinator.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Multi-Agent Coordination Protocols Implementation
 * Part of the AGENT-ZERO-GENESIS project - AZ-MULTI-001
 */

#ifndef _OPENCOG_AGENTZERO_MULTI_AGENT_COORDINATOR_H
#define _OPENCOG_AGENTZERO_MULTI_AGENT_COORDINATOR_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <functional>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#include "CommTypes.h"
#include "AgentComms.h"

namespace opencog {
namespace agentzero {
namespace communication {

// Forward declarations
class AgentComms;

/**
 * Agent capability descriptor
 */
struct AgentCapability {
    std::string capability_name;      // Name of the capability
    std::string description;          // Description
    double proficiency;               // Proficiency level (0.0-1.0)
    std::map<std::string, std::string> parameters; // Additional parameters
    
    AgentCapability() : proficiency(0.0) {}
    AgentCapability(const std::string& name, double prof = 1.0) 
        : capability_name(name), proficiency(prof) {}
};

/**
 * Agent registration information
 */
struct AgentRegistration {
    AgentId agent_id;                           // Agent identifier
    std::vector<AgentCapability> capabilities;  // Agent capabilities
    std::string status;                         // Current status (active, busy, idle, offline)
    double load_factor;                         // Current load (0.0-1.0)
    std::chrono::system_clock::time_point last_heartbeat; // Last heartbeat time
    std::map<std::string, std::string> metadata; // Additional metadata
    
    AgentRegistration() : load_factor(0.0), 
        last_heartbeat(std::chrono::system_clock::now()) {}
};

/**
 * Task descriptor for multi-agent coordination
 */
struct CoordinationTask {
    std::string task_id;                        // Unique task identifier
    std::string description;                    // Task description
    std::vector<std::string> required_capabilities; // Required capabilities
    MessagePriority priority;                   // Task priority
    AgentId assigned_to;                        // Assigned agent (if any)
    std::string status;                         // Task status (pending, assigned, in_progress, completed, failed)
    std::chrono::system_clock::time_point created; // Creation time
    std::chrono::system_clock::time_point deadline; // Deadline (if any)
    mutable Handle task_atom;                   // AtomSpace representation (mutable for updateTaskAtom)
    std::map<std::string, std::string> parameters; // Task parameters
    
    CoordinationTask() : priority(MessagePriority::NORMAL), 
        status("pending"),
        created(std::chrono::system_clock::now()) {}
};

/**
 * Consensus proposal for distributed decision making
 */
struct ConsensusProposal {
    std::string proposal_id;                    // Unique proposal identifier
    std::string description;                    // Proposal description
    AgentId proposer;                           // Proposing agent
    std::map<AgentId, bool> votes;              // Agent votes (true=agree, false=disagree)
    std::chrono::system_clock::time_point created; // Creation time
    std::chrono::system_clock::time_point voting_deadline; // Voting deadline
    std::string status;                         // Status (voting, accepted, rejected)
    double threshold;                           // Acceptance threshold (0.0-1.0)
    
    ConsensusProposal() : threshold(0.5), status("voting"),
        created(std::chrono::system_clock::now()),
        voting_deadline(std::chrono::system_clock::now() + std::chrono::minutes(5)) {}
};

/**
 * Resource allocation request
 */
struct ResourceRequest {
    std::string request_id;                     // Unique request identifier
    AgentId requester;                          // Requesting agent
    std::string resource_type;                  // Type of resource
    double amount;                              // Amount requested
    MessagePriority priority;                   // Request priority
    std::string status;                         // Status (pending, granted, denied)
    std::chrono::system_clock::time_point created; // Creation time
    
    ResourceRequest() : amount(0.0), priority(MessagePriority::NORMAL),
        status("pending"), created(std::chrono::system_clock::now()) {}
};

/**
 * Coordination statistics
 */
struct CoordinationStats {
    size_t total_agents;                        // Total registered agents
    size_t active_agents;                       // Currently active agents
    size_t total_tasks;                         // Total tasks
    size_t completed_tasks;                     // Completed tasks
    size_t failed_tasks;                        // Failed tasks
    size_t total_proposals;                     // Total consensus proposals
    size_t accepted_proposals;                  // Accepted proposals
    size_t total_conflicts;                     // Total conflicts detected
    size_t resolved_conflicts;                  // Resolved conflicts
    
    CoordinationStats() : total_agents(0), active_agents(0), total_tasks(0),
        completed_tasks(0), failed_tasks(0), total_proposals(0),
        accepted_proposals(0), total_conflicts(0), resolved_conflicts(0) {}
};

/**
 * MultiAgentCoordinator - Coordination protocols for multi-agent systems
 *
 * This class provides coordination protocols for Agent-Zero multi-agent systems,
 * including agent registration, task delegation, consensus mechanisms, conflict
 * resolution, resource allocation, and state synchronization.
 *
 * Key Features:
 * - Agent registration and discovery
 * - Task delegation with capability matching
 * - Consensus-based decision making
 * - Conflict detection and resolution
 * - Resource allocation management
 * - State synchronization across agents
 * - AtomSpace integration for knowledge representation
 */
class MultiAgentCoordinator {
private:
    // Core components
    AtomSpacePtr _atomspace;
    std::shared_ptr<AgentComms> _comms;
    AgentId _coordinator_id;
    
    // Agent management
    std::map<std::string, AgentRegistration> _registered_agents;
    mutable std::mutex _agents_mutex;
    
    // Task management
    std::map<std::string, CoordinationTask> _tasks;
    mutable std::mutex _tasks_mutex;
    
    // Consensus management
    std::map<std::string, ConsensusProposal> _proposals;
    mutable std::mutex _proposals_mutex;
    
    // Resource management
    std::map<std::string, ResourceRequest> _resource_requests;
    std::map<std::string, double> _available_resources;
    mutable std::mutex _resources_mutex;
    
    // Statistics
    CoordinationStats _stats;
    mutable std::mutex _stats_mutex;
    
    // Configuration
    bool _initialized;
    std::chrono::seconds _heartbeat_timeout;
    std::chrono::seconds _task_timeout;
    
    // Internal methods
    void initializeAtomSpace();
    void createCoordinationContextAtoms();
    void updateAgentAtom(const AgentRegistration& agent);
    void updateTaskAtom(const CoordinationTask& task);
    void updateProposalAtom(const ConsensusProposal& proposal);
    void checkAgentHeartbeats();
    void checkTaskDeadlines();
    void checkVotingDeadlines();
    std::vector<AgentId> findCapableAgents(const std::vector<std::string>& capabilities);
    AgentId selectBestAgent(const std::vector<AgentId>& candidates, const CoordinationTask& task);
    double calculateAgentScore(const AgentRegistration& agent, const CoordinationTask& task);
    void handleConflict(const std::string& conflict_type, const std::vector<AgentId>& involved_agents);
    
public:
    /**
     * Constructor
     * @param atomspace AtomSpace for knowledge representation
     * @param comms Communication system
     * @param coordinator_id Coordinator agent identifier
     */
    MultiAgentCoordinator(AtomSpacePtr atomspace,
                         std::shared_ptr<AgentComms> comms,
                         const AgentId& coordinator_id);
    
    /**
     * Destructor
     */
    ~MultiAgentCoordinator();
    
    /**
     * Initialize the coordinator
     * @return true if initialization successful
     */
    bool initialize();
    
    /**
     * Shutdown the coordinator
     */
    void shutdown();
    
    // ==============================================================
    // Agent Registration and Discovery
    // ==============================================================
    
    /**
     * Register an agent with the coordinator
     * @param agent_id Agent identifier
     * @param capabilities Agent capabilities
     * @return true if registration successful
     */
    bool registerAgent(const AgentId& agent_id, 
                      const std::vector<AgentCapability>& capabilities);
    
    /**
     * Unregister an agent
     * @param agent_id Agent identifier
     * @return true if unregistration successful
     */
    bool unregisterAgent(const AgentId& agent_id);
    
    /**
     * Update agent status
     * @param agent_id Agent identifier
     * @param status New status
     * @param load_factor Current load factor
     * @return true if update successful
     */
    bool updateAgentStatus(const AgentId& agent_id, 
                          const std::string& status,
                          double load_factor = 0.0);
    
    /**
     * Record agent heartbeat
     * @param agent_id Agent identifier
     * @return true if heartbeat recorded
     */
    bool recordHeartbeat(const AgentId& agent_id);
    
    /**
     * Get registered agents
     * @return Vector of registered agents
     */
    std::vector<AgentId> getRegisteredAgents() const;
    
    /**
     * Get agent information
     * @param agent_id Agent identifier
     * @return Agent registration info (nullptr if not found)
     */
    std::shared_ptr<AgentRegistration> getAgentInfo(const AgentId& agent_id) const;
    
    /**
     * Find agents with specific capability
     * @param capability Capability name
     * @return Vector of capable agents
     */
    std::vector<AgentId> findAgentsByCapability(const std::string& capability) const;
    
    // ==============================================================
    // Task Delegation and Distribution
    // ==============================================================
    
    /**
     * Create a coordination task
     * @param description Task description
     * @param required_capabilities Required capabilities
     * @param priority Task priority
     * @return Task ID (empty string on failure)
     */
    std::string createTask(const std::string& description,
                          const std::vector<std::string>& required_capabilities,
                          MessagePriority priority = MessagePriority::NORMAL);
    
    /**
     * Assign task to an agent
     * @param task_id Task identifier
     * @param agent_id Agent to assign to (empty for auto-assignment)
     * @return true if assignment successful
     */
    bool assignTask(const std::string& task_id, const AgentId& agent_id = AgentId());
    
    /**
     * Update task status
     * @param task_id Task identifier
     * @param status New status
     * @return true if update successful
     */
    bool updateTaskStatus(const std::string& task_id, const std::string& status);
    
    /**
     * Complete a task
     * @param task_id Task identifier
     * @return true if completion recorded
     */
    bool completeTask(const std::string& task_id);
    
    /**
     * Get task information
     * @param task_id Task identifier
     * @return Task info (nullptr if not found)
     */
    std::shared_ptr<CoordinationTask> getTaskInfo(const std::string& task_id) const;
    
    /**
     * Get all tasks with specific status
     * @param status Task status filter
     * @return Vector of task IDs
     */
    std::vector<std::string> getTasksByStatus(const std::string& status) const;
    
    /**
     * Get tasks assigned to an agent
     * @param agent_id Agent identifier
     * @return Vector of task IDs
     */
    std::vector<std::string> getAgentTasks(const AgentId& agent_id) const;
    
    // ==============================================================
    // Consensus Mechanisms
    // ==============================================================
    
    /**
     * Create a consensus proposal
     * @param description Proposal description
     * @param threshold Acceptance threshold (0.0-1.0)
     * @param voting_duration Voting duration
     * @return Proposal ID (empty string on failure)
     */
    std::string createProposal(const std::string& description,
                              double threshold = 0.5,
                              std::chrono::seconds voting_duration = std::chrono::minutes(5));
    
    /**
     * Cast a vote on a proposal
     * @param proposal_id Proposal identifier
     * @param agent_id Voting agent
     * @param vote Vote (true=agree, false=disagree)
     * @return true if vote recorded
     */
    bool castVote(const std::string& proposal_id, 
                 const AgentId& agent_id,
                 bool vote);
    
    /**
     * Get proposal status
     * @param proposal_id Proposal identifier
     * @return Proposal info (nullptr if not found)
     */
    std::shared_ptr<ConsensusProposal> getProposalInfo(const std::string& proposal_id) const;
    
    /**
     * Check if proposal has reached consensus
     * @param proposal_id Proposal identifier
     * @return true if consensus reached
     */
    bool hasReachedConsensus(const std::string& proposal_id) const;
    
    // ==============================================================
    // Conflict Resolution
    // ==============================================================
    
    /**
     * Detect conflicts between agents
     * @return Vector of conflict descriptions
     */
    std::vector<std::string> detectConflicts() const;
    
    /**
     * Resolve a conflict
     * @param conflict_id Conflict identifier
     * @param resolution Resolution strategy
     * @return true if resolution successful
     */
    bool resolveConflict(const std::string& conflict_id,
                        const std::string& resolution);
    
    // ==============================================================
    // Resource Allocation
    // ==============================================================
    
    /**
     * Register available resource
     * @param resource_type Resource type
     * @param amount Available amount
     * @return true if registration successful
     */
    bool registerResource(const std::string& resource_type, double amount);
    
    /**
     * Request resource allocation
     * @param requester Requesting agent
     * @param resource_type Resource type
     * @param amount Amount requested
     * @param priority Request priority
     * @return Request ID (empty string on failure)
     */
    std::string requestResource(const AgentId& requester,
                               const std::string& resource_type,
                               double amount,
                               MessagePriority priority = MessagePriority::NORMAL);
    
    /**
     * Release allocated resource
     * @param request_id Request identifier
     * @return true if release successful
     */
    bool releaseResource(const std::string& request_id);
    
    /**
     * Get available resource amount
     * @param resource_type Resource type
     * @return Available amount
     */
    double getAvailableResource(const std::string& resource_type) const;
    
    // ==============================================================
    // State Synchronization
    // ==============================================================
    
    /**
     * Synchronize state across agents
     * @param agents Agents to synchronize
     * @return true if synchronization initiated
     */
    bool synchronizeState(const std::vector<AgentId>& agents);
    
    /**
     * Broadcast state update
     * @param update_type Update type
     * @param update_data Update data
     * @return Number of agents notified
     */
    size_t broadcastStateUpdate(const std::string& update_type,
                               const std::string& update_data);
    
    // ==============================================================
    // Monitoring and Statistics
    // ==============================================================
    
    /**
     * Get coordination statistics
     * @return Coordination statistics
     */
    CoordinationStats getStatistics() const;
    
    /**
     * Get active agents count
     * @return Number of active agents
     */
    size_t getActiveAgentsCount() const;
    
    /**
     * Get pending tasks count
     * @return Number of pending tasks
     */
    size_t getPendingTasksCount() const;
    
    /**
     * Check system health
     * @return true if system healthy
     */
    bool isHealthy() const;
    
    /**
     * Get AtomSpace handle
     * @return AtomSpace pointer
     */
    AtomSpacePtr getAtomSpace() const { return _atomspace; }
};

} // namespace communication
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_MULTI_AGENT_COORDINATOR_H
