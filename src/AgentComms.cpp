/*
 * opencog/agentzero/communication/AgentComms.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Agent Communication Protocol Implementation
 * Part of the AGENT-ZERO-GENESIS project - AZ-COMM-001
 */

#include <iostream>
#include <sstream>
#include <chrono>
#include <random>

#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>
#include <opencog/util/random.h>

#include "AgentComms.h"
#include "MessageRouter.h"
#include "ProtocolManager.h"
#include "MessageSerializer.h"

using namespace opencog;
using namespace opencog::agentzero::communication;

// Utility function implementations
namespace opencog {
namespace agentzero {
namespace communication {
namespace utils {

std::string generateMessageId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "msg_";
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    ss << time_t << "_";
    
    // Add random hex suffix
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

std::string messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::INFO: return "INFO";
        case MessageType::REQUEST: return "REQUEST";
        case MessageType::RESPONSE: return "RESPONSE";
        case MessageType::NOTIFICATION: return "NOTIFICATION";
        case MessageType::GOAL_UPDATE: return "GOAL_UPDATE";
        case MessageType::TASK_ASSIGNMENT: return "TASK_ASSIGNMENT";
        case MessageType::STATUS_REPORT: return "STATUS_REPORT";
        case MessageType::KNOWLEDGE_SHARE: return "KNOWLEDGE_SHARE";
        case MessageType::QUERY: return "QUERY";
        case MessageType::LEARNING_UPDATE: return "LEARNING_UPDATE";
        case MessageType::HEARTBEAT: return "HEARTBEAT";
        case MessageType::ERROR: return "ERROR";
        case MessageType::SHUTDOWN: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

MessageType stringToMessageType(const std::string& str) {
    if (str == "INFO") return MessageType::INFO;
    if (str == "REQUEST") return MessageType::REQUEST;
    if (str == "RESPONSE") return MessageType::RESPONSE;
    if (str == "NOTIFICATION") return MessageType::NOTIFICATION;
    if (str == "GOAL_UPDATE") return MessageType::GOAL_UPDATE;
    if (str == "TASK_ASSIGNMENT") return MessageType::TASK_ASSIGNMENT;
    if (str == "STATUS_REPORT") return MessageType::STATUS_REPORT;
    if (str == "KNOWLEDGE_SHARE") return MessageType::KNOWLEDGE_SHARE;
    if (str == "QUERY") return MessageType::QUERY;
    if (str == "LEARNING_UPDATE") return MessageType::LEARNING_UPDATE;
    if (str == "HEARTBEAT") return MessageType::HEARTBEAT;
    if (str == "ERROR") return MessageType::ERROR;
    if (str == "SHUTDOWN") return MessageType::SHUTDOWN;
    return MessageType::INFO; // default
}

std::string priorityToString(MessagePriority priority) {
    switch (priority) {
        case MessagePriority::CRITICAL: return "CRITICAL";
        case MessagePriority::HIGH: return "HIGH";
        case MessagePriority::NORMAL: return "NORMAL";
        case MessagePriority::LOW: return "LOW";
        default: return "NORMAL";
    }
}

std::string protocolTypeToString(ProtocolType protocol) {
    switch (protocol) {
        case ProtocolType::LOCAL: return "LOCAL";
        case ProtocolType::NETWORK: return "NETWORK";
        case ProtocolType::IPC: return "IPC";
        case ProtocolType::BROADCAST: return "BROADCAST";
        default: return "LOCAL";
    }
}

bool validateAgentId(const AgentId& id) {
    return !id.name.empty() && id.name.find('@') == std::string::npos;
}

AgentId parseAgentId(const std::string& id_string) {
    size_t at_pos = id_string.find('@');
    if (at_pos == std::string::npos) {
        return AgentId(id_string);
    } else {
        return AgentId(id_string.substr(0, at_pos), id_string.substr(at_pos + 1));
    }
}

} // namespace utils
} // namespace communication
} // namespace agentzero
} // namespace opencog

// AgentComms implementation

#ifdef HAVE_COGSERVER
AgentComms::AgentComms(CogServer& cogserver, const AgentId& agent_id)
    : Module(cogserver), _agent_id(agent_id), _running(false), _initialized(false)
{
    _atomspace = &cogserver.getAtomSpace();
    initialize(agent_id, CommConfig(), _atomspace);
}
#endif

