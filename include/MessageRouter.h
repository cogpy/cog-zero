/*
 * opencog/agentzero/communication/MessageRouter.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Message Routing Component
 * Part of the AGENT-ZERO-GENESIS project - AZ-COMM-001
 */

#ifndef _OPENCOG_AGENTZERO_MESSAGE_ROUTER_H
#define _OPENCOG_AGENTZERO_MESSAGE_ROUTER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#include "CommTypes.h"

namespace opencog {
namespace agentzero {
namespace communication {

/**
 * MessageRouter - Routes messages between agents based on addressing and protocols
 *
 * This class handles the routing logic for messages in the Agent-Zero communication
 * system, determining the best path for message delivery based on agent locations,
 * protocol availability, and routing preferences.
 *
 * Key Features:
 * - Multi-protocol routing (local, network, IPC)
 * - Agent discovery and registration
 * - Route optimization and caching
 * - Load balancing for broadcast messages
 * - Integration with AtomSpace for persistent routing tables
 */
class MessageRouter {
private:
    // AtomSpace for persistent routing information
    AtomSpacePtr _atomspace;
    Handle _routing_context_atom;
    
    // Routing tables
    std::unordered_map<std::string, AgentId> _local_agents;      // Local agent registry
    std::unordered_map<std::string, AgentId> _remote_agents;     // Remote agent registry
    std::unordered_map<std::string, ProtocolType> _preferred_protocols; // Agent protocol preferences
    
    // Route caching for performance
    mutable std::unordered_map<std::string, std::vector<ProtocolType>> _route_cache;
    mutable std::mutex _cache_mutex;
    
    // Statistics
    size_t _routes_calculated;
    size_t _cache_hits;
    size_t _cache_misses;
    mutable std::mutex _stats_mutex;
    
    // Internal methods
    void initializeAtomSpace();
    void loadRoutingTable();
    void saveRoutingTable();
    std::vector<ProtocolType> calculateRoute(const AgentId& sender, const AgentId& recipient) const;
    std::string getRouteKey(const AgentId& sender, const AgentId& recipient) const;
    void invalidateCache();
    void updateRoutingAtoms(const AgentId& agent_id, ProtocolType protocol);

public:
    /**
     * Constructor
     * @param atomspace AtomSpace for persistent routing information
     */
    explicit MessageRouter(AtomSpacePtr atomspace);
    
    /**
     * Destructor
     */
    virtual ~MessageRouter();
    
    // Agent registration
    /**
     * Register a local agent
     * @param agent_id Agent to register
     * @param preferred_protocol Preferred communication protocol
     * @return true if registration successful
     */
    bool registerLocalAgent(const AgentId& agent_id, ProtocolType preferred_protocol = ProtocolType::LOCAL);
    
    /**
     * Register a remote agent
     * @param agent_id Agent to register
     * @param preferred_protocol Preferred communication protocol
     * @return true if registration successful
     */
    bool registerRemoteAgent(const AgentId& agent_id, ProtocolType preferred_protocol = ProtocolType::NETWORK);
    
    /**
     * Unregister an agent
     * @param agent_id Agent to unregister
     * @return true if unregistration successful
     */
    bool unregisterAgent(const AgentId& agent_id);
    
    /**
     * Check if agent is registered
     * @param agent_id Agent to check
     * @return true if agent is registered
     */
    bool isAgentRegistered(const AgentId& agent_id) const;
    
    /**
     * Get agent registration type (local/remote)
     * @param agent_id Agent to check
     * @return true if agent is local, false if remote, throws if not found
     */
    bool isLocalAgent(const AgentId& agent_id) const;
    
    // Routing operations
    /**
     * Get optimal route for message delivery
     * @param sender Sending agent
     * @param recipient Receiving agent
     * @param available_protocols Available protocol types
     * @return Ordered list of protocols to try for delivery
     */
    std::vector<ProtocolType> getRoute(const AgentId& sender, 
                                      const AgentId& recipient,
                                      const std::vector<ProtocolType>& available_protocols) const;
    
    /**
     * Get preferred protocol for an agent
     * @param agent_id Agent to check
     * @return Preferred protocol type
     */
    ProtocolType getPreferredProtocol(const AgentId& agent_id) const;
    
    /**
     * Set preferred protocol for an agent
     * @param agent_id Agent to update
     * @param protocol New preferred protocol
     * @return true if update successful
     */
    bool setPreferredProtocol(const AgentId& agent_id, ProtocolType protocol);
    
    /**
     * Get routes for broadcast messages
     * @param sender Sending agent
     * @param recipients List of receiving agents
     * @param available_protocols Available protocol types
     * @return Map of recipient to optimal route
     */
    std::unordered_map<std::string, std::vector<ProtocolType>> 
    getBroadcastRoutes(const AgentId& sender,
                      const std::vector<AgentId>& recipients,
                      const std::vector<ProtocolType>& available_protocols) const;
    
    // Agent discovery
    /**
     * Get list of all registered agents
     * @return Vector of all known agent IDs
     */
    std::vector<AgentId> getAllAgents() const;
    
    /**
     * Get list of local agents
     * @return Vector of local agent IDs
     */
    std::vector<AgentId> getLocalAgents() const;
    
    /**
     * Get list of remote agents
     * @return Vector of remote agent IDs
     */
    std::vector<AgentId> getRemoteAgents() const;
    
    /**
     * Discover agents through network protocols
     * @param protocol Protocol to use for discovery
     * @return Number of new agents discovered
     */
    size_t discoverAgents(ProtocolType protocol = ProtocolType::NETWORK);
    
    // Cache management
    /**
     * Clear route cache
     */
    void clearCache();
    
    /**
     * Get cache statistics
     * @return Pair of (hits, misses)
     */
    std::pair<size_t, size_t> getCacheStats() const;
    
    /**
     * Set cache size limit
     * @param max_entries Maximum number of cached routes
     */
    void setCacheLimit(size_t max_entries);
    
    // Persistence
    /**
     * Save routing table to AtomSpace
     * @return true if save successful
     */
    bool persistRoutingTable();
    
    /**
     * Load routing table from AtomSpace
     * @return true if load successful
     */
    bool loadPersistedRoutingTable();
    
    // Statistics and monitoring
    /**
     * Get routing statistics
     * @return JSON string with routing statistics
     */
    std::string getStats() const;
    
    /**
     * Reset routing statistics
     */
    void resetStats();
    
    /**
     * Get number of registered agents
     * @return Total number of registered agents
     */
    size_t getAgentCount() const;
    
    // Route quality assessment
    /**
     * Evaluate route quality based on latency, reliability, etc.
     * @param route Route to evaluate
     * @param sender Sending agent
     * @param recipient Receiving agent
     * @return Quality score (0.0 = poor, 1.0 = excellent)
     */
    double evaluateRouteQuality(const std::vector<ProtocolType>& route,
                               const AgentId& sender,
                               const AgentId& recipient) const;
    
    /**
     * Update route performance metrics
     * @param sender Sending agent
     * @param recipient Receiving agent
     * @param protocol Protocol used
     * @param success Whether message delivery was successful
     * @param latency_ms Delivery latency in milliseconds
     */
    void updateRouteMetrics(const AgentId& sender,
                           const AgentId& recipient,
                           ProtocolType protocol,
                           bool success,
                           uint32_t latency_ms);
};

} // namespace communication
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_MESSAGE_ROUTER_H