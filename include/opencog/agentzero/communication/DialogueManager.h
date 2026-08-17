/*
 * opencog/agentzero/communication/DialogueManager.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * DialogueManager — multi-turn conversation management
 * Part of Phase 6: Communication & NLP (AZ-NLP-002)
 */

#ifndef _OPENCOG_AGENTZERO_COMMUNICATION_DIALOGUE_MANAGER_H
#define _OPENCOG_AGENTZERO_COMMUNICATION_DIALOGUE_MANAGER_H

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/communication/LanguageProcessor.h"

namespace opencog {
namespace agentzero {
namespace communication {

/** Single turn in a conversation. */
struct DialogueTurn {
    std::string id;
    std::string speaker_id;
    std::string content;
    std::string intent;
    std::chrono::system_clock::time_point timestamp;
    Handle atom = Handle::UNDEFINED;
};

/** Runtime state for one multi-turn conversation. */
struct Conversation {
    std::string id;
    std::vector<std::string> participants;
    std::string topic;
    std::map<std::string, std::string> context;
    std::vector<DialogueTurn> history;
    std::vector<Handle> goals;
    std::chrono::system_clock::time_point last_activity;
    bool active = true;
};

/**
 * DialogueManager — multi-turn conversation orchestration.
 *
 * Tracks concurrent conversations, persists turns into AtomSpace,
 * and uses LanguageProcessor for NLU/NLG on each user utterance.
 */
class DialogueManager
{
public:
    explicit DialogueManager(AtomSpacePtr atomspace,
                             const std::string& agent_id = "DialogueAgent");
    virtual ~DialogueManager();

    DialogueManager(const DialogueManager&) = delete;
    DialogueManager& operator=(const DialogueManager&) = delete;

    bool startConversation(const std::string& conversation_id,
                           const std::vector<std::string>& participants);
    bool endConversation(const std::string& conversation_id);
    bool isConversationActive(const std::string& conversation_id) const;

    /**
     * Process an incoming utterance and return the agent reply.
     * Creates the conversation lazily if it does not yet exist.
     */
    std::string processMessage(const std::string& conversation_id,
                               const std::string& sender_id,
                               const std::string& message_content);

    /** Append an outbound agent message without generating a reply. */
    bool sendMessage(const std::string& conversation_id,
                     const std::string& recipient_id,
                     const std::string& message_content);

    void setConversationContext(const std::string& conversation_id,
                                const std::string& key,
                                const std::string& value);
    std::string getConversationContext(const std::string& conversation_id,
                                       const std::string& key) const;

    void setConversationTopic(const std::string& conversation_id,
                              const std::string& topic);
    std::string getConversationTopic(const std::string& conversation_id) const;

    void addConversationGoal(const std::string& conversation_id,
                             const Handle& goal_atom);
    std::vector<Handle> getConversationGoals(const std::string& conversation_id) const;

    std::vector<DialogueTurn> getHistory(const std::string& conversation_id,
                                         size_t limit = 0) const;
    std::vector<std::string> getActiveConversations() const;
    std::vector<std::string> getParticipants(const std::string& conversation_id) const;

    void setMaxHistory(size_t max_history);
    void setTimeout(std::chrono::minutes timeout);

    size_t getActiveCount() const;
    std::string getAgentId() const { return _agent_id; }
    LanguageProcessor& getLanguageProcessor() { return *_language_processor; }
    AtomSpacePtr getAtomSpace() const { return _atomspace; }

    /** Drop conversations idle longer than the configured timeout. */
    size_t cleanupInactiveConversations();

private:
    AtomSpacePtr _atomspace;
    std::string _agent_id;
    std::unique_ptr<LanguageProcessor> _language_processor;

    mutable std::mutex _mutex;
    std::map<std::string, Conversation> _conversations;

    size_t _max_history = 200;
    std::chrono::minutes _timeout{30};

    Handle _agent_self = Handle::UNDEFINED;
    Handle _dialogue_root = Handle::UNDEFINED;

    void initializeAtoms();
    Conversation* getOrCreate(const std::string& conversation_id,
                              const std::string& sender_id);
    DialogueTurn makeTurn(const std::string& speaker,
                          const std::string& content,
                          const std::string& intent);
    Handle storeTurnAtom(const std::string& conversation_id,
                         const DialogueTurn& turn);
    std::string buildContextString(const Conversation& conv) const;
    void trimHistory(Conversation& conv);
};

} // namespace communication
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_COMMUNICATION_DIALOGUE_MANAGER_H