AgentComms::AgentComms(const AgentId& agent_id, const CommConfig& config, AtomSpacePtr atomspace)
    : _agent_id(agent_id), _config(config), _atomspace(atomspace), 
      _running(false), _initialized(false)
{
    if (!_atomspace) {
        _atomspace = createAtomSpace();
    }
    initialize(agent_id, config, _atomspace);
}

AgentComms::~AgentComms() {
    stop();
}

bool AgentComms::initialize(const AgentId& agent_id, const CommConfig& config, AtomSpacePtr atomspace) {
    if (_initialized.load()) {
        logger().warn("AgentComms already initialized");
        return false;
    }
    
    _agent_id = agent_id;
    _config = config;
    
    if (atomspace) {
        _atomspace = atomspace;
    } else if (!_atomspace) {
        _atomspace = createAtomSpace();
    }
    
    try {
        // Initialize components
        _message_router = std::make_unique<MessageRouter>(_atomspace);
        _protocol_manager = std::make_unique<ProtocolManager>(_config);
        _serializer = std::make_unique<MessageSerializer>(_atomspace);
        
        // Initialize AtomSpace structure
        initializeAtomSpace();
        
        _initialized.store(true);
        
        logger().info("AgentComms initialized for agent: %s", _agent_id.toString().c_str());
        return true;
        
    } catch (const std::exception& e) {
        logger().error("Failed to initialize AgentComms: %s", e.what());
        return false;
    }
}

bool AgentComms::start() {
    if (!_initialized.load()) {
        logger().error("AgentComms not initialized - cannot start");
        return false;
    }
    
    if (_running.load()) {
        logger().warn("AgentComms already running");
        return true;
    }
    
    try {
        // Start message processing thread
        startProcessorThread();
        
        _running.store(true);
        _stats.last_activity = std::chrono::system_clock::now();
        
        logger().info("AgentComms started for agent: %s", _agent_id.toString().c_str());
        return true;
        
    } catch (const std::exception& e) {
        logger().error("Failed to start AgentComms: %s", e.what());
        return false;
    }
}

bool AgentComms::stop() {
    if (!_running.load()) {
        return true;
    }
    
    _running.store(false);
    
    try {
        // Stop message processing
        stopProcessorThread();
        
        logger().info("AgentComms stopped for agent: %s", _agent_id.toString().c_str());
        return true;
        
    } catch (const std::exception& e) {
        logger().error("Failed to stop AgentComms: %s", e.what());
        return false;
    }
}

std::string AgentComms::sendMessage(const AgentId& recipient,
                                   MessageType type,
                                   const std::string& content,
                                   MessagePriority priority,
                                   ProtocolType protocol) {
    if (!_running.load()) {
        logger().warn("Cannot send message - AgentComms not running");
        return "";
    }
    
    // Create message
    auto message = std::make_shared<CommMessage>();
    message->message_id = utils::generateMessageId();
    message->sender = _agent_id;
    message->recipient = recipient;
    message->type = type;
    message->priority = priority;
    message->protocol = protocol;
    message->content = content;
    message->timestamp = std::chrono::system_clock::now();
    
    if (sendMessage(message)) {
        return message->message_id;
    }
    
    return "";
}

std::string AgentComms::sendAtomMessage(const AgentId& recipient,
                                       MessageType type,
                                       const Handle& atom_content,
                                       MessagePriority priority,
                                       ProtocolType protocol) {
    if (!_running.load()) {
        logger().warn("Cannot send atom message - AgentComms not running");
        return "";
    }
    
    // Create message with atom content
    auto message = std::make_shared<CommMessage>();
    message->message_id = utils::generateMessageId();
    message->sender = _agent_id;
    message->recipient = recipient;
    message->type = type;
    message->priority = priority;
    message->protocol = protocol;
    message->atom_content = atom_content;
    message->timestamp = std::chrono::system_clock::now();
    
    // Serialize atom to content if serializer available
    if (_serializer && atom_content != Handle::UNDEFINED) {
        message->content = _serializer->serializeAtom(atom_content);
    }
    
    if (sendMessage(message)) {
        return message->message_id;
    }
    
    return "";
}

bool AgentComms::sendMessage(const CommMessagePtr& message) {
    if (!message || !_running.load()) {
        return false;
    }
    
    try {
        // Validate message
        if (!utils::validateAgentId(message->recipient)) {
            logger().warn("Invalid recipient agent ID: %s", message->recipient.toString().c_str());
            return false;
        }
        
        // Add to outgoing queue
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            if (_outgoing_queue.size() >= _config.max_queue_size) {
                logger().warn("Outgoing queue full, dropping message");
                return false;
            }
            _outgoing_queue.push(message);
        }
        
        // Notify processor thread
        _queue_cv.notify_one();
        
        return true;
        
    } catch (const std::exception& e) {
        logger().error("Failed to send message: %s", e.what());
        return false;
    }
}

