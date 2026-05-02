/*
 * opencog/agentzero/communication/CommTypes.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Communication Types and Constants
 * Part of the AGENT-ZERO-GENESIS project - AZ-COMM-001
 */

#ifndef _OPENCOG_AGENTZERO_COMM_TYPES_H
#define _OPENCOG_AGENTZERO_COMM_TYPES_H

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <chrono>
#include <functional>

#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {
namespace communication {

// Message priority levels
enum class MessagePriority : uint8_t {
    CRITICAL = 0,   // Urgent system messages
    HIGH = 1,       // Important operational messages
    NORMAL = 2,     // Standard communications
    LOW = 3         // Background/informational messages
};

// Communication protocol types
enum class ProtocolType : uint8_t {
    LOCAL = 0,      // In-process communication via AtomSpace
    NETWORK = 1,    // Network communication via CogServer
    IPC = 2,        // Inter-process communication
    BROADCAST = 3   // Broadcast to multiple recipients
};

// Message types for Agent-Zero communication
enum class MessageType : uint8_t {
    // Basic message types
    INFO = 0,           // Informational message
    REQUEST = 1,        // Request for action/information
    RESPONSE = 2,       // Response to a request
    NOTIFICATION = 3,   // Event notification
    
    // Agent coordination
    GOAL_UPDATE = 10,   // Goal state changes
    TASK_ASSIGNMENT = 11, // Task delegation
    STATUS_REPORT = 12, // Agent status updates
    
    // Knowledge sharing
    KNOWLEDGE_SHARE = 20, // Share knowledge/facts
    QUERY = 21,          // Knowledge query
    LEARNING_UPDATE = 22, // Learning/adaptation updates
    
    // System messages
    HEARTBEAT = 30,      // Keep-alive messages
    ERROR = 31,          // Error reports
    SHUTDOWN = 32        // Shutdown notifications
};

// Message status tracking
enum class MessageStatus : uint8_t {
    CREATED = 0,    // Message created but not sent
    SENT = 1,       // Message sent
    DELIVERED = 2,  // Message delivered to recipient
    PROCESSED = 3,  // Message processed by recipient
    FAILED = 4,     // Message delivery failed
    TIMEOUT = 5     // Message timed out
};

// Agent identifier for communication routing
struct AgentId {
    std::string name;           // Agent name
    std::string instance_id;    // Unique instance identifier
    std::string network_address; // Network address (if applicable)
    
    AgentId() = default;
    AgentId(const std::string& n) : name(n) {}
    AgentId(const std::string& n, const std::string& id) : name(n), instance_id(id) {}
    
    bool operator==(const AgentId& other) const {
        return name == other.name && instance_id == other.instance_id;
    }
    
    std::string toString() const {
        if (instance_id.empty()) {
            return name;
        }
        return name + "@" + instance_id;
    }
};

// Communication message structure
struct CommMessage {
    std::string message_id;         // Unique message identifier
    AgentId sender;                 // Sender agent
    AgentId recipient;              // Recipient agent
    MessageType type;               // Message type
    MessagePriority priority;       // Message priority
    ProtocolType protocol;          // Communication protocol
    MessageStatus status;           // Current status
    
    std::string content;            // Message content (JSON/text)
    Handle atom_content;            // AtomSpace representation
    
    std::chrono::system_clock::time_point timestamp; // Creation timestamp
    std::chrono::system_clock::time_point expires;   // Expiration time
    
    std::map<std::string, std::string> metadata;     // Additional metadata
    
    CommMessage() : 
        type(MessageType::INFO),
        priority(MessagePriority::NORMAL),
        protocol(ProtocolType::LOCAL),
        status(MessageStatus::CREATED),
        timestamp(std::chrono::system_clock::now()),
        expires(std::chrono::system_clock::now() + std::chrono::minutes(30))
    {}
};

using CommMessagePtr = std::shared_ptr<CommMessage>;

// Message handler function type
using MessageHandler = std::function<bool(const CommMessagePtr&)>;

// Communication statistics
struct CommStats {
    size_t messages_sent;       // Total messages sent
    size_t messages_received;   // Total messages received
    size_t messages_failed;     // Failed message count
    size_t bytes_sent;          // Total bytes sent
    size_t bytes_received;      // Total bytes received
    
    std::chrono::system_clock::time_point last_activity; // Last communication
    
    CommStats() : 
        messages_sent(0), messages_received(0), messages_failed(0),
        bytes_sent(0), bytes_received(0),
        last_activity(std::chrono::system_clock::now())
    {}
};

// Communication configuration
struct CommConfig {
    bool enable_network;        // Enable network communication
    bool enable_persistence;    // Persist messages in AtomSpace
    bool enable_compression;    // Enable message compression
    bool enable_encryption;     // Enable message encryption (future)
    
    size_t max_message_size;    // Maximum message size
    std::chrono::seconds message_timeout; // Default message timeout
    size_t max_queue_size;      // Maximum message queue size
    
    std::string network_address; // Network binding address
    uint16_t network_port;       // Network port
    
    CommConfig() :
        enable_network(false),
        enable_persistence(true),
        enable_compression(false),
        enable_encryption(false),
        max_message_size(1024 * 1024), // 1MB default
        message_timeout(std::chrono::seconds(30)),
        max_queue_size(1000),
        network_address("127.0.0.1"),
        network_port(17001)
    {}
};

// Utility functions for communication types
namespace utils {

/**
 * Generate unique message ID
 */
std::string generateMessageId();

/**
 * Convert message type to string
 */
std::string messageTypeToString(MessageType type);

/**
 * Convert string to message type
 */
MessageType stringToMessageType(const std::string& str);

/**
 * Convert priority to string
 */
std::string priorityToString(MessagePriority priority);

/**
 * Convert protocol type to string
 */
std::string protocolTypeToString(ProtocolType protocol);

/**
 * Validate agent ID format
 */
bool validateAgentId(const AgentId& id);

/**
 * Create agent ID from string
 */
AgentId parseAgentId(const std::string& id_string);

} // namespace utils

} // namespace communication
} // namespace agentzero  
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_COMM_TYPES_H