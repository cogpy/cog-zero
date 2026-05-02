/*
 * src/DialogueManager.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * DialogueManager Implementation
 * Manages conversations and dialogue state with AtomSpace integration
 * Part of the AGENT-ZERO-GENESIS project Phase 6: Communication & NLP
 */

#include <sstream>
#include <algorithm>
#include <random>
#include <iomanip>
#include <ctime>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/DialogueManager.h"
#include "opencog/agentzero/ConversationState.h"
#include "opencog/agentzero/LanguageProcessor.h"
#include "opencog/agentzero/MessageHandler.h"

using namespace opencog;
using namespace opencog::agentzero;
using opencog::HandleSeq;

// Message implementation
std::string Message::generateMessageId() const {
    // Generate timestamp-based ID with random component
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    std::stringstream ss;
    ss << "msg_" << time_t << "_" << ms.count() << "_" << dis(gen);
    return ss.str();
}

// DialogueManager implementation
DialogueManager::DialogueManager(AtomSpacePtr atomspace, const std::string& agent_id)
    : _atomspace(atomspace ? atomspace : createAtomSpace())
    , _agent_id(agent_id)
    , _max_conversation_history(1000)
    , _conversation_timeout(std::chrono::minutes(30))
    , _enable_context_tracking(true)
    , _enable_goal_oriented_dialogue(true)
    , _agent_self_atom(Handle::UNDEFINED)
    , _dialogue_context_atom(Handle::UNDEFINED)
    , _conversation_memory_atom(Handle::UNDEFINED)
    , _active_conversations_atom(Handle::UNDEFINED)
{
    logger().info("DialogueManager initializing for agent: %s", _agent_id.c_str());
    
    // Initialize components
    _language_processor = std::make_unique<LanguageProcessor>(_atomspace);
    _message_handler = std::make_unique<MessageHandler>(_atomspace);
    
    // Initialize AtomSpace representation
    initializeDialogueAtoms();
    
    logger().info("DialogueManager initialized successfully");
}

DialogueManager::~DialogueManager() {
    logger().info("DialogueManager shutting down");
    
    // Clean up active conversations
    for (auto& [conv_id, context] : _active_conversations) {
        if (context->is_active) {
            endConversation(conv_id);
        }
    }
    
    _active_conversations.clear();
    _conversation_history.clear();
    
    logger().info("DialogueManager shutdown complete");
}

void DialogueManager::initializeDialogueAtoms() {
    // Create agent self-representation atom
    _agent_self_atom = _atomspace->add_node(CONCEPT_NODE, std::string(_agent_id));
    _agent_self_atom->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    
    // Create core dialogue management atoms
    _dialogue_context_atom = _atomspace->add_node(CONCEPT_NODE, std::string("DialogueContext"));
    _conversation_memory_atom = _atomspace->add_node(CONCEPT_NODE, std::string("ConversationMemory"));
    _active_conversations_atom = _atomspace->add_node(CONCEPT_NODE, std::string("ActiveConversations"));
    
    // Create relationship links
    _atomspace->add_link(MEMBER_LINK, 
        {_dialogue_context_atom, _agent_self_atom});
    _atomspace->add_link(MEMBER_LINK, 
        {_conversation_memory_atom, _agent_self_atom});
    _atomspace->add_link(MEMBER_LINK, 
        {_active_conversations_atom, _agent_self_atom});
    
    logger().info("DialogueManager atoms initialized in AtomSpace");
}

bool DialogueManager::startConversation(const std::string& conversation_id,
                                       const std::vector<std::string>& participants) {
    if (conversation_id.empty()) {
        logger().error("Cannot start conversation with empty ID");
        return false;
    }
    
    // Check if conversation already exists
    if (_active_conversations.find(conversation_id) != _active_conversations.end()) {
        logger().warn("Conversation %s already exists", conversation_id.c_str());
        return false;
    }
    
    // Create new conversation context
    auto context = std::make_unique<DialogueContext>(conversation_id);
    context->participants = participants;
    context->is_active = true;
    context->last_activity = std::chrono::system_clock::now();
    
    // Create AtomSpace representation
    createConversationAtoms(conversation_id);
    
    // Add participants to context
    for (const auto& participant : participants) {
        if (participant != _agent_id) {
            context->participants.push_back(participant);
        }
    }
    
    _active_conversations[conversation_id] = std::move(context);
    
    logger().info("Started conversation %s with %zu participants", 
                  conversation_id.c_str(), participants.size());
    
    return true;
}

