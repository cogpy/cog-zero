/*
 * opencog/agentzero/DialogueManager.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * DialogueManager - Manages conversations and dialogue state
 * Handles multi-turn conversations with context tracking
 * Part of the AGENT-ZERO-GENESIS project Phase 6: Communication & NLP
 */

#ifndef _OPENCOG_AGENTZERO_DIALOGUE_MANAGER_H
#define _OPENCOG_AGENTZERO_DIALOGUE_MANAGER_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <queue>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {

// Forward declarations
class ConversationState;
class LanguageProcessor;
class MessageHandler;

/**
 * Message represents a single communication unit in a conversation
 */
struct Message {
    std::string id;
    std::string sender_id;
    std::string recipient_id;
    std::string content;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;
    Handle atom_representation;
    
    Message(const std::string& sender, const std::string& recipient, const std::string& text)
        : sender_id(sender), recipient_id(recipient), content(text),
          timestamp(std::chrono::system_clock::now()), 
          atom_representation(Handle::UNDEFINED) {
        // Generate unique ID
        id = generateMessageId();
    }
    
private:
    std::string generateMessageId() const;
};

/**
 * DialogueContext holds contextual information about ongoing conversation
 */
struct DialogueContext {
    std::string conversation_id;
    std::vector<std::string> participants;
    std::string topic;
    std::unordered_map<std::string, std::string> context_variables;
    std::queue<Handle> active_goals;
    std::chrono::system_clock::time_point last_activity;
    bool is_active;
    
    DialogueContext(const std::string& conv_id) 
        : conversation_id(conv_id), is_active(true),
          last_activity(std::chrono::system_clock::now()) {}
};

/**
 * DialogueManager - Main class for managing conversations
 *
 * This class handles multi-turn conversations, maintains dialogue state,
 * and integrates with OpenCog's AtomSpace for knowledge representation.
 * It supports multiple concurrent conversations and provides context
 * tracking and memory management.
 *
 * Key Features:
 * - Multi-turn conversation management
 * - Context tracking and memory
 * - AtomSpace integration for dialogue representation
 * - Natural language processing integration
 * - Goal-oriented dialogue handling
 * - Message history and retrieval
 */
class DialogueManager {
private:
    // Core components
    AtomSpacePtr _atomspace;
    std::unique_ptr<ConversationState> _conversation_state;
    std::unique_ptr<LanguageProcessor> _language_processor;
    std::unique_ptr<MessageHandler> _message_handler;
    
    // Dialogue state management
    std::unordered_map<std::string, std::unique_ptr<DialogueContext>> _active_conversations;
    std::unordered_map<std::string, std::vector<Message>> _conversation_history;
    
    // Configuration
    size_t _max_conversation_history;
    std::chrono::minutes _conversation_timeout;
    bool _enable_context_tracking;
    bool _enable_goal_oriented_dialogue;
    
    // Agent identity
    std::string _agent_id;
    Handle _agent_self_atom;
    
    // Core atoms for dialogue management
    Handle _dialogue_context_atom;
    Handle _conversation_memory_atom;
    Handle _active_conversations_atom;
    
    // Internal methods
    void initializeDialogueAtoms();
    void createConversationAtoms(const std::string& conversation_id);
    Handle createMessageAtom(const Message& message);
    void updateConversationContext(const std::string& conversation_id, const Message& message);
    void processIncomingMessage(const std::string& conversation_id, const Message& message);
    std::string generateResponse(const std::string& conversation_id, const Message& input_message);
    void cleanupInactiveConversations();
    DialogueContext* getOrCreateConversation(const std::string& conversation_id);

public:
    /**
     * Constructor - Creates DialogueManager instance
     * @param atomspace AtomSpace for dialogue representation
     * @param agent_id Identifier for this agent
     */
    DialogueManager(AtomSpacePtr atomspace, const std::string& agent_id = "DialogueAgent");
    
    /**
     * Destructor - Cleans up conversations and resources
     */
    virtual ~DialogueManager();
    
    // Core dialogue operations
    
    /**
     * Start a new conversation
     * @param conversation_id Unique identifier for the conversation
     * @param participants List of participant IDs
     * @return true if conversation started successfully
     */
    bool startConversation(const std::string& conversation_id, 
                          const std::vector<std::string>& participants);
    
    /**
     * End an existing conversation
     * @param conversation_id Identifier of conversation to end
     * @return true if conversation ended successfully
     */
    bool endConversation(const std::string& conversation_id);
    
    /**
     * Process an incoming message and generate response
     * @param conversation_id Conversation this message belongs to
     * @param sender_id ID of the message sender
     * @param message_content Text content of the message
     * @return Generated response message
     */
    std::string processMessage(const std::string& conversation_id,
                              const std::string& sender_id,
                              const std::string& message_content);
    
    /**
     * Send a message in a conversation
     * @param conversation_id Target conversation
     * @param recipient_id ID of the recipient
     * @param message_content Text content to send
     * @return true if message was sent successfully
     */
    bool sendMessage(const std::string& conversation_id,
                    const std::string& recipient_id,
                    const std::string& message_content);
    
