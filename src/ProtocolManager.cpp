/*
 * opencog/agentzero/communication/ProtocolManager.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Protocol Management Component Implementation
 * Part of the AGENT-ZERO-GENESIS project - AZ-COMM-001
 */

#include <sstream>
#include <algorithm>

#include <opencog/util/Logger.h>

#include "ProtocolManager.h"

using namespace opencog;
using namespace opencog::agentzero::communication;

ProtocolManager::ProtocolManager(const CommConfig& config)
    : _config(config)
{
    initializeProtocols();
}

ProtocolManager::~ProtocolManager() {
#ifdef HAVE_COGSERVER
    if (isNetworkServerRunning()) {
        stopNetworkServer();
    }
#endif
}

void ProtocolManager::initializeProtocols() {
    // Initialize protocol handlers
    _handlers[ProtocolType::LOCAL] = [this](const CommMessagePtr& msg) {
        return handleLocalProtocol(msg);
    };
    
    _handlers[ProtocolType::NETWORK] = [this](const CommMessagePtr& msg) {
        return handleNetworkProtocol(msg);
    };
    
    _handlers[ProtocolType::IPC] = [this](const CommMessagePtr& msg) {
        return handleIPCProtocol(msg);
    };
    
    _handlers[ProtocolType::BROADCAST] = [this](const CommMessagePtr& msg) {
        return handleBroadcastProtocol(msg);
    };
    
    // Initialize protocol enabled status
    _protocol_enabled[ProtocolType::LOCAL] = true;  // Always enabled
    _protocol_enabled[ProtocolType::NETWORK] = _config.enable_network;
    _protocol_enabled[ProtocolType::IPC] = true;    // Always enabled
    _protocol_enabled[ProtocolType::BROADCAST] = true; // Always enabled
    
    // Initialize statistics
    for (const auto& protocol : {ProtocolType::LOCAL, ProtocolType::NETWORK, 
                                ProtocolType::IPC, ProtocolType::BROADCAST}) {
        _messages_sent[protocol] = 0;
        _messages_failed[protocol] = 0;
        _bytes_sent[protocol] = 0;
    }
    
    logger().debug("ProtocolManager initialized with %zu protocols", _handlers.size());
}

bool ProtocolManager::sendMessage(const CommMessagePtr& message) {
    if (!message) {
        return false;
    }
    
    return sendMessageViaProtocol(message, message->protocol);
}

