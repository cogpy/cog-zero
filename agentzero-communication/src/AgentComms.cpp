/*
 * opencog/agentzero/communication/AgentComms.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * AgentComms — inter-agent messaging protocol (Phase 6 / AZ-COMM-001)
 */

#include <algorithm>
#include <random>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/communication/AgentComms.h"

using namespace opencog;
using namespace opencog::agentzero::communication;

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

std::string AgentComms::generateMessageId()
{
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<int> dis(0, 15);
    std::ostringstream ss;
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch())
                  .count();
    ss << "msg_" << ms << "_";
    for (int i = 0; i < 8; ++i) ss << std::hex << dis(gen);
    return ss.str();
}

std::string AgentComms::messageTypeToString(MessageType type)
{
    switch (type) {
        case MessageType::INFO: return "INFO";
        case MessageType::REQUEST: return "REQUEST";
        case MessageType::RESPONSE: return "RESPONSE";
        case MessageType::NOTIFICATION: return "NOTIFICATION";
        case MessageType::QUERY: return "QUERY";
        case MessageType::HEARTBEAT: return "HEARTBEAT";
        case MessageType::ERROR: return "ERROR";
        case MessageType::TASK_ASSIGNMENT: return "TASK_ASSIGNMENT";
        case MessageType::KNOWLEDGE_SHARE: return "KNOWLEDGE_SHARE";
        case MessageType::SHUTDOWN: return "SHUTDOWN";
        default: return "INFO";
    }
}

MessageType AgentComms::stringToMessageType(const std::string& s)
{
    if (s == "INFO") return MessageType::INFO;
    if (s == "REQUEST") return MessageType::REQUEST;
    if (s == "RESPONSE") return MessageType::RESPONSE;
    if (s == "NOTIFICATION") return MessageType::NOTIFICATION;
    if (s == "QUERY") return MessageType::QUERY;
    if (s == "HEARTBEAT") return MessageType::HEARTBEAT;
    if (s == "ERROR") return MessageType::ERROR;
    if (s == "TASK_ASSIGNMENT") return MessageType::TASK_ASSIGNMENT;
    if (s == "KNOWLEDGE_SHARE") return MessageType::KNOWLEDGE_SHARE;
    if (s == "SHUTDOWN") return MessageType::SHUTDOWN;
    return MessageType::INFO;
}

std::string AgentComms::priorityToString(MessagePriority p)
{
    switch (p) {
        case MessagePriority::CRITICAL: return "CRITICAL";
        case MessagePriority::HIGH: return "HIGH";
        case MessagePriority::NORMAL: return "NORMAL";
        case MessagePriority::LOW: return "LOW";
        default: return "NORMAL";
    }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

AgentComms::AgentComms(AtomSpacePtr atomspace, const std::string& agent_id)
    : _atomspace(atomspace)
    , _agent_id(agent_id)
{
    if (!_atomspace)
        throw std::runtime_error("AgentComms requires a valid AtomSpace");
    initializeAtoms();
    logger().info() << "[AgentComms] initialized agent_id=" << _agent_id;
}

AgentComms::~AgentComms() = default;

void AgentComms::initializeAtoms()
{
    _comms_root = _atomspace->add_node(CONCEPT_NODE, "AgentCommsRoot");
    _agent_node = _atomspace->add_node(CONCEPT_NODE, "agent:" + _agent_id);
    _agent_node->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    _atomspace->add_link(MEMBER_LINK, _agent_node, _comms_root);
}

Handle AgentComms::storeMessageAtom(const AgentMessage& msg)
{
    Handle msg_node = _atomspace->add_node(CONCEPT_NODE, "message:" + msg.id);
    Handle sender = _atomspace->add_node(CONCEPT_NODE, "agent:" + msg.sender_id);
    Handle recipient = _atomspace->add_node(
        CONCEPT_NODE,
        msg.recipient_id.empty() ? "agent:*" : "agent:" + msg.recipient_id);
    Handle content = _atomspace->add_node(CONCEPT_NODE, "content:" + msg.content);
    Handle type_node = _atomspace->add_node(CONCEPT_NODE,
                                            "msgtype:" + messageTypeToString(msg.type));

    Handle sent_pred = _atomspace->add_node(PREDICATE_NODE, "agent-message");
    Handle payload = _atomspace->add_link(
        LIST_LINK, HandleSeq{sender, recipient, content, type_node});
    _atomspace->add_link(EVALUATION_LINK, sent_pred, payload);
    _atomspace->add_link(MEMBER_LINK, msg_node, _comms_root);
    return msg_node;
}

AgentMessage AgentComms::makeMessage(const std::string& recipient,
                                     const std::string& content,
                                     MessageType type,
                                     MessagePriority priority) const
{
    AgentMessage msg;
    msg.id = generateMessageId();
    msg.sender_id = _agent_id;
    msg.recipient_id = recipient;
    msg.type = type;
    msg.priority = priority;
    msg.content = content;
    msg.timestamp = std::chrono::system_clock::now();
    return msg;
}

// ---------------------------------------------------------------------------
// Peer registry
// ---------------------------------------------------------------------------

bool AgentComms::registerPeer(const std::string& peer_id)
{
    if (peer_id.empty() || peer_id == _agent_id) return false;
    std::lock_guard<std::mutex> lock(_mutex);
    _peers[peer_id] = true;
    Handle peer = _atomspace->add_node(CONCEPT_NODE, "agent:" + peer_id);
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "peer-of");
    _atomspace->add_link(EVALUATION_LINK, pred,
                         _atomspace->add_link(LIST_LINK, _agent_node, peer));
    return true;
}