bool DialogueManager::endConversation(const std::string& conversation_id) {
    auto it = _active_conversations.find(conversation_id);
    if (it == _active_conversations.end()) {
        logger().warn("Conversation %s not found", conversation_id.c_str());
        return false;
    }
    
    // Mark conversation as inactive
    it->second->is_active = false;
    
    // Archive conversation history (keep it for reference)
    logger().info("Ended conversation %s", conversation_id.c_str());
    
    // Remove from active conversations but keep in history
    _active_conversations.erase(it);
    
    return true;
}

std::string DialogueManager::processMessage(const std::string& conversation_id,
                                           const std::string& sender_id,
                                           const std::string& message_content) {
    if (conversation_id.empty() || sender_id.empty() || message_content.empty()) {
        logger().error("Invalid message parameters");
        return "Error: Invalid message parameters";
    }
    
    // Get or create conversation
    DialogueContext* context = getOrCreateConversation(conversation_id);
    if (!context) {
        logger().error("Failed to get or create conversation %s", conversation_id.c_str());
        return "Error: Could not access conversation";
    }
    
    // Create message object
    Message incoming_message(sender_id, _agent_id, message_content);
    
    // Store message in conversation history
    _conversation_history[conversation_id].push_back(incoming_message);
    
    // Update conversation context
    updateConversationContext(conversation_id, incoming_message);
    
    // Process message and generate response
    std::string response = generateResponse(conversation_id, incoming_message);
    
    // Create response message
    if (!response.empty()) {
        Message response_message(_agent_id, sender_id, response);
        _conversation_history[conversation_id].push_back(response_message);
        
        // Create AtomSpace representation
        createMessageAtom(response_message);
    }
    
    // Cleanup old conversations periodically
    static int message_count = 0;
    if (++message_count % 100 == 0) {
        cleanupInactiveConversations();
    }
    
    return response;
}

bool DialogueManager::sendMessage(const std::string& conversation_id,
                                 const std::string& recipient_id,
                                 const std::string& message_content) {
    if (conversation_id.empty() || recipient_id.empty() || message_content.empty()) {
        logger().error("Invalid send message parameters");
        return false;
    }
    
    // Get conversation
    auto it = _active_conversations.find(conversation_id);
    if (it == _active_conversations.end()) {
        logger().error("Conversation %s not found for sending message", conversation_id.c_str());
        return false;
    }
    
    // Create outgoing message
    Message outgoing_message(_agent_id, recipient_id, message_content);
    
    // Store in conversation history
    _conversation_history[conversation_id].push_back(outgoing_message);
    
    // Create AtomSpace representation
    createMessageAtom(outgoing_message);
    
    // Update conversation activity
    it->second->last_activity = std::chrono::system_clock::now();
    
    logger().info("Sent message in conversation %s to %s", 
                  conversation_id.c_str(), recipient_id.c_str());
    
    return true;
}

void DialogueManager::setConversationContext(const std::string& conversation_id,
                                           const std::string& key,
                                           const std::string& value) {
    DialogueContext* context = getOrCreateConversation(conversation_id);
    if (context && _enable_context_tracking) {
        context->context_variables[key] = value;
        logger().debug("Set context %s=%s for conversation %s", 
                      key.c_str(), value.c_str(), conversation_id.c_str());
    }
}

std::string DialogueManager::getConversationContext(const std::string& conversation_id,
                                                   const std::string& key) const {
    auto it = _active_conversations.find(conversation_id);
    if (it != _active_conversations.end()) {
        auto context_it = it->second->context_variables.find(key);
        if (context_it != it->second->context_variables.end()) {
            return context_it->second;
        }
    }
    return "";
}

void DialogueManager::setConversationTopic(const std::string& conversation_id,
                                         const std::string& topic) {
    DialogueContext* context = getOrCreateConversation(conversation_id);
    if (context) {
        context->topic = topic;
        logger().info("Set topic '%s' for conversation %s", 
                     topic.c_str(), conversation_id.c_str());
    }
}

std::string DialogueManager::getConversationTopic(const std::string& conversation_id) const {
    auto it = _active_conversations.find(conversation_id);
    if (it != _active_conversations.end()) {
        return it->second->topic;
    }
    return "";
}

