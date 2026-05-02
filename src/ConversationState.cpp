/*
 * src/ConversationState.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ConversationState Implementation
 * Manages state and memory for individual conversations
 * Part of the AGENT-ZERO-GENESIS project Phase 6: Communication & NLP
 */

#include <algorithm>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/ConversationState.h"

using opencog::HandleSeq;

using namespace opencog;
using namespace opencog::agentzero;

ConversationState::ConversationState(const std::string& conversation_id, AtomSpacePtr atomspace)
    : _conversation_id(conversation_id)
    , _atomspace(atomspace)
    , _is_active(true)
    , _last_activity(std::chrono::system_clock::now())
    , _conversation_atom(Handle::UNDEFINED)
    , _context_atom(Handle::UNDEFINED)
    , _participants_atom(Handle::UNDEFINED)
{
    // Create AtomSpace representation
    _conversation_atom = _atomspace->add_node(CONCEPT_NODE, std::string("Conversation:" + _conversation_id));
    _context_atom = _atomspace->add_node(CONCEPT_NODE, std::string("Context:" + _conversation_id));
    _participants_atom = _atomspace->add_node(CONCEPT_NODE, std::string("Participants:" + _conversation_id));
    
    // Link context and participants to conversation
    _atomspace->add_link(MEMBER_LINK, HandleSeq{_context_atom, _conversation_atom});
    _atomspace->add_link(MEMBER_LINK, HandleSeq{_participants_atom, _conversation_atom});
    
    logger().debug("ConversationState created for %s", _conversation_id.c_str());
}

ConversationState::~ConversationState() {
    logger().debug("ConversationState destroyed for %s", _conversation_id.c_str());
}

void ConversationState::updateActivity() {
    _last_activity = std::chrono::system_clock::now();
}

void ConversationState::setActive(bool active) {
    _is_active = active;
    if (active) {
        updateActivity();
    }
}

bool ConversationState::isActive() const {
    return _is_active;
}

void ConversationState::setContext(const std::string& key, const std::string& value) {
    _context_variables[key] = value;
    updateActivity();
    
    // Update AtomSpace representation
    Handle key_atom = _atomspace->add_node(CONCEPT_NODE, std::string("ContextKey:" + key));
    Handle value_atom = _atomspace->add_node(CONCEPT_NODE, std::string("ContextValue:" + value));
    
    _atomspace->add_link(EVALUATION_LINK, HandleSeq{
        _atomspace->add_node(PREDICATE_NODE, std::string("hasContextValue")),
        _atomspace->add_link(LIST_LINK, HandleSeq{_context_atom, key_atom, value_atom})
    });
}

std::string ConversationState::getContext(const std::string& key) const {
    auto it = _context_variables.find(key);
    return (it != _context_variables.end()) ? it->second : "";
}

void ConversationState::removeContext(const std::string& key) {
    _context_variables.erase(key);
    updateActivity();
}

std::unordered_map<std::string, std::string> ConversationState::getAllContext() const {
    return _context_variables;
}

void ConversationState::addParticipant(const std::string& participant_id) {
    if (std::find(_participants.begin(), _participants.end(), participant_id) == _participants.end()) {
        _participants.push_back(participant_id);
        updateActivity();
        
        // Update AtomSpace representation
        Handle participant_atom = _atomspace->add_node(CONCEPT_NODE, std::string(participant_id));
        _atomspace->add_link(MEMBER_LINK, HandleSeq{participant_atom, _participants_atom});
    }
}

void ConversationState::removeParticipant(const std::string& participant_id) {
    auto it = std::find(_participants.begin(), _participants.end(), participant_id);
    if (it != _participants.end()) {
        _participants.erase(it);
        updateActivity();
    }
}

std::vector<std::string> ConversationState::getParticipants() const {
    return _participants;
}

bool ConversationState::hasParticipant(const std::string& participant_id) const {
    return std::find(_participants.begin(), _participants.end(), participant_id) != _participants.end();
}

void ConversationState::setTopic(const std::string& topic) {
    _current_topic = topic;
    updateActivity();
    
    // Update AtomSpace representation
    if (!topic.empty()) {
        Handle topic_atom = _atomspace->add_node(CONCEPT_NODE, std::string("Topic:" + topic));
        _atomspace->add_link(EVALUATION_LINK, HandleSeq{
            _atomspace->add_node(PREDICATE_NODE, std::string("hasTopic"),
            _atomspace->add_link(LIST_LINK, HandleSeq{_conversation_atom, topic_atom})
        });
    }
}

std::string ConversationState::getTopic() const {
    return _current_topic;
}

Handle ConversationState::getConversationAtom() const {
    return _conversation_atom;
}

void ConversationState::updateAtomSpace() {
    // Update activity timestamp in AtomSpace
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    
    Handle timestamp_atom = _atomspace->add_node(NUMBER_NODE, std::string(std::to_stringtimestamp))
    _atomspace->add_link(EVALUATION_LINK, HandleSeq{
        _atomspace->add_node(PREDICATE_NODE, std::string("lastActivity"),
        _atomspace->add_link(LIST_LINK, HandleSeq{_conversation_atom, timestamp_atom})
    });
    
    // Update active status
    Handle active_atom = _atomspace->add_node(CONCEPT_NODE, _is_active ? "Active" : "Inactive");
    _atomspace->add_link(EVALUATION_LINK, HandleSeq{
        _atomspace->add_node(PREDICATE_NODE, std::string("hasStatus"),
        _atomspace->add_link(LIST_LINK, HandleSeq{_conversation_atom, active_atom})
    });
}

std::string ConversationState::getConversationId() const {
    return _conversation_id;
}

std::chrono::system_clock::time_point ConversationState::getLastActivity() const {
    return _last_activity;
}