bool AgentComms::unregisterPeer(const std::string& peer_id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _peers.erase(peer_id) > 0;
}

std::vector<std::string> AgentComms::listPeers() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<std::string> out;
    out.reserve(_peers.size());
    for (const auto& kv : _peers)
        if (kv.second) out.push_back(kv.first);
    return out;
}

bool AgentComms::isPeerRegistered(const std::string& peer_id) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _peers.find(peer_id);
    return it != _peers.end() && it->second;
}

// ---------------------------------------------------------------------------
// Messaging
// ---------------------------------------------------------------------------

bool AgentComms::deliver(const AgentMessage& message)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_inbox.size() >= _max_inbox) {
        logger().warn() << "[AgentComms] inbox full; dropping message " << message.id;
        return false;
    }
    AgentMessage copy = message;
    if (copy.atom == Handle::UNDEFINED)
        copy.atom = storeMessageAtom(copy);
    _inbox.push_back(copy);
    ++_received_count;
    return true;
}

bool AgentComms::sendMessage(const AgentMessage& message)
{
    // Local loopback when recipient is self or broadcast marker handled by caller.
    AgentMessage msg = message;
    if (msg.id.empty()) msg.id = generateMessageId();
    if (msg.sender_id.empty()) msg.sender_id = _agent_id;
    if (msg.timestamp.time_since_epoch().count() == 0)
        msg.timestamp = std::chrono::system_clock::now();

    // AtomSpace write + counter update happen before any inbox deliver so we
    // never call deliver() (which locks _mutex) while already holding it.
    msg.atom = storeMessageAtom(msg);
    {
        std::lock_guard<std::mutex> lock(_mutex);
        ++_sent_count;
    }

    // Loopback to self / broadcast marker (deliver acquires _mutex itself).
    if (msg.recipient_id == _agent_id || msg.recipient_id == "*") {
        deliver(msg);
    }
    // For unit tests / single-process use, unknown peers still "send" successfully
    // after AtomSpace logging; multi-process transport is out of scope here.
    return true;
}

bool AgentComms::send(const std::string& recipient_id,
                      const std::string& content,
                      MessageType type,
                      MessagePriority priority)
{
    if (content.empty()) return false;
    return sendMessage(makeMessage(recipient_id, content, type, priority));
}

bool AgentComms::broadcast(const std::string& content, MessageType type)
{
    if (content.empty()) return false;
    AgentMessage msg = makeMessage("*", content, type, MessagePriority::NORMAL);
    // Fan-out via AtomSpace log + local deliver. Keep storeMessageAtom outside
    // _mutex so deliver() never nests the same non-recursive lock.
    msg.atom = storeMessageAtom(msg);
    {
        std::lock_guard<std::mutex> lock(_mutex);
        ++_sent_count;
    }
    // Always deliver a copy to self so dispatch/tests observe broadcasts.
    deliver(msg);
    return true;
}

bool AgentComms::receive(AgentMessage& out)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_inbox.empty()) return false;
    out = _inbox.front();
    _inbox.pop_front();
    return true;
}

size_t AgentComms::inboxSize() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _inbox.size();
}

void AgentComms::clearInbox()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _inbox.clear();
}

void AgentComms::setHandler(MessageType type, MessageHandlerFn handler)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _handlers[type] = std::move(handler);
}

void AgentComms::clearHandler(MessageType type)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _handlers.erase(type);
}

size_t AgentComms::dispatch(size_t max_messages)
{
    size_t handled = 0;
    for (size_t i = 0; i < max_messages; ++i) {
        AgentMessage msg;
        MessageHandlerFn handler;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_inbox.empty()) break;
            msg = _inbox.front();
            _inbox.pop_front();
            auto it = _handlers.find(msg.type);
            if (it != _handlers.end()) handler = it->second;
        }
        if (handler) handler(msg);
        ++handled;
    }
    return handled;
}

size_t AgentComms::getSentCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _sent_count;
}

size_t AgentComms::getReceivedCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _received_count;
}