void DialogueManager::addConversationGoal(const std::string& conversation_id, 
                                        const Handle& goal_atom) {
    DialogueContext* context = getOrCreateConversation(conversation_id);
    if (context && _enable_goal_oriented_dialogue && goal_atom != Handle::UNDEFINED) {
        context->active_goals.push(goal_atom);
        logger().info("Added goal to conversation %s", conversation_id.c_str());
    }
}

void DialogueManager::removeConversationGoal(const std::string& conversation_id,
                                           const Handle& goal_atom) {
    auto it = _active_conversations.find(conversation_id);
    if (it != _active_conversations.end() && goal_atom != Handle::UNDEFINED) {
        // Note: std::queue doesn't have remove, so we'd need to rebuild it
        // For now, just log the removal request
        logger().info("Goal removal requested for conversation %s", conversation_id.c_str());
    }
}

std::vector<Handle> DialogueManager::getConversationGoals(const std::string& conversation_id) const {
    std::vector<Handle> goals;
    auto it = _active_conversations.find(conversation_id);
    if (it != _active_conversations.end()) {
        // Convert queue to vector (note: this is a copy, queue is not modified)
        std::queue<Handle> temp_queue = it->second->active_goals;
        while (!temp_queue.empty()) {
            goals.push_back(temp_queue.front());
            temp_queue.pop();
        }
    }
    return goals;
}

std::vector<Message> DialogueManager::getConversationHistory(const std::string& conversation_id,
                                                           size_t limit) const {
    auto it = _conversation_history.find(conversation_id);
    if (it == _conversation_history.end()) {
        return {};
    }
    
    const auto& history = it->second;
    if (limit == 0 || limit >= history.size()) {
        return history;
    }
    
    // Return the most recent 'limit' messages
    size_t start_index = history.size() - limit;
    return std::vector<Message>(history.begin() + start_index, history.end());
}

std::vector<Message> DialogueManager::searchMessageHistory(const std::string& conversation_id,
                                                         const std::string& search_term) const {
    std::vector<Message> results;
    
    if (conversation_id.empty()) {
        // Search all conversations
        for (const auto& [conv_id, history] : _conversation_history) {
            for (const auto& message : history) {
                if (message.content.find(search_term) != std::string::npos) {
                    results.push_back(message);
                }
            }
        }
    } else {
        // Search specific conversation
        auto it = _conversation_history.find(conversation_id);
        if (it != _conversation_history.end()) {
            for (const auto& message : it->second) {
                if (message.content.find(search_term) != std::string::npos) {
                    results.push_back(message);
                }
            }
        }
    }
    
    return results;
}

void DialogueManager::clearConversationHistory(const std::string& conversation_id, 
                                             size_t keep_recent) {
    auto it = _conversation_history.find(conversation_id);
    if (it != _conversation_history.end()) {
        auto& history = it->second;
        if (keep_recent > 0 && history.size() > keep_recent) {
            // Keep only the most recent messages
            size_t remove_count = history.size() - keep_recent;
            history.erase(history.begin(), history.begin() + remove_count);
        } else if (keep_recent == 0) {
            // Clear all history
            history.clear();
        }
        logger().info("Cleared conversation history for %s, kept %zu recent messages", 
                     conversation_id.c_str(), keep_recent);
    }
}

std::vector<std::string> DialogueManager::getActiveConversations() const {
    std::vector<std::string> active_convs;
    for (const auto& [conv_id, context] : _active_conversations) {
        if (context->is_active) {
            active_convs.push_back(conv_id);
        }
    }
    return active_convs;
}

bool DialogueManager::isConversationActive(const std::string& conversation_id) const {
    auto it = _active_conversations.find(conversation_id);
    return it != _active_conversations.end() && it->second->is_active;
}

std::vector<std::string> DialogueManager::getConversationParticipants(const std::string& conversation_id) const {
    auto it = _active_conversations.find(conversation_id);
    if (it != _active_conversations.end()) {
        return it->second->participants;
    }
    return {};
}

