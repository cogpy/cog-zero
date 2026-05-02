/*
 * opencog/agentzero/communication/AgentComms.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Agent Communication Protocol Implementation
 * Part of the AGENT-ZERO-GENESIS project - AZ-COMM-001
 */

#ifndef _OPENCOG_AGENTZERO_AGENT_COMMS_H
#define _OPENCOG_AGENTZERO_AGENT_COMMS_H

#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>

#ifdef HAVE_COGSERVER
#include <opencog/cogserver/server/Module.h>
#endif

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#include "CommTypes.h"

namespace opencog {
namespace agentzero {
namespace communication {

// Forward declarations
class MessageRouter;
class ProtocolManager;
class MessageSerializer;

/**
 * AgentComms - Main communication protocol handler for Agent-Zero
 *
 * This class provides the core communication infrastructure for Agent-Zero,
 * implementing message routing, protocol management, and AtomSpace integration.
 * It optionally integrates with CogServer for network communication.
 *
 * Key Features:
 * - Multi-protocol message routing (local, network, IPC)
 * - AtomSpace-based message persistence
 * - Priority-based message queuing
 * - Message serialization and deserialization
 * - Statistics and monitoring
 * - Optional CogServer module integration
 */
#ifdef HAVE_COGSERVER
class AgentComms : public Module
#else
class AgentComms
#endif
{
private:
    // Core components
    std::unique_ptr<MessageRouter> _message_router;
    std::unique_ptr<ProtocolManager> _protocol_manager;
    std::unique_ptr<MessageSerializer> _serializer;
    
    // AtomSpace integration
    AtomSpacePtr _atomspace;
    Handle _comm_context_atom;  // Root atom for communication context
    
    // Configuration and state
    CommConfig _config;
    AgentId _agent_id;
    std::atomic<bool> _running;
    std::atomic<bool> _initialized;
    
    // Message handling
    std::queue<CommMessagePtr> _incoming_queue;
    std::queue<CommMessagePtr> _outgoing_queue;
    std::unordered_map<std::string, MessageHandler> _message_handlers;
    
    // Threading and synchronization
    std::thread _processor_thread;
    std::mutex _queue_mutex;
    std::condition_variable _queue_cv;
    
    // Statistics
    CommStats _stats;
    mutable std::mutex _stats_mutex;
    
    // Internal methods
    void initializeAtomSpace();
    void createCommContextAtoms();
    void startProcessorThread();
    void stopProcessorThread();
    void processMessages();
    void processIncomingMessage(const CommMessagePtr& message);
    void processOutgoingMessage(const CommMessagePtr& message);
    bool storeMessageInAtomSpace(const CommMessagePtr& message);
    CommMessagePtr loadMessageFromAtomSpace(const Handle& message_atom);
    void updateStats(const CommMessagePtr& message, bool incoming);

public:
    /**
     * Constructor - Creates AgentComms instance
     * @param agent_id Agent identifier for this communication instance
     * @param config Communication configuration
     * @param atomspace Optional AtomSpace to use (creates new one if null)
     */
#ifdef HAVE_COGSERVER
    AgentComms(CogServer& cogserver, const AgentId& agent_id = AgentId("AgentZero"));
#endif
    AgentComms(const AgentId& agent_id = AgentId("AgentZero"), 
               const CommConfig& config = CommConfig(),
               AtomSpacePtr atomspace = nullptr);
    
    /**
     * Destructor - Cleans up resources and stops processing
     */
    virtual ~AgentComms();
    
#ifdef HAVE_COGSERVER
    // Module interface implementation
    virtual void init() override;
    virtual bool config(const char* config_string) override;
    virtual const char* id() override;
#endif
    
    // Core initialization methods
    /**
     * Initialize the communication system
     * @param agent_id Agent identifier
     * @param config Communication configuration
     * @param atomspace AtomSpace for persistence
     * @return true if initialization successful
     */
    bool initialize(const AgentId& agent_id, 
                   const CommConfig& config = CommConfig(),
                   AtomSpacePtr atomspace = nullptr);
    
    /**
     * Start the communication processing
     * @return true if successfully started
     */
    bool start();
    
    /**
     * Stop the communication processing
     * @return true if successfully stopped
     */
    bool stop();
    
    /**
     * Check if communication system is running
     */
    bool isRunning() const { return _running.load(); }
    
    /**
     * Check if communication system is initialized
     */
    bool isInitialized() const { return _initialized.load(); }
    
    // Message operations
    /**
     * Send a message to another agent
     * @param recipient Target agent ID
     * @param type Message type
     * @param content Message content
     * @param priority Message priority (default: NORMAL)
     * @param protocol Preferred protocol (default: LOCAL)
     * @return Message ID if sent successfully, empty string if failed
     */
    std::string sendMessage(const AgentId& recipient,
                           MessageType type,
                           const std::string& content,
                           MessagePriority priority = MessagePriority::NORMAL,
                           ProtocolType protocol = ProtocolType::LOCAL);
    
    /**
     * Send a message with AtomSpace content
     * @param recipient Target agent ID
     * @param type Message type
     * @param atom_content AtomSpace handle containing message
     * @param priority Message priority (default: NORMAL)
     * @param protocol Preferred protocol (default: LOCAL)
     * @return Message ID if sent successfully, empty string if failed
     */
    std::string sendAtomMessage(const AgentId& recipient,
                               MessageType type,
                               const Handle& atom_content,
                               MessagePriority priority = MessagePriority::NORMAL,
                               ProtocolType protocol = ProtocolType::LOCAL);
    
    /**
     * Send a pre-constructed message
     * @param message Message to send
     * @return true if sent successfully
     */
    bool sendMessage(const CommMessagePtr& message);
    
    /**
     * Broadcast a message to multiple recipients
     * @param recipients List of target agent IDs
     * @param type Message type
     * @param content Message content
     * @param priority Message priority (default: NORMAL)
     * @return Number of successful sends
     */
    size_t broadcastMessage(const std::vector<AgentId>& recipients,
                           MessageType type,
                           const std::string& content,
                           MessagePriority priority = MessagePriority::NORMAL);
    
    // Message handler registration
    /**
     * Register a handler for specific message types
     * @param message_type Type of messages to handle
     * @param handler Function to handle messages
     * @return true if handler registered successfully
     */
    bool registerMessageHandler(MessageType message_type, MessageHandler handler);
    
    /**
     * Unregister a message handler
     * @param message_type Type of messages to stop handling
     * @return true if handler unregistered successfully
     */
    bool unregisterMessageHandler(MessageType message_type);
    
    // Configuration and management
    /**
     * Update communication configuration
     * @param config New configuration
     * @return true if update successful
     */
    bool updateConfig(const CommConfig& config);
    
    /**
     * Get current configuration
     */
    const CommConfig& getConfig() const { return _config; }
    
    /**
     * Get agent ID
     */
    const AgentId& getAgentId() const { return _agent_id; }
    
    /**
     * Get AtomSpace instance
     */
    AtomSpacePtr getAtomSpace() const { return _atomspace; }
    
    /**
     * Set AtomSpace (primarily for testing)
     */
    void setAtomSpace(AtomSpacePtr atomspace);
    
    // Statistics and monitoring
    /**
     * Get communication statistics
     */
    CommStats getStats() const;
    
    /**
     * Reset communication statistics
     */
    void resetStats();
    
    /**
     * Get status information as JSON string
     */
    std::string getStatusInfo() const;
    
    /**
     * Get pending message count
     */
    size_t getPendingMessageCount() const;
    
    // Advanced operations
    /**
     * Query messages from AtomSpace by criteria
     * @param sender_filter Sender agent filter (empty = all)
     * @param type_filter Message type filter
     * @param max_results Maximum number of results
     * @return List of matching messages
     */
    std::vector<CommMessagePtr> queryMessages(const std::string& sender_filter = "",
                                             MessageType type_filter = MessageType::INFO,
                                             size_t max_results = 100);
    
    /**
     * Purge old messages from AtomSpace
     * @param older_than Age threshold for purging
     * @return Number of messages purged
     */
    size_t purgeOldMessages(std::chrono::hours older_than = std::chrono::hours(24));
    
    /**
     * Enable/disable specific protocols
     * @param protocol Protocol type to modify
     * @param enabled Whether to enable or disable
     * @return true if change successful
     */
    bool setProtocolEnabled(ProtocolType protocol, bool enabled);
    
    /**
     * Get list of available agents (discovered through network/registry)
     */
    std::vector<AgentId> getAvailableAgents() const;
};

} // namespace communication
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_AGENT_COMMS_H