size_t AgentComms::broadcastMessage(const std::vector<AgentId>& recipients,
                                   MessageType type,
                                   const std::string& content,
                                   MessagePriority priority) {
    size_t success_count = 0;
    
    for (const auto& recipient : recipients) {
        if (!sendMessage(recipient, type, content, priority, ProtocolType::BROADCAST).empty()) {
            ++success_count;
        }
    }
    
    return success_count;
}

bool AgentComms::registerMessageHandler(MessageType message_type, MessageHandler handler) {
    std::string type_key = utils::messageTypeToString(message_type);
    _message_handlers[type_key] = handler;
    
    logger().debug("Registered handler for message type: %s", type_key.c_str());
    return true;
}

bool AgentComms::unregisterMessageHandler(MessageType message_type) {
    std::string type_key = utils::messageTypeToString(message_type);
    auto it = _message_handlers.find(type_key);
    if (it != _message_handlers.end()) {
        _message_handlers.erase(it);
        logger().debug("Unregistered handler for message type: %s", type_key.c_str());
        return true;
    }
    return false;
}

CommStats AgentComms::getStats() const {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    return _stats;
}

void AgentComms::resetStats() {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    _stats = CommStats();
}

std::string AgentComms::getStatusInfo() const {
    std::stringstream ss;
    auto stats = getStats();
    
    ss << "{"
       << "\"agent_id\":\"" << _agent_id.toString() << "\","
       << "\"running\":" << (_running.load() ? "true" : "false") << ","
       << "\"initialized\":" << (_initialized.load() ? "true" : "false") << ","
       << "\"messages_sent\":" << stats.messages_sent << ","
       << "\"messages_received\":" << stats.messages_received << ","
       << "\"messages_failed\":" << stats.messages_failed << ","
       << "\"bytes_sent\":" << stats.bytes_sent << ","
       << "\"bytes_received\":" << stats.bytes_received << ","
       << "\"pending_messages\":" << getPendingMessageCount()
       << "}";
    
    return ss.str();
}

size_t AgentComms::getPendingMessageCount() const {
    std::lock_guard<std::mutex> lock(_queue_mutex);
    return _incoming_queue.size() + _outgoing_queue.size();
}

// Private methods

void AgentComms::initializeAtomSpace() {
    if (!_atomspace) {
        return;
    }
    
    // Create communication context atoms
    createCommContextAtoms();
}

void AgentComms::createCommContextAtoms() {
    // Create root communication context atom
    _comm_context_atom = _atomspace->add_node(CONCEPT_NODE, "AgentCommContext_" + _agent_id.toString());
    
    // Create agent self atom
    Handle agent_atom = _atomspace->add_node(CONCEPT_NODE, "Agent_" + _agent_id.toString());
    
    // Link agent to communication context
    _atomspace->add_link(MEMBER_LINK, {agent_atom, _comm_context_atom});
    
    logger().debug("Created communication context atoms for agent: %s", _agent_id.toString().c_str());
}

void AgentComms::startProcessorThread() {
    _processor_thread = std::thread(&AgentComms::processMessages, this);
}

void AgentComms::stopProcessorThread() {
    if (_processor_thread.joinable()) {
        _queue_cv.notify_all();
        _processor_thread.join();
    }
}

void AgentComms::processMessages() {
    logger().debug("Message processor thread started");
    
    while (_running.load()) {
        std::unique_lock<std::mutex> lock(_queue_mutex);
        
        // Wait for messages or shutdown signal
        _queue_cv.wait(lock, [this] { 
            return !_running.load() || !_incoming_queue.empty() || !_outgoing_queue.empty(); 
        });
        
        // Process incoming messages
        while (!_incoming_queue.empty() && _running.load()) {
            auto message = _incoming_queue.front();
            _incoming_queue.pop();
            lock.unlock();
            
            processIncomingMessage(message);
            lock.lock();
        }
        
        // Process outgoing messages
        while (!_outgoing_queue.empty() && _running.load()) {
            auto message = _outgoing_queue.front();
            _outgoing_queue.pop();
            lock.unlock();
            
            processOutgoingMessage(message);
            lock.lock();
        }
    }
    
    logger().debug("Message processor thread stopped");
}

