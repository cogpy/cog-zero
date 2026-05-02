/*
 * opencog/agentzero/communication/MessageRouter.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Message Routing Component Implementation
 * Part of the AGENT-ZERO-GENESIS project - AZ-COMM-001
 */

#include <sstream>
#include <algorithm>

#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/util/Logger.h>

#include "MessageRouter.h"

using namespace opencog;
using namespace opencog::agentzero::communication;

MessageRouter::MessageRouter(AtomSpacePtr atomspace)
    : _atomspace(atomspace), _routes_calculated(0), _cache_hits(0), _cache_misses(0)
{
    if (_atomspace) {
        initializeAtomSpace();
        loadPersistedRoutingTable();
    }
}

MessageRouter::~MessageRouter() {
    if (_atomspace) {
        persistRoutingTable();
    }
}

void MessageRouter::initializeAtomSpace() {
    if (!_atomspace) {
        return;
    }
    
    // Create routing context atom
    _routing_context_atom = _atomspace->add_node(CONCEPT_NODE, "MessageRoutingContext");
    
    logger().debug("Initialized MessageRouter AtomSpace context");
}

bool MessageRouter::registerLocalAgent(const AgentId& agent_id, ProtocolType preferred_protocol) {
    if (agent_id.name.empty()) {
        return false;
    }
    
    std::string key = agent_id.toString();
    _local_agents[key] = agent_id;
    _preferred_protocols[key] = preferred_protocol;
    
    // Update AtomSpace
    updateRoutingAtoms(agent_id, preferred_protocol);
    
    // Invalidate cache since routing topology changed
    invalidateCache();
    
    logger().debug("Registered local agent: %s with protocol: %s", 
                   key.c_str(), utils::protocolTypeToString(preferred_protocol).c_str());
    
    return true;
}

bool MessageRouter::registerRemoteAgent(const AgentId& agent_id, ProtocolType preferred_protocol) {
    if (agent_id.name.empty()) {
        return false;
    }
    
    std::string key = agent_id.toString();
    _remote_agents[key] = agent_id;
    _preferred_protocols[key] = preferred_protocol;
    
    // Update AtomSpace
    updateRoutingAtoms(agent_id, preferred_protocol);
    
    // Invalidate cache since routing topology changed
    invalidateCache();
    
    logger().debug("Registered remote agent: %s with protocol: %s", 
                   key.c_str(), utils::protocolTypeToString(preferred_protocol).c_str());
    
    return true;
}

bool MessageRouter::unregisterAgent(const AgentId& agent_id) {
    std::string key = agent_id.toString();
    
    bool found = false;
    
    auto local_it = _local_agents.find(key);
    if (local_it != _local_agents.end()) {
        _local_agents.erase(local_it);
        found = true;
    }
    
    auto remote_it = _remote_agents.find(key);
    if (remote_it != _remote_agents.end()) {
        _remote_agents.erase(remote_it);
        found = true;
    }
    
    auto pref_it = _preferred_protocols.find(key);
    if (pref_it != _preferred_protocols.end()) {
        _preferred_protocols.erase(pref_it);
    }
    
    if (found) {
        // Invalidate cache since routing topology changed
        invalidateCache();
        
        logger().debug("Unregistered agent: %s", key.c_str());
    }
    
    return found;
}

bool MessageRouter::isAgentRegistered(const AgentId& agent_id) const {
    std::string key = agent_id.toString();
    return _local_agents.find(key) != _local_agents.end() ||
           _remote_agents.find(key) != _remote_agents.end();
}

bool MessageRouter::isLocalAgent(const AgentId& agent_id) const {
    std::string key = agent_id.toString();
    
    if (_local_agents.find(key) != _local_agents.end()) {
        return true;
    }
    
    if (_remote_agents.find(key) != _remote_agents.end()) {
        return false;
    }
    
    throw std::runtime_error("Agent not registered: " + key);
}

std::vector<ProtocolType> MessageRouter::getRoute(const AgentId& sender,
                                                  const AgentId& recipient,
                                                  const std::vector<ProtocolType>& available_protocols) const {
    std::string route_key = getRouteKey(sender, recipient);
    
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(_cache_mutex);
        auto it = _route_cache.find(route_key);
        if (it != _route_cache.end()) {
            std::lock_guard<std::mutex> stats_lock(_stats_mutex);
            const_cast<MessageRouter*>(this)->_cache_hits++;
            return it->second;
        }
    }
    
    // Calculate new route
    std::vector<ProtocolType> route = calculateRoute(sender, recipient);
    
    // Filter by available protocols
    std::vector<ProtocolType> filtered_route;
    for (const auto& protocol : route) {
        if (std::find(available_protocols.begin(), available_protocols.end(), protocol) != available_protocols.end()) {
            filtered_route.push_back(protocol);
        }
    }
    
    // Cache the result
    {
        std::lock_guard<std::mutex> lock(_cache_mutex);
        _route_cache[route_key] = filtered_route;
    }
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(_stats_mutex);
        const_cast<MessageRouter*>(this)->_routes_calculated++;
        const_cast<MessageRouter*>(this)->_cache_misses++;
    }
    
    return filtered_route;
}

