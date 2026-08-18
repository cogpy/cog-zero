/*
 * opencog/agentzero/communication/AgentComms.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * AgentComms — inter-agent messaging protocol
 * Part of Phase 6: Communication & NLP (AZ-COMM-001)
 */

#ifndef _OPENCOG_AGENTZERO_COMMUNICATION_AGENT_COMMS_H
#define _OPENCOG_AGENTZERO_COMMUNICATION_AGENT_COMMS_H

#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {
namespace communication {

enum class MessageType {
    INFO,
    REQUEST,
    RESPONSE,
    NOTIFICATION,
    QUERY,
    HEARTBEAT,
    ERROR,
    TASK_ASSIGNMENT,
    KNOWLEDGE_SHARE,
    SHUTDOWN
};

enum class MessagePriority {
    CRITICAL = 0,
    HIGH = 1,
    NORMAL = 2,
    LOW = 3
};

/** Envelope for inter-agent messages. */
struct AgentMessage {
    std::string id;
    std::string sender_id;
    std::string recipient_id;   ///< empty or "*" = broadcast
    MessageType type = MessageType::INFO;
    MessagePriority priority = MessagePriority::NORMAL;
    std::string content;
    std::map<std::string, std::string> headers;
    std::chrono::system_clock::time_point timestamp;
    Handle atom = Handle::UNDEFINED;
};

using MessageHandlerFn = std::function<void(const AgentMessage&)>;

/**
 * AgentComms — lightweight in-process inter-agent messaging.
 *
 * Provides send/receive/broadcast, inbox queues, type-based handlers,
 * and AtomSpace logging of traffic for later reasoning.
 */
class AgentComms
{
public:
    explicit AgentComms(AtomSpacePtr atomspace,
                        const std::string& agent_id = "Agent");
    virtual ~AgentComms();

    AgentComms(const AgentComms&) = delete;
    AgentComms& operator=(const AgentComms&) = delete;

    /** Register a peer agent id that may exchange messages with us. */
    bool registerPeer(const std::string& peer_id);
    bool unregisterPeer(const std::string& peer_id);
    std::vector<std::string> listPeers() const;
    bool isPeerRegistered(const std::string& peer_id) const;

    /** Deliver a message to a peer inbox (or broadcast). */
    bool send(const std::string& recipient_id,
              const std::string& content,
              MessageType type = MessageType::INFO,
              MessagePriority priority = MessagePriority::NORMAL);

    bool sendMessage(const AgentMessage& message);
    bool broadcast(const std::string& content,
                   MessageType type = MessageType::NOTIFICATION);

    /** Pop next message from local inbox (false if empty). */
    bool receive(AgentMessage& out);
    size_t inboxSize() const;
    void clearInbox();

    /** Register a callback invoked when a message of the given type arrives. */
    void setHandler(MessageType type, MessageHandlerFn handler);
    void clearHandler(MessageType type);

    /** Process up to max_messages from the inbox, invoking handlers. */
    size_t dispatch(size_t max_messages = 32);

    /** Inject a message into this agent's inbox (used by tests / bus). */
    bool deliver(const AgentMessage& message);

    std::string getAgentId() const { return _agent_id; }
    AtomSpacePtr getAtomSpace() const { return _atomspace; }

    size_t getSentCount() const;
    size_t getReceivedCount() const;

    static std::string messageTypeToString(MessageType type);
    static MessageType stringToMessageType(const std::string& s);
    static std::string priorityToString(MessagePriority p);
    static std::string generateMessageId();

private:
    AtomSpacePtr _atomspace;
    std::string _agent_id;

    mutable std::mutex _mutex;
    std::map<std::string, bool> _peers; // peer_id -> active
    std::deque<AgentMessage> _inbox;
    std::map<MessageType, MessageHandlerFn> _handlers;

    size_t _sent_count = 0;
    size_t _received_count = 0;
    size_t _max_inbox = 1000;

    Handle _comms_root = Handle::UNDEFINED;
    Handle _agent_node = Handle::UNDEFINED;

    void initializeAtoms();
    Handle storeMessageAtom(const AgentMessage& msg);
    AgentMessage makeMessage(const std::string& recipient,
                             const std::string& content,
                             MessageType type,
                             MessagePriority priority) const;
};

} // namespace communication
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_COMMUNICATION_AGENT_COMMS_H