bool ProtocolManager::sendMessageViaProtocol(const CommMessagePtr& message, ProtocolType protocol) {
    if (!message) {
        return false;
    }
    
    // Check if protocol is enabled
    if (!isProtocolEnabled(protocol)) {
        logger().warn("Protocol %s is disabled", utils::protocolTypeToString(protocol).c_str());
        updateProtocolStats(protocol, false, 0);
        return false;
    }
    
    // Find protocol handler
    auto it = _handlers.find(protocol);
    if (it == _handlers.end()) {
        logger().error("No handler found for protocol: %s", 
                      utils::protocolTypeToString(protocol).c_str());
        updateProtocolStats(protocol, false, 0);
        return false;
    }
    
    try {
        // Call protocol handler
        bool success = it->second(message);
        
        // Update statistics
        size_t message_size = message->content.size();
        updateProtocolStats(protocol, success, message_size);
        
        if (success) {
            logger().debug("Message %s sent via %s protocol", 
                          message->message_id.c_str(),
                          utils::protocolTypeToString(protocol).c_str());
        } else {
            logger().warn("Failed to send message %s via %s protocol",
                         message->message_id.c_str(),
                         utils::protocolTypeToString(protocol).c_str());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger().error("Exception in protocol %s handler: %s",
                      utils::protocolTypeToString(protocol).c_str(), e.what());
        updateProtocolStats(protocol, false, 0);
        return false;
    }
}

bool ProtocolManager::registerProtocolHandler(ProtocolType protocol, ProtocolHandler handler) {
    if (!handler) {
        return false;
    }
    
    _handlers[protocol] = handler;
    
    // Enable protocol by default when handler is registered
    _protocol_enabled[protocol] = true;
    
    // Initialize statistics if not present
    if (_messages_sent.find(protocol) == _messages_sent.end()) {
        _messages_sent[protocol] = 0;
        _messages_failed[protocol] = 0;
        _bytes_sent[protocol] = 0;
    }
    
    logger().debug("Registered custom handler for protocol: %s",
                   utils::protocolTypeToString(protocol).c_str());
    
    return true;
}

bool ProtocolManager::unregisterProtocolHandler(ProtocolType protocol) {
    auto it = _handlers.find(protocol);
    if (it == _handlers.end()) {
        return false;
    }
    
    _handlers.erase(it);
    _protocol_enabled[protocol] = false;
    
    logger().debug("Unregistered handler for protocol: %s",
                   utils::protocolTypeToString(protocol).c_str());
    
    return true;
}

bool ProtocolManager::setProtocolEnabled(ProtocolType protocol, bool enabled) {
    _protocol_enabled[protocol] = enabled;
    
    logger().debug("Protocol %s %s", 
                   utils::protocolTypeToString(protocol).c_str(),
                   enabled ? "enabled" : "disabled");
    
    return true;
}

bool ProtocolManager::isProtocolEnabled(ProtocolType protocol) const {
    auto it = _protocol_enabled.find(protocol);
    return it != _protocol_enabled.end() && it->second;
}

std::vector<ProtocolType> ProtocolManager::getEnabledProtocols() const {
    std::vector<ProtocolType> enabled_protocols;
    
    for (const auto& pair : _protocol_enabled) {
        if (pair.second) {
            enabled_protocols.push_back(pair.first);
        }
    }
    
    return enabled_protocols;
}

bool ProtocolManager::testProtocol(ProtocolType protocol) {
    // Create a test message
    auto test_message = std::make_shared<CommMessage>();
    test_message->message_id = "test_" + utils::generateMessageId();
    test_message->sender = AgentId("TestSender");
    test_message->recipient = AgentId("TestRecipient");
    test_message->type = MessageType::HEARTBEAT;
    test_message->content = "protocol_test";
    test_message->protocol = protocol;
    
    // Try to send test message
    return sendMessageViaProtocol(test_message, protocol);
}

double ProtocolManager::getProtocolHealth(ProtocolType protocol) const {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    
    auto sent_it = _messages_sent.find(protocol);
    auto failed_it = _messages_failed.find(protocol);
    
    if (sent_it == _messages_sent.end() || sent_it->second == 0) {
        return 1.0; // No messages sent, assume healthy
    }
    
    size_t sent = sent_it->second;
    size_t failed = (failed_it != _messages_failed.end()) ? failed_it->second : 0;
    
    double success_rate = 1.0 - (static_cast<double>(failed) / static_cast<double>(sent));
    return std::max(0.0, success_rate);
}

bool ProtocolManager::updateConfig(const CommConfig& config) {
    _config = config;
    
    // Update protocol enabled status based on new config
    _protocol_enabled[ProtocolType::NETWORK] = config.enable_network;
    
    logger().debug("ProtocolManager configuration updated");
    return true;
}

std::string ProtocolManager::getStats() const {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    
    std::stringstream ss;
    ss << "{";
    
    bool first = true;
    for (const auto& pair : _messages_sent) {
        if (!first) ss << ",";
        first = false;
        
        ProtocolType protocol = pair.first;
        std::string protocol_name = utils::protocolTypeToString(protocol);
        
        size_t sent = pair.second;
        size_t failed = (_messages_failed.find(protocol) != _messages_failed.end()) ? 
                       _messages_failed.at(protocol) : 0;
        uint64_t bytes = (_bytes_sent.find(protocol) != _bytes_sent.end()) ? 
                        _bytes_sent.at(protocol) : 0;
        
        ss << "\"" << protocol_name << "\":{"
           << "\"messages_sent\":" << sent << ","
           << "\"messages_failed\":" << failed << ","
           << "\"bytes_sent\":" << bytes << ","
           << "\"enabled\":" << (isProtocolEnabled(protocol) ? "true" : "false") << ","
           << "\"health\":" << getProtocolHealth(protocol)
           << "}";
    }
    
    ss << "}";
    return ss.str();
}

void ProtocolManager::resetStats() {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    
    for (auto& pair : _messages_sent) {
        pair.second = 0;
    }
    
    for (auto& pair : _messages_failed) {
        pair.second = 0;
    }
    
    for (auto& pair : _bytes_sent) {
        pair.second = 0;
    }
}

size_t ProtocolManager::getMessageCount(ProtocolType protocol) const {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    
    auto it = _messages_sent.find(protocol);
    return (it != _messages_sent.end()) ? it->second : 0;
}

double ProtocolManager::getFailureRate(ProtocolType protocol) const {
    return 1.0 - getProtocolHealth(protocol);
}

// Protocol-specific implementations

bool ProtocolManager::handleLocalProtocol(const CommMessagePtr& message) {
    if (!message) {
        return false;
    }
    
    // Local protocol implementation
    // In a full implementation, this would deliver the message to local agents
    // via direct function calls or shared memory
    
    logger().debug("Handling local protocol for message: %s", message->message_id.c_str());
    
    // For now, just log the message (placeholder implementation)
    logger().debug("Local delivery: %s -> %s: %s",
                   message->sender.toString().c_str(),
                   message->recipient.toString().c_str(),
                   message->content.c_str());
    
    return true;
}

bool ProtocolManager::handleNetworkProtocol(const CommMessagePtr& message) {
    if (!message) {
        return false;
    }
    
#ifdef HAVE_COGSERVER
    // Network protocol implementation using CogServer
    logger().debug("Handling network protocol for message: %s", message->message_id.c_str());
    
    // In a full implementation, this would use CogServer's network facilities
    // to send the message to remote agents
    
    // For now, just log the message (placeholder implementation)
    logger().debug("Network delivery: %s -> %s: %s",
                   message->sender.toString().c_str(),
                   message->recipient.toString().c_str(),
                   message->content.c_str());
    
    return true;
#else
    logger().warn("Network protocol not available - CogServer not compiled");
    return false;
#endif
}

bool ProtocolManager::handleIPCProtocol(const CommMessagePtr& message) {
    if (!message) {
        return false;
    }
    
    // IPC protocol implementation
    // In a full implementation, this would use inter-process communication
    // mechanisms like named pipes, Unix sockets, or message queues
    
    logger().debug("Handling IPC protocol for message: %s", message->message_id.c_str());
    
    // For now, just log the message (placeholder implementation)
    logger().debug("IPC delivery: %s -> %s: %s",
                   message->sender.toString().c_str(),
                   message->recipient.toString().c_str(),
                   message->content.c_str());
    
    return true;
}

bool ProtocolManager::handleBroadcastProtocol(const CommMessagePtr& message) {
    if (!message) {
        return false;
    }
    
    // Broadcast protocol implementation
    // This would typically delegate to other protocols for actual delivery
    
    logger().debug("Handling broadcast protocol for message: %s", message->message_id.c_str());
    
    // For now, just log the message (placeholder implementation)
    logger().debug("Broadcast delivery: %s -> %s: %s",
                   message->sender.toString().c_str(),
                   message->recipient.toString().c_str(),
                   message->content.c_str());
    
    return true;
}

void ProtocolManager::updateProtocolStats(ProtocolType protocol, bool success, size_t bytes) {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    
    _messages_sent[protocol]++;
    if (!success) {
        _messages_failed[protocol]++;
    } else {
        _bytes_sent[protocol] += bytes;
    }
}

// Network-specific operations

#ifdef HAVE_COGSERVER
bool ProtocolManager::setNetworkBinding(const std::string& address, uint16_t port) {
    _config.network_address = address;
    _config.network_port = port;
    
    logger().debug("Network binding set to %s:%d", address.c_str(), port);
    return true;
}

std::pair<std::string, uint16_t> ProtocolManager::getNetworkBinding() const {
    return {_config.network_address, _config.network_port};
}

bool ProtocolManager::startNetworkServer() {
    // Placeholder for CogServer network server startup
    logger().debug("Starting network server on %s:%d", 
                   _config.network_address.c_str(), _config.network_port);
    return true;
}

bool ProtocolManager::stopNetworkServer() {
    // Placeholder for CogServer network server shutdown
    logger().debug("Stopping network server");
    return true;
}

bool ProtocolManager::isNetworkServerRunning() const {
    // Placeholder for network server status check
    return _config.enable_network;
}
#endif

// Protocol utility methods

uint32_t ProtocolManager::estimateDeliveryTime(ProtocolType protocol, size_t message_size) const {
    switch (protocol) {
        case ProtocolType::LOCAL:
            return 1; // Sub-millisecond for local delivery
        case ProtocolType::IPC:
            return 5 + (message_size / 1000); // 5ms base + 1ms per KB
        case ProtocolType::NETWORK:
            return 50 + (message_size / 100); // 50ms base + 10ms per KB
        case ProtocolType::BROADCAST:
            return estimateDeliveryTime(ProtocolType::NETWORK, message_size) * 2;
        default:
            return 100; // Default estimate
    }
}

size_t ProtocolManager::getMaxMessageSize(ProtocolType protocol) const {
    switch (protocol) {
        case ProtocolType::LOCAL:
            return SIZE_MAX; // No practical limit for local messages
        case ProtocolType::IPC:
            return 64 * 1024 * 1024; // 64MB for IPC
        case ProtocolType::NETWORK:
            return _config.max_message_size; // Configurable network limit
        case ProtocolType::BROADCAST:
            return std::min(getMaxMessageSize(ProtocolType::NETWORK),
                           getMaxMessageSize(ProtocolType::IPC));
        default:
            return _config.max_message_size;
    }
}

bool ProtocolManager::supportsBroadcast(ProtocolType protocol) const {
    switch (protocol) {
        case ProtocolType::LOCAL:
            return true;  // Can broadcast to local agents
        case ProtocolType::NETWORK:
            return true;  // Can broadcast over network
        case ProtocolType::IPC:
            return false; // IPC typically point-to-point
        case ProtocolType::BROADCAST:
            return true;  // This is the broadcast protocol
        default:
            return false;
    }
}

double ProtocolManager::getProtocolReliability(ProtocolType protocol) const {
    switch (protocol) {
        case ProtocolType::LOCAL:
            return 0.99; // Very reliable for local communication
        case ProtocolType::IPC:
            return 0.95; // Reliable for IPC
        case ProtocolType::NETWORK:
            return 0.85; // Less reliable due to network issues
        case ProtocolType::BROADCAST:
            return 0.80; // Lowest reliability for broadcast
        default:
            return 0.5;  // Unknown protocol
    }
}