std::vector<ProtocolType> MessageRouter::calculateRoute(const AgentId& sender, const AgentId& recipient) const {
    std::vector<ProtocolType> route;
    
    try {
        bool sender_local = isLocalAgent(sender);
        bool recipient_local = isLocalAgent(recipient);
        
        // Determine optimal routing strategy
        if (sender_local && recipient_local) {
            // Both agents are local - prefer local communication
            route.push_back(ProtocolType::LOCAL);
            route.push_back(ProtocolType::IPC);
        } else if (sender_local && !recipient_local) {
            // Sending to remote agent - prefer network protocols
            ProtocolType preferred = getPreferredProtocol(recipient);
            route.push_back(preferred);
            
            // Add fallback protocols
            if (preferred != ProtocolType::NETWORK) {
                route.push_back(ProtocolType::NETWORK);
            }
            if (preferred != ProtocolType::IPC) {
                route.push_back(ProtocolType::IPC);
            }
        } else if (!sender_local && recipient_local) {
            // Receiving from remote agent - prefer network protocols
            route.push_back(ProtocolType::NETWORK);
            route.push_back(ProtocolType::IPC);
            route.push_back(ProtocolType::LOCAL);
        } else {
            // Both agents are remote - this shouldn't happen in normal operation
            // but provide a fallback route
            route.push_back(ProtocolType::NETWORK);
        }
        
    } catch (const std::exception& e) {
        // If agents not found, assume network communication
        logger().warn("Route calculation failed: %s - defaulting to network", e.what());
        route.push_back(ProtocolType::NETWORK);
        route.push_back(ProtocolType::LOCAL);
    }
    
    return route;
}

ProtocolType MessageRouter::getPreferredProtocol(const AgentId& agent_id) const {
    std::string key = agent_id.toString();
    auto it = _preferred_protocols.find(key);
    if (it != _preferred_protocols.end()) {
        return it->second;
    }
    
    // Default preference based on registration type
    try {
        return isLocalAgent(agent_id) ? ProtocolType::LOCAL : ProtocolType::NETWORK;
    } catch (const std::exception&) {
        return ProtocolType::NETWORK; // Default fallback
    }
}

bool MessageRouter::setPreferredProtocol(const AgentId& agent_id, ProtocolType protocol) {
    std::string key = agent_id.toString();
    
    if (!isAgentRegistered(agent_id)) {
        return false;
    }
    
    _preferred_protocols[key] = protocol;
    
    // Update AtomSpace
    updateRoutingAtoms(agent_id, protocol);
    
    // Invalidate cache since preferences changed
    invalidateCache();
    
    return true;
}

std::unordered_map<std::string, std::vector<ProtocolType>>
MessageRouter::getBroadcastRoutes(const AgentId& sender,
                                 const std::vector<AgentId>& recipients,
                                 const std::vector<ProtocolType>& available_protocols) const {
    std::unordered_map<std::string, std::vector<ProtocolType>> routes;
    
    for (const auto& recipient : recipients) {
        std::string key = recipient.toString();
        routes[key] = getRoute(sender, recipient, available_protocols);
    }
    
    return routes;
}

std::vector<AgentId> MessageRouter::getAllAgents() const {
    std::vector<AgentId> agents;
    
    for (const auto& pair : _local_agents) {
        agents.push_back(pair.second);
    }
    
    for (const auto& pair : _remote_agents) {
        agents.push_back(pair.second);
    }
    
    return agents;
}

std::vector<AgentId> MessageRouter::getLocalAgents() const {
    std::vector<AgentId> agents;
    
    for (const auto& pair : _local_agents) {
        agents.push_back(pair.second);
    }
    
    return agents;
}

std::vector<AgentId> MessageRouter::getRemoteAgents() const {
    std::vector<AgentId> agents;
    
    for (const auto& pair : _remote_agents) {
        agents.push_back(pair.second);
    }
    
    return agents;
}

size_t MessageRouter::discoverAgents(ProtocolType protocol) {
    // This is a placeholder for network discovery functionality
    // In a full implementation, this would query network protocols
    // for available agents and register them
    
    logger().debug("Agent discovery via %s protocol not yet implemented",
                   utils::protocolTypeToString(protocol).c_str());
    
    return 0;
}

void MessageRouter::clearCache() {
    std::lock_guard<std::mutex> lock(_cache_mutex);
    _route_cache.clear();
}

void MessageRouter::invalidateCache() {
    clearCache();
}