std::string DialogueManager::getStatusInfo() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"agent_id\": \"" << _agent_id << "\",\n";
    ss << "  \"active_conversations\": " << _active_conversations.size() << ",\n";
    ss << "  \"total_conversations\": " << _conversation_history.size() << ",\n";
    ss << "  \"max_history\": " << _max_conversation_history << ",\n";
    ss << "  \"timeout_minutes\": " << _conversation_timeout.count() << ",\n";
    ss << "  \"context_tracking\": " << (_enable_context_tracking ? "true" : "false") << ",\n";
    ss << "  \"goal_oriented\": " << (_enable_goal_oriented_dialogue ? "true" : "false") << ",\n";
    ss << "  \"conversations\": [\n";
    
    bool first = true;
    for (const auto& [conv_id, context] : _active_conversations) {
        if (!first) ss << ",\n";
        first = false;
        
        auto history_it = _conversation_history.find(conv_id);
        size_t message_count = (history_it != _conversation_history.end()) ? 
                              history_it->second.size() : 0;
        
        ss << "    {\n";
        ss << "      \"id\": \"" << conv_id << "\",\n";
        ss << "      \"active\": " << (context->is_active ? "true" : "false") << ",\n";
        ss << "      \"participants\": " << context->participants.size() << ",\n";
        ss << "      \"messages\": " << message_count << ",\n";
        ss << "      \"topic\": \"" << context->topic << "\"\n";
        ss << "    }";
    }
    
    ss << "\n  ]\n";
    ss << "}";
    
    return ss.str();
}

void DialogueManager::setMaxConversationHistory(size_t max_history) {
    _max_conversation_history = max_history;
    logger().info("Set max conversation history to %zu", max_history);
}

void DialogueManager::setConversationTimeout(std::chrono::minutes timeout_minutes) {
    _conversation_timeout = timeout_minutes;
    logger().info("Set conversation timeout to %ld minutes", timeout_minutes.count());
}

void DialogueManager::setContextTracking(bool enabled) {
    _enable_context_tracking = enabled;
    logger().info("Context tracking %s", enabled ? "enabled" : "disabled");
}

void DialogueManager::setGoalOrientedDialogue(bool enabled) {
    _enable_goal_oriented_dialogue = enabled;
    logger().info("Goal-oriented dialogue %s", enabled ? "enabled" : "disabled");
}

Handle DialogueManager::conversationToAtom(const std::string& conversation_id) {
    // Create or get conversation atom
    Handle conv_atom = _atomspace->add_node(CONCEPT_NODE, std::string("Conversation:" + conversation_id));
    
    // Link to active conversations
    _atomspace->add_link(MEMBER_LINK, HandleSeq{conv_atom, _active_conversations_atom});
    
    return conv_atom;
}

void DialogueManager::updateDialogueAtoms() {
    // Update AtomSpace representation with current state
    for (const auto& [conv_id, context] : _active_conversations) {
        Handle conv_atom = conversationToAtom(conv_id);
        
        // Update conversation properties
        if (!context->topic.empty()) {
            Handle topic_atom = _atomspace->add_node(CONCEPT_NODE, std::string("Topic:" + context->topic));
            _atomspace->add_link(EVALUATION_LINK, HandleSeq{
                _atomspace->add_node(PREDICATE_NODE, std::string("hasTopic")),
                _atomspace->add_link(LIST_LINK, HandleSeq{conv_atom, topic_atom})
            });
        }
        
        // Add participants
        for (const auto& participant : context->participants) {
            Handle participant_atom = _atomspace->add_node(CONCEPT_NODE, std::string(participant));
            _atomspace->add_link(EVALUATION_LINK, HandleSeq{
                _atomspace->add_node(PREDICATE_NODE, std::string("participatesIn")),
                _atomspace->add_link(LIST_LINK, HandleSeq{participant_atom, conv_atom})
            });
        }
    }
}

// Private helper methods

void DialogueManager::createConversationAtoms(const std::string& conversation_id) {
    Handle conv_atom = _atomspace->add_node(CONCEPT_NODE, std::string("Conversation:" + conversation_id));
    conv_atom->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    
    // Link to agent and active conversations
    _atomspace->add_link(MEMBER_LINK, HandleSeq{conv_atom, _active_conversations_atom});
    _atomspace->add_link(EVALUATION_LINK, HandleSeq{
        _atomspace->add_node(PREDICATE_NODE, std::string("manages")),
        _atomspace->add_link(LIST_LINK, HandleSeq{_agent_self_atom, conv_atom})
    });
}

