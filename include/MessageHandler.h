/*
 * opencog/agentzero/MessageHandler.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * MessageHandler - Handles message routing and processing
 * Part of the AGENT-ZERO-GENESIS project Phase 6: Communication & NLP
 */

#ifndef _OPENCOG_AGENTZERO_MESSAGE_HANDLER_H
#define _OPENCOG_AGENTZERO_MESSAGE_HANDLER_H

#include <string>
#include <vector>
#include <functional>
#include <memory>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

namespace opencog {
namespace agentzero {

// Forward declaration
struct Message;

/**
 * MessageHandler - Message routing and processing
 *
 * This class handles the routing and processing of messages within
 * the dialogue system, including validation, transformation, and
 * integration with the AtomSpace.
 */
class MessageHandler {
public:
    // Callback type for message processing
    using MessageCallback = std::function<void(const Message&)>;
    
private:
    AtomSpacePtr _atomspace;
    
    // Message processing callbacks
    std::vector<MessageCallback> _message_callbacks;
    
public:
    MessageHandler(AtomSpacePtr atomspace);
    virtual ~MessageHandler();
    
    // Message processing
    bool processMessage(const Message& message);
    Handle messageToAtom(const Message& message);
    Message atomToMessage(const Handle& message_atom);
    
    // Callback management
    void addMessageCallback(MessageCallback callback);
    void removeAllCallbacks();
    
    // Validation
    bool validateMessage(const Message& message) const;
    
    // AtomSpace operations
    std::vector<Handle> findMessagesByParticipant(const std::string& participant_id);
    std::vector<Handle> findMessagesByTimeRange(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end);
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_MESSAGE_HANDLER_H