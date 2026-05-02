/*
 * opencog/agentzero/ConversationState.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ConversationState - Manages state and memory for individual conversations
 * Part of the AGENT-ZERO-GENESIS project Phase 6: Communication & NLP
 */

#ifndef _OPENCOG_AGENTZERO_CONVERSATION_STATE_H
#define _OPENCOG_AGENTZERO_CONVERSATION_STATE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

namespace opencog {
namespace agentzero {

/**
 * ConversationState - Manages individual conversation state and context
 *
 * This class maintains the state of a single conversation including
 * context variables, participant information, and temporal tracking.
 */
class ConversationState {
private:
    std::string _conversation_id;
    AtomSpacePtr _atomspace;
    
    // State tracking
    std::unordered_map<std::string, std::string> _context_variables;
    std::vector<std::string> _participants;
    std::string _current_topic;
    std::chrono::system_clock::time_point _last_activity;
    bool _is_active;
    
    // AtomSpace representation
    Handle _conversation_atom;
    Handle _context_atom;
    Handle _participants_atom;
    
public:
    ConversationState(const std::string& conversation_id, AtomSpacePtr atomspace);
    virtual ~ConversationState();
    
    // State management
    void updateActivity();
    void setActive(bool active);
    bool isActive() const;
    
    // Context management
    void setContext(const std::string& key, const std::string& value);
    std::string getContext(const std::string& key) const;
    void removeContext(const std::string& key);
    std::unordered_map<std::string, std::string> getAllContext() const;
    
    // Participant management
    void addParticipant(const std::string& participant_id);
    void removeParticipant(const std::string& participant_id);
    std::vector<std::string> getParticipants() const;
    bool hasParticipant(const std::string& participant_id) const;
    
    // Topic management
    void setTopic(const std::string& topic);
    std::string getTopic() const;
    
    // AtomSpace integration
    Handle getConversationAtom() const;
    void updateAtomSpace();
    
    // Information
    std::string getConversationId() const;
    std::chrono::system_clock::time_point getLastActivity() const;
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_CONVERSATION_STATE_H