Handle DialogueManager::createMessageAtom(const Message& message) {
    Handle msg_atom = _atomspace->add_node(CONCEPT_NODE, std::string("Message:" + message.id));
    msg_atom->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    
    // Store message properties
    Handle sender_atom = _atomspace->add_node(CONCEPT_NODE, std::string(message.sender_id));
    Handle recipient_atom = _atomspace->add_node(CONCEPT_NODE, std::string(message.recipient_id));
    Handle content_atom = _atomspace->add_node(CONCEPT_NODE, std::string("Content:" + message.content));
    
    // Create relationships
    _atomspace->add_link(EVALUATION_LINK, HandleSeq{
        _atomspace->add_node(PREDICATE_NODE, std::string("sentBy")),
        _atomspace->add_link(LIST_LINK, HandleSeq{msg_atom, sender_atom})
    });
    
    _atomspace->add_link(EVALUATION_LINK, HandleSeq{
        _atomspace->add_node(PREDICATE_NODE, std::string("sentTo")),
        _atomspace->add_link(LIST_LINK, HandleSeq{msg_atom, recipient_atom})
    });
    
    _atomspace->add_link(EVALUATION_LINK, HandleSeq{
        _atomspace->add_node(PREDICATE_NODE, std::string("hasContent")),
        _atomspace->add_link(LIST_LINK, HandleSeq{msg_atom, content_atom})
    });
    
    return msg_atom;
}

void DialogueManager::updateConversationContext(const std::string& conversation_id, 
                                              const Message& message) {
    auto it = _active_conversations.find(conversation_id);
    if (it != _active_conversations.end()) {
        it->second->last_activity = message.timestamp;
        
        // Simple context updates based on message content
        if (_enable_context_tracking) {
            // Extract potential context from message
            // This is a simple implementation - could be enhanced with NLP
            std::string content_lower = message.content;
            std::transform(content_lower.begin(), content_lower.end(), 
                          content_lower.begin(), ::tolower);
            
            // Look for topic indicators
            if (content_lower.find("about") != std::string::npos ||
                content_lower.find("regarding") != std::string::npos) {
                // Potential topic change - could be enhanced with proper NLP
                setConversationContext(conversation_id, "last_topic_mention", message.content);
            }
        }
    }
}

std::string DialogueManager::generateResponse(const std::string& conversation_id,
                                            const Message& input_message) {
    // Simple response generation - can be enhanced with more sophisticated NLP
    std::string response;
    
    // Get conversation context
    std::string topic = getConversationTopic(conversation_id);
    
    // Use language processor if available
    if (_language_processor) {
        std::string context = topic.empty() ? "" : "Topic: " + topic;
        response = _language_processor->generateResponse(input_message.content, context);
    }
    
    // Fallback to simple responses
    if (response.empty()) {
        // Simple rule-based responses
        std::string content_lower = input_message.content;
        std::transform(content_lower.begin(), content_lower.end(), 
                      content_lower.begin(), ::tolower);
        
        if (content_lower.find("hello") != std::string::npos ||
            content_lower.find("hi") != std::string::npos) {
            response = "Hello! How can I help you today?";
        } else if (content_lower.find("bye") != std::string::npos ||
                   content_lower.find("goodbye") != std::string::npos) {
            response = "Goodbye! It was nice talking with you.";
        } else if (content_lower.find("?") != std::string::npos) {
            response = "That's an interesting question. Let me think about that.";
        } else {
            response = "I understand. Please tell me more about that.";
        }
    }
    
    return response;
}

void DialogueManager::cleanupInactiveConversations() {
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> to_remove;
    
    for (const auto& [conv_id, context] : _active_conversations) {
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
            now - context->last_activity);
        
        if (elapsed > _conversation_timeout) {
            to_remove.push_back(conv_id);
        }
    }
    
    for (const auto& conv_id : to_remove) {
        logger().info("Cleaning up inactive conversation: %s", conv_id.c_str());
        endConversation(conv_id);
    }
}

DialogueContext* DialogueManager::getOrCreateConversation(const std::string& conversation_id) {
    auto it = _active_conversations.find(conversation_id);
    if (it != _active_conversations.end()) {
        return it->second.get();
    }
    
    // Create new conversation with just the agent and unknown participant
    std::vector<std::string> participants = {_agent_id, "unknown"};
    if (startConversation(conversation_id, participants)) {
        it = _active_conversations.find(conversation_id);
        if (it != _active_conversations.end()) {
            return it->second.get();
        }
    }
    
    return nullptr;
}

/*
 * DialogueManager.cpp (agentzero::communication)
 *
 * Copyright (C) 2024 Agent-Zero-Genesis Project
 * 
 * Conversational interaction management for Agent-Zero
 */

#include "agentzero/communication/DialogueManager.h"

#include <algorithm>
#include <sstream>
#include <regex>