std::pair<size_t, size_t> MessageRouter::getCacheStats() const {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    return {_cache_hits, _cache_misses};
}

std::string MessageRouter::getRouteKey(const AgentId& sender, const AgentId& recipient) const {
    return sender.toString() + "->" + recipient.toString();
}

void MessageRouter::updateRoutingAtoms(const AgentId& agent_id, ProtocolType protocol) {
    if (!_atomspace) {
        return;
    }
    
    try {
        // Create agent atom
        Handle agent_atom = _atomspace->add_node(CONCEPT_NODE, "Agent_" + agent_id.toString());
        
        // Create protocol atom
        Handle protocol_atom = _atomspace->add_node(CONCEPT_NODE, utils::protocolTypeToString(protocol));
        
        // Create preferred protocol relationship
        _atomspace->add_link(EVALUATION_LINK, {
            _atomspace->add_node(PREDICATE_NODE, "preferred_protocol"),
            _atomspace->add_link(LIST_LINK, {agent_atom, protocol_atom})
        });
        
        // Link to routing context
        _atomspace->add_link(MEMBER_LINK, {agent_atom, _routing_context_atom});
        
    } catch (const std::exception& e) {
        logger().error("Failed to update routing atoms: %s", e.what());
    }
}

bool MessageRouter::persistRoutingTable() {
    // The routing table is automatically persisted through updateRoutingAtoms
    // This method is a placeholder for additional persistence logic
    
    logger().debug("Routing table persisted to AtomSpace");
    return true;
}

bool MessageRouter::loadPersistedRoutingTable() {
    if (!_atomspace || !_routing_context_atom) {
        return false;
    }
    
    try {
        // This is a placeholder for loading routing information from AtomSpace
        // In a full implementation, this would query the AtomSpace for
        // agent registration and protocol preference information
        
        logger().debug("Loaded routing table from AtomSpace");
        return true;
        
    } catch (const std::exception& e) {
        logger().error("Failed to load routing table: %s", e.what());
        return false;
    }
}

std::string MessageRouter::getStats() const {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    auto cache_stats = getCacheStats();
    
    std::stringstream ss;
    ss << "{"
       << "\"local_agents\":" << _local_agents.size() << ","
       << "\"remote_agents\":" << _remote_agents.size() << ","
       << "\"routes_calculated\":" << _routes_calculated << ","
       << "\"cache_hits\":" << cache_stats.first << ","
       << "\"cache_misses\":" << cache_stats.second << ","
       << "\"cache_size\":" << _route_cache.size()
       << "}";
    
    return ss.str();
}

void MessageRouter::resetStats() {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    _routes_calculated = 0;
    _cache_hits = 0;
    _cache_misses = 0;
}

size_t MessageRouter::getAgentCount() const {
    return _local_agents.size() + _remote_agents.size();
}

double MessageRouter::evaluateRouteQuality(const std::vector<ProtocolType>& route,
                                          const AgentId& sender,
                                          const AgentId& recipient) const {
    if (route.empty()) {
        return 0.0;
    }
    
    // Simple quality assessment based on protocol preferences
    // In a full implementation, this would consider latency, reliability, etc.
    
    double quality = 1.0;
    
    try {
        bool sender_local = isLocalAgent(sender);
        bool recipient_local = isLocalAgent(recipient);
        
        ProtocolType primary_protocol = route[0];
        
        // Prefer local communication for local agents
        if (sender_local && recipient_local) {
            if (primary_protocol == ProtocolType::LOCAL) {
                quality = 1.0;
            } else if (primary_protocol == ProtocolType::IPC) {
                quality = 0.8;
            } else {
                quality = 0.6;
            }
        }
        // Prefer network communication for remote agents
        else if (!sender_local || !recipient_local) {
            if (primary_protocol == ProtocolType::NETWORK) {
                quality = 1.0;
            } else if (primary_protocol == ProtocolType::IPC) {
                quality = 0.7;
            } else {
                quality = 0.5;
            }
        }
        
    } catch (const std::exception&) {
        quality = 0.5; // Default quality for unknown agents
    }
    
    // Reduce quality for longer routes (more fallbacks)
    if (route.size() > 1) {
        quality *= 0.9;
    }
    
    return quality;
}

void MessageRouter::updateRouteMetrics(const AgentId& sender,
                                      const AgentId& recipient,
                                      ProtocolType protocol,
                                      bool success,
                                      uint32_t latency_ms) {
    // This is a placeholder for route performance tracking
    // In a full implementation, this would store metrics for
    // route quality assessment and optimization
    
    logger().debug("Route metrics update: %s->%s via %s, success=%s, latency=%dms",
                   sender.toString().c_str(),
                   recipient.toString().c_str(),
                   utils::protocolTypeToString(protocol).c_str(),
                   success ? "true" : "false",
                   latency_ms);
}