    // Context and state management
    
    /**
     * Set context variable for a conversation
     * @param conversation_id Target conversation
     * @param key Context variable name
     * @param value Context variable value
     */
    void setConversationContext(const std::string& conversation_id,
                               const std::string& key,
                               const std::string& value);
    
    /**
     * Get context variable from a conversation
     * @param conversation_id Target conversation
     * @param key Context variable name
     * @return Context variable value, empty string if not found
     */
    std::string getConversationContext(const std::string& conversation_id,
                                      const std::string& key) const;
    
    /**
     * Set the current topic for a conversation
     * @param conversation_id Target conversation
     * @param topic New topic string
     */
    void setConversationTopic(const std::string& conversation_id,
                             const std::string& topic);
    
    /**
     * Get the current topic of a conversation
     * @param conversation_id Target conversation
     * @return Current topic string
     */
    std::string getConversationTopic(const std::string& conversation_id) const;
    
    // Goal-oriented dialogue
    
    /**
     * Add a goal to pursue in the conversation
     * @param conversation_id Target conversation
     * @param goal_atom AtomSpace handle representing the goal
     */
    void addConversationGoal(const std::string& conversation_id, const Handle& goal_atom);
    
    /**
     * Remove a goal from conversation tracking
     * @param conversation_id Target conversation
     * @param goal_atom Goal to remove
     */
    void removeConversationGoal(const std::string& conversation_id, const Handle& goal_atom);
    
    /**
     * Get active goals for a conversation
     * @param conversation_id Target conversation
     * @return Vector of goal atoms
     */
    std::vector<Handle> getConversationGoals(const std::string& conversation_id) const;
    
    // History and retrieval
    
    /**
     * Get message history for a conversation
     * @param conversation_id Target conversation
     * @param limit Maximum number of messages to return (0 = all)
     * @return Vector of messages in chronological order
     */
    std::vector<Message> getConversationHistory(const std::string& conversation_id,
                                               size_t limit = 0) const;
    
    /**
     * Search message history for content
     * @param conversation_id Target conversation (empty = all conversations)
     * @param search_term Text to search for
     * @return Vector of matching messages
     */
    std::vector<Message> searchMessageHistory(const std::string& conversation_id,
                                            const std::string& search_term) const;
    
    /**
     * Clear message history for a conversation
     * @param conversation_id Target conversation
     * @param keep_recent Number of recent messages to keep
     */
    void clearConversationHistory(const std::string& conversation_id, size_t keep_recent = 0);
    
    // Status and information
    
    /**
     * Get list of active conversation IDs
     * @return Vector of conversation identifiers
     */
    std::vector<std::string> getActiveConversations() const;
    
    /**
     * Check if a conversation is currently active
     * @param conversation_id Conversation to check
     * @return true if conversation is active
     */
    bool isConversationActive(const std::string& conversation_id) const;
    
    /**
     * Get participants in a conversation
     * @param conversation_id Target conversation
     * @return Vector of participant IDs
     */
    std::vector<std::string> getConversationParticipants(const std::string& conversation_id) const;
    
    /**
     * Get detailed status information about all conversations
     * @return JSON string with status details
     */
    std::string getStatusInfo() const;
    
    // Configuration
    
    /**
     * Set maximum conversation history length
     * @param max_history Maximum number of messages to keep per conversation
     */
    void setMaxConversationHistory(size_t max_history);
    
    /**
     * Set conversation timeout
     * @param timeout_minutes Minutes of inactivity before conversation is considered inactive
     */
    void setConversationTimeout(std::chrono::minutes timeout_minutes);
    
    /**
     * Enable or disable context tracking
     * @param enabled Whether to track conversation context
     */
    void setContextTracking(bool enabled);
    
    /**
     * Enable or disable goal-oriented dialogue
     * @param enabled Whether to use goal-oriented dialogue management
     */
    void setGoalOrientedDialogue(bool enabled);
    
    // AtomSpace integration
    
    /**
     * Get the AtomSpace used by this DialogueManager
     * @return Shared pointer to AtomSpace
     */
    AtomSpacePtr getAtomSpace() const { return _atomspace; }
    
    /**
     * Get the agent's self-representation atom
     * @return Handle to agent self atom
     */
    Handle getAgentSelfAtom() const { return _agent_self_atom; }
    
    /**
     * Get atom representing dialogue context
     * @return Handle to dialogue context atom
     */
    Handle getDialogueContextAtom() const { return _dialogue_context_atom; }
    
    /**
     * Convert a conversation to AtomSpace representation
     * @param conversation_id Conversation to convert
     * @return Handle to conversation atom
     */
    Handle conversationToAtom(const std::string& conversation_id);
    
    /**
     * Update AtomSpace with current dialogue state
     */
    void updateDialogueAtoms();
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_DIALOGUE_MANAGER_H