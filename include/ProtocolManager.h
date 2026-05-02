/*
 * opencog/agentzero/communication/ProtocolManager.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Protocol Management Component
 * Part of the AGENT-ZERO-GENESIS project - AZ-COMM-001
 */

#ifndef _OPENCOG_AGENTZERO_PROTOCOL_MANAGER_H
#define _OPENCOG_AGENTZERO_PROTOCOL_MANAGER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>

#include <opencog/util/Logger.h>

#include "CommTypes.h"

namespace opencog {
namespace agentzero {
namespace communication {

// Protocol handler function type
using ProtocolHandler = std::function<bool(const CommMessagePtr&)>;

/**
 * ProtocolManager - Manages different communication protocols
 *
 * This class handles the various communication protocols available in the
 * Agent-Zero communication system, providing a unified interface for
 * message transmission across different transport mechanisms.
 *
 * Key Features:
 * - Multi-protocol support (local, network, IPC, broadcast)
 * - Protocol-specific configuration and optimization
 * - Load balancing and failover
 * - Protocol health monitoring
 * - Dynamic protocol enabling/disabling
 */
class ProtocolManager {
private:
    // Configuration
    CommConfig _config;
    
    // Protocol handlers
    std::unordered_map<ProtocolType, ProtocolHandler> _handlers;
    std::unordered_map<ProtocolType, bool> _protocol_enabled;
    
    // Protocol statistics
    std::unordered_map<ProtocolType, size_t> _messages_sent;
    std::unordered_map<ProtocolType, size_t> _messages_failed;
    std::unordered_map<ProtocolType, uint64_t> _bytes_sent;
    mutable std::mutex _stats_mutex;
    
    // Internal methods
    void initializeProtocols();
    bool handleLocalProtocol(const CommMessagePtr& message);
    bool handleNetworkProtocol(const CommMessagePtr& message);
    bool handleIPCProtocol(const CommMessagePtr& message);
    bool handleBroadcastProtocol(const CommMessagePtr& message);
    void updateProtocolStats(ProtocolType protocol, bool success, size_t bytes);

public:
    /**
     * Constructor
     * @param config Communication configuration
     */
    explicit ProtocolManager(const CommConfig& config);
    
    /**
     * Destructor
     */
    virtual ~ProtocolManager();
    
    // Core protocol operations
    /**
     * Send message using appropriate protocol
     * @param message Message to send
     * @return true if sent successfully
     */
    bool sendMessage(const CommMessagePtr& message);
    
    /**
     * Send message using specific protocol
     * @param message Message to send
     * @param protocol Protocol to use
     * @return true if sent successfully
     */
    bool sendMessageViaProtocol(const CommMessagePtr& message, ProtocolType protocol);
    
    /**
     * Register custom protocol handler
     * @param protocol Protocol type
     * @param handler Handler function
     * @return true if registration successful
     */
    bool registerProtocolHandler(ProtocolType protocol, ProtocolHandler handler);
    
    /**
     * Unregister protocol handler
     * @param protocol Protocol type to unregister
     * @return true if unregistration successful
     */
    bool unregisterProtocolHandler(ProtocolType protocol);
    
    // Protocol management
    /**
     * Enable/disable specific protocol
     * @param protocol Protocol to modify
     * @param enabled Whether to enable or disable
     * @return true if change successful
     */
    bool setProtocolEnabled(ProtocolType protocol, bool enabled);
    
    /**
     * Check if protocol is enabled
     * @param protocol Protocol to check
     * @return true if protocol is enabled
     */
    bool isProtocolEnabled(ProtocolType protocol) const;
    
    /**
     * Get list of enabled protocols
     * @return Vector of enabled protocols
     */
    std::vector<ProtocolType> getEnabledProtocols() const;
    
    /**
     * Test protocol connectivity
     * @param protocol Protocol to test
     * @return true if protocol is operational
     */
    bool testProtocol(ProtocolType protocol);
    
    /**
     * Get protocol health status
     * @param protocol Protocol to check
     * @return Health score (0.0 = unhealthy, 1.0 = healthy)
     */
    double getProtocolHealth(ProtocolType protocol) const;
    
    // Configuration
    /**
     * Update configuration
     * @param config New configuration
     * @return true if update successful
     */
    bool updateConfig(const CommConfig& config);
    
    /**
     * Get current configuration
     */
    const CommConfig& getConfig() const { return _config; }
    
    // Statistics and monitoring
    /**
     * Get protocol statistics
     * @return JSON string with protocol statistics
     */
    std::string getStats() const;
    
    /**
     * Reset protocol statistics
     */
    void resetStats();
    
    /**
     * Get message count for specific protocol
     * @param protocol Protocol to check
     * @return Number of messages sent via protocol
     */
    size_t getMessageCount(ProtocolType protocol) const;
    
    /**
     * Get failure rate for specific protocol
     * @param protocol Protocol to check
     * @return Failure rate (0.0 = no failures, 1.0 = all failures)
     */
    double getFailureRate(ProtocolType protocol) const;
    
    // Network-specific operations (when HAVE_COGSERVER)
#ifdef HAVE_COGSERVER
    /**
     * Set network binding address and port
     * @param address IP address to bind to
     * @param port Port number to bind to
     * @return true if binding successful
     */
    bool setNetworkBinding(const std::string& address, uint16_t port);
    
    /**
     * Get current network binding
     * @return Pair of (address, port)
     */
    std::pair<std::string, uint16_t> getNetworkBinding() const;
    
    /**
     * Start network server for incoming connections
     * @return true if server started successfully
     */
    bool startNetworkServer();
    
    /**
     * Stop network server
     * @return true if server stopped successfully
     */
    bool stopNetworkServer();
    
    /**
     * Check if network server is running
     * @return true if server is active
     */
    bool isNetworkServerRunning() const;
#endif
    
    // Protocol-specific utilities
    /**
     * Estimate message delivery time for protocol
     * @param protocol Protocol to estimate
     * @param message_size Message size in bytes
     * @return Estimated delivery time in milliseconds
     */
    uint32_t estimateDeliveryTime(ProtocolType protocol, size_t message_size) const;
    
    /**
     * Get maximum message size for protocol
     * @param protocol Protocol to check
     * @return Maximum message size in bytes
     */
    size_t getMaxMessageSize(ProtocolType protocol) const;
    
    /**
     * Check if protocol supports broadcast
     * @param protocol Protocol to check
     * @return true if protocol supports broadcast
     */
    bool supportsBroadcast(ProtocolType protocol) const;
    
    /**
     * Get protocol reliability score
     * @param protocol Protocol to check
     * @return Reliability score (0.0 = unreliable, 1.0 = reliable)
     */
    double getProtocolReliability(ProtocolType protocol) const;
};

} // namespace communication
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_PROTOCOL_MANAGER_H