namespace agentzero {
namespace communication {

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

DialogueManager::DialogueManager(opencog::AtomSpacePtr atomspace)
    : _atomspace(atomspace), _turn_count(0)
{
}

// -----------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------

std::string DialogueManager::process_dialogue(const std::string& input)
{
    if (input.empty()) {
        return "I didn't receive any input. Please say something.";
    }

    ++_turn_count;
    std::string response = generate_response(input);

    _history.emplace_back(input, response);
    store_turn_in_atomspace(input, response);

    return response;
}

void DialogueManager::reset_context()
{
    _history.clear();
    _context.clear();
    _turn_count = 0;
}

size_t DialogueManager::get_turn_count() const
{
    return _turn_count;
}

const std::vector<DialogueManager::Turn>& DialogueManager::get_history() const
{
    return _history;
}

void DialogueManager::set_context(const std::string& key, const std::string& value)
{
    _context[key] = value;
}

std::string DialogueManager::get_context(const std::string& key) const
{
    auto it = _context.find(key);
    return (it != _context.end()) ? it->second : "";
}

// -----------------------------------------------------------------------
// Private helpers
// -----------------------------------------------------------------------

std::string DialogueManager::generate_response(const std::string& input)
{
    // Lowercase copy for pattern matching
    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Compiled once per process — static const avoids repeated regex construction.
    static const std::regex re_greeting(R"(\b(hello|hi|hey|greetings|good\s+(morning|afternoon|evening))\b)");
    static const std::regex re_farewell(R"(\b(bye|goodbye|farewell|see\s+you|take\s+care)\b)");
    static const std::regex re_gratitude(R"(\b(thank(s|\s+you)?|appreciate|grateful)\b)");
    static const std::regex re_identity(R"(\b(who\s+are\s+you|your\s+name|what\s+are\s+you)\b)");
    static const std::regex re_help(R"(\b(help|assist|support|guide)\b)");
    static const std::regex re_how_are_you(R"(\bhow\s+are\s+you\b)");
    static const std::regex re_question(R"(\b(what|how|when|where|why|who|which|can|could|would|is|are|do|does)\b)");

    if (std::regex_search(lower, re_greeting)) {
        return "Hello! How can I assist you today?";
    }
    if (std::regex_search(lower, re_farewell)) {
        return "Goodbye! Feel free to return whenever you need assistance.";
    }
    if (std::regex_search(lower, re_gratitude)) {
        return "You're welcome! Is there anything else I can help with?";
    }
    if (std::regex_search(lower, re_identity)) {
        return "I am Agent-Zero, an AI assistant built on the OpenCog cognitive architecture.";
    }
    if (std::regex_search(lower, re_help)) {
        return "I'm here to help. Could you provide more details about what you need?";
    }
    if (std::regex_search(lower, re_how_are_you)) {
        return "I'm functioning well, thank you for asking! How can I assist you?";
    }

    // Question — use conversation history for context
    if (lower.find('?') != std::string::npos ||
        std::regex_search(lower, re_question)) {
        if (!_history.empty()) {
            return "That's a good question. Based on our conversation so far, I'll do my best to help. Could you elaborate further?";
        }
        return "That's an interesting question. Could you provide more context so I can give you a better answer?";
    }

    // Default contextual response
    if (_turn_count > 1) {
        return "I understand. Is there anything specific you would like me to do with that information?";
    }
    return "I see. How can I help you further?";
}

void DialogueManager::store_turn_in_atomspace(const std::string& input,
                                               const std::string& response)
{
    if (!_atomspace) return;

    try {
        // Represent the dialogue turn as a pair of ConceptNodes linked by
        // an EvaluationLink with predicate "dialogue-turn".
        auto input_node  = _atomspace->add_node(opencog::CONCEPT_NODE,
                                                "utterance:" + input);
        auto output_node = _atomspace->add_node(opencog::CONCEPT_NODE,
                                                "response:" + response);
        auto pred_node   = _atomspace->add_node(opencog::PREDICATE_NODE,
                                                "dialogue-turn");
        opencog::HandleSeq args{input_node, output_node};
        _atomspace->add_link(opencog::EVALUATION_LINK,
                             opencog::HandleSeq{pred_node,
                                               _atomspace->add_link(opencog::LIST_LINK,
                                                                     std::move(args))});
    } catch (...) {
        // AtomSpace errors are non-fatal for dialogue management
    }
}

} // namespace communication
} // namespace agentzero