void AgentComms::processIncomingMessage(const CommMessagePtr& message) {
    if (!message) return;
    
    try {
        // Store message in AtomSpace if persistence enabled
        if (_config.enable_persistence) {
            storeMessageInAtomSpace(message);
        }
        
        // Find and invoke message handler
        std::string type_key = utils::messageTypeToString(message->type);
        auto it = _message_handlers.find(type_key);
        if (it != _message_handlers.end() && it->second) {
            message->status = MessageStatus::PROCESSED;
            bool handled = it->second(message);
            logger().debug("Message %s handled: %s", message->message_id.c_str(), 
                          handled ? "success" : "failure");
        } else {
            logger().debug("No handler found for message type: %s", type_key.c_str());
            message->status = MessageStatus::DELIVERED;
        }
        
        // Update statistics
        updateStats(message, true);
        
    } catch (const std::exception& e) {
        logger().error("Error processing incoming message %s: %s", 
                      message->message_id.c_str(), e.what());
        message->status = MessageStatus::FAILED;
    }
}

void AgentComms::processOutgoingMessage(const CommMessagePtr& message) {
    if (!message) return;
    
    try {
        // Route message through protocol manager
        if (_protocol_manager) {
            bool sent = _protocol_manager->sendMessage(message);
            message->status = sent ? MessageStatus::SENT : MessageStatus::FAILED;
            
            if (sent) {
                logger().debug("Message %s sent via %s protocol", 
                              message->message_id.c_str(),
                              utils::protocolTypeToString(message->protocol).c_str());
            } else {
                logger().warn("Failed to send message %s", message->message_id.c_str());
            }
        } else {
            logger().warn("No protocol manager available - message %s dropped", 
                         message->message_id.c_str());
            message->status = MessageStatus::FAILED;
        }
        
        // Store message in AtomSpace if persistence enabled
        if (_config.enable_persistence) {
            storeMessageInAtomSpace(message);
        }
        
        // Update statistics
        updateStats(message, false);
        
    } catch (const std::exception& e) {
        logger().error("Error processing outgoing message %s: %s", 
                      message->message_id.c_str(), e.what());
        message->status = MessageStatus::FAILED;
    }
}

bool AgentComms::storeMessageInAtomSpace(const CommMessagePtr& message) {
    if (!_atomspace || !message) {
        return false;
    }
    
    try {
        // Create message atom
        Handle message_atom = _atomspace->add_node(CONCEPT_NODE, "Message_" + message->message_id);
        
        // Store message properties as atoms and links
        Handle sender_atom = _atomspace->add_node(CONCEPT_NODE, "Agent_" + message->sender.toString());
        Handle recipient_atom = _atomspace->add_node(CONCEPT_NODE, "Agent_" + message->recipient.toString());
        Handle type_atom = _atomspace->add_node(CONCEPT_NODE, utils::messageTypeToString(message->type));
        
        // Create relationships
        _atomspace->add_link(EVALUATION_LINK, {
            _atomspace->add_node(PREDICATE_NODE, "sender"),
            _atomspace->add_link(LIST_LINK, {message_atom, sender_atom})
        });
        
        _atomspace->add_link(EVALUATION_LINK, {
            _atomspace->add_node(PREDICATE_NODE, "recipient"),
            _atomspace->add_link(LIST_LINK, {message_atom, recipient_atom})
        });
        
        _atomspace->add_link(EVALUATION_LINK, {
            _atomspace->add_node(PREDICATE_NODE, "message_type"),
            _atomspace->add_link(LIST_LINK, {message_atom, type_atom})
        });
        
        // Store content if available
        if (!message->content.empty()) {
            Handle content_atom = _atomspace->add_node(CONCEPT_NODE, message->content);
            _atomspace->add_link(EVALUATION_LINK, {
                _atomspace->add_node(PREDICATE_NODE, "content"),
                _atomspace->add_link(LIST_LINK, {message_atom, content_atom})
            });
        }
        
        // Link to communication context
        _atomspace->add_link(MEMBER_LINK, {message_atom, _comm_context_atom});
        
        return true;
        
    } catch (const std::exception& e) {
        logger().error("Failed to store message in AtomSpace: %s", e.what());
        return false;
    }
}

void AgentComms::updateStats(const CommMessagePtr& message, bool incoming) {
    if (!message) return;
    
    std::lock_guard<std::mutex> lock(_stats_mutex);
    
    if (incoming) {
        _stats.messages_received++;
        _stats.bytes_received += message->content.size();
    } else {
        if (message->status == MessageStatus::SENT || message->status == MessageStatus::DELIVERED) {
            _stats.messages_sent++;
            _stats.bytes_sent += message->content.size();
        } else if (message->status == MessageStatus::FAILED) {
            _stats.messages_failed++;
        }
    }
    
    _stats.last_activity = std::chrono::system_clock::now();
}

