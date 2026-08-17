/*
 * opencog/agentzero/communication/HumanInterface.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * HumanInterface — user-facing dialogue layer
 * Part of Phase 6: Communication & NLP (AZ-HUMAN-001)
 */

#ifndef _OPENCOG_AGENTZERO_COMMUNICATION_HUMAN_INTERFACE_H
#define _OPENCOG_AGENTZERO_COMMUNICATION_HUMAN_INTERFACE_H

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/communication/DialogueManager.h"
#include "opencog/agentzero/communication/LanguageProcessor.h"

namespace opencog {
namespace agentzero {
namespace communication {

/** Per-user interactive session. */
struct HumanSession {
    std::string session_id;
    std::string user_id;
    std::string conversation_id;
    bool active = true;
    size_t turn_count = 0;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point last_activity;
};

/**
 * HumanInterface — high-level human↔agent dialogue facade.
 *
 * Owns a DialogueManager and LanguageProcessor, manages user sessions,
 * and formats responses for presentation.
 */
class HumanInterface
{
public:
    explicit HumanInterface(AtomSpacePtr atomspace,
                            const std::string& agent_name = "Agent-Zero");
    virtual ~HumanInterface();

    HumanInterface(const HumanInterface&) = delete;
    HumanInterface& operator=(const HumanInterface&) = delete;

    /** Create a new user session; returns session_id. */
    std::string startSession(const std::string& user_id = "user");

    /** End a session and its underlying conversation. */
    bool endSession(const std::string& session_id);

    /** Process free-form user input within a session. */
    std::string processInput(const std::string& session_id,
                             const std::string& input);

    /**
     * Convenience one-shot: uses (or creates) the default session.
     * Suitable for simple REPL-style interaction.
     */
    std::string processHumanInput(const std::string& input);

    /** Generate a proactive / context-only prompt without recording a user turn. */
    std::string generateResponse(const std::string& context);

    /** Reset the default session (end + recreate). */
    void resetSession();

    bool isSessionActive(const std::string& session_id) const;
    size_t getActiveSessionCount() const;
    std::vector<std::string> listSessions() const;

    const std::string& getAgentName() const { return _agent_name; }
    DialogueManager& getDialogueManager() { return *_dialogue; }
    LanguageProcessor& getLanguageProcessor() { return *_language; }
    AtomSpacePtr getAtomSpace() const { return _atomspace; }

private:
    AtomSpacePtr _atomspace;
    std::string _agent_name;
    std::unique_ptr<LanguageProcessor> _language;
    std::unique_ptr<DialogueManager> _dialogue;

    mutable std::mutex _mutex;
    std::map<std::string, HumanSession> _sessions;
    std::string _default_session_id;

    Handle _interface_root = Handle::UNDEFINED;

    void initializeAtoms();
    std::string makeSessionId(const std::string& user_id) const;
    std::string trim(const std::string& s) const;
    std::string formatResponse(const std::string& raw) const;
    HumanSession* requireSession(const std::string& session_id);
};

} // namespace communication
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_COMMUNICATION_HUMAN_INTERFACE_H