#ifdef HAVE_COGSERVER
// CogServer Module interface implementation
void AgentComms::init() {
    logger().info("AgentComms CogServer module initializing");
}

bool AgentComms::config(const char* config_string) {
    logger().info("AgentComms config: %s", config_string ? config_string : "null");
    return true;
}

const char* AgentComms::id() {
    return "AgentComms";
}
#endif

/*
 * AgentComms.cpp (agentzero::communication)
 *
 * Copyright (C) 2024 Agent-Zero-Genesis Project
 * 
 * Inter-agent communication protocols for Agent-Zero
 */

#include "agentzero/communication/AgentComms.h"

#include <chrono>
#include <sstream>

namespace agentzero {
namespace communication {

// -----------------------------------------------------------------------
// Static member definitions
// -----------------------------------------------------------------------

std::mutex AgentComms::_bus_mutex;
std::map<std::string, std::vector<std::string>> AgentComms::_message_bus;
std::atomic<uint64_t> AgentComms::_id_counter{0};

// -----------------------------------------------------------------------
// Construction / destruction
// -----------------------------------------------------------------------

AgentComms::AgentComms(opencog::AtomSpacePtr atomspace,
                       const std::string& agent_id)
    : _atomspace(atomspace)
{
    if (agent_id.empty()) {
        // Generate a unique ID using timestamp + monotonic counter.
        // This guarantees uniqueness within a single process.  For
        // cross-process uniqueness a UUID library would be preferable,
        // but AgentComms currently operates within one process only
        // (the static bus is not shared across process boundaries).
        auto now = std::chrono::system_clock::now();
        uint64_t ts = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count());
        std::ostringstream oss;
        oss << "agent_" << ts << "_" << _id_counter.fetch_add(1);
        _agent_id = oss.str();
    } else {
        _agent_id = agent_id;
    }

    // Ensure inbox exists in the bus
    std::lock_guard<std::mutex> lock(_bus_mutex);
    _message_bus.emplace(_agent_id, std::vector<std::string>{});
}

AgentComms::~AgentComms()
{
    // Remove this agent's inbox from the bus
    std::lock_guard<std::mutex> lock(_bus_mutex);
    _message_bus.erase(_agent_id);
}

// -----------------------------------------------------------------------
// Messaging
// -----------------------------------------------------------------------

bool AgentComms::send_message(const std::string& target_agent_id,
                               const std::string& message)
{
    if (target_agent_id.empty() || message.empty()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(_bus_mutex);
        // Create inbox for target if it doesn't already exist
        _message_bus[target_agent_id].push_back(_agent_id + "|" + message);
    }

    record_message_in_atomspace(target_agent_id, message);
    return true;
}

std::vector<std::string> AgentComms::receive_messages()
{
    std::lock_guard<std::mutex> lock(_bus_mutex);

    auto it = _message_bus.find(_agent_id);
    if (it == _message_bus.end() || it->second.empty()) {
        return {};
    }

    std::vector<std::string> messages;
    messages.swap(it->second);
    return messages;
}

// -----------------------------------------------------------------------
// AtomSpace integration
// -----------------------------------------------------------------------

void AgentComms::record_message_in_atomspace(const std::string& target,
                                              const std::string& message)
{
    if (!_atomspace) return;

    try {
        auto sender_node  = _atomspace->add_node(opencog::CONCEPT_NODE,
                                                 "agent:" + _agent_id);
        auto receiver_node = _atomspace->add_node(opencog::CONCEPT_NODE,
                                                  "agent:" + target);
        auto msg_node     = _atomspace->add_node(opencog::CONCEPT_NODE,
                                                 "message:" + message);
        auto pred_node    = _atomspace->add_node(opencog::PREDICATE_NODE,
                                                 "sent-message");
        opencog::HandleSeq args{sender_node, receiver_node, msg_node};
        _atomspace->add_link(opencog::EVALUATION_LINK,
                             opencog::HandleSeq{pred_node,
                                               _atomspace->add_link(opencog::LIST_LINK,
                                                                     std::move(args))});
    } catch (...) {
        // AtomSpace errors are non-fatal for message delivery
    }
}

} // namespace communication
} // namespace agentzero
