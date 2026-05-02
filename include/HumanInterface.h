/*
 * HumanInterface.h
 *
 * Copyright (C) 2024 Agent-Zero-Genesis Project
 * 
 * Human-agent interaction layer for Agent-Zero
 */

#ifndef _AGENTZERO_HUMAN_INTERFACE_H
#define _AGENTZERO_HUMAN_INTERFACE_H

#include <memory>
#include <string>

#include <opencog/atomspace/AtomSpace.h>

namespace agentzero {
namespace communication {

class DialogueManager;

/**
 * HumanInterface — user-facing dialogue layer.
 *
 * Provides a high-level entry point for human-agent interaction:
 * - Accepts free-form text input from a user.
 * - Delegates conversation management to a DialogueManager.
 * - Formats and returns agent responses.
 * - Supports session reset for new conversations.
 */
class HumanInterface
{
public:
    /**
     * @param atomspace  Shared AtomSpace for knowledge storage.
     * @param agent_name Display name presented to the user.
     */
    explicit HumanInterface(opencog::AtomSpacePtr atomspace,
                            const std::string& agent_name = "Agent-Zero");
    ~HumanInterface();

    // Non-copyable
    HumanInterface(const HumanInterface&) = delete;
    HumanInterface& operator=(const HumanInterface&) = delete;

    /**
     * Process a free-form text utterance from the user and return
     * the agent's reply.
     */
    std::string process_human_input(const std::string& input);

    /**
     * Generate a context-driven response without recording a new turn.
     * Useful for proactive prompts or status messages.
     */
    std::string generate_response(const std::string& context);

    /**
     * Reset the current session (clears conversation history).
     */
    void reset_session();

    /**
     * Return the agent display name.
     */
    const std::string& get_agent_name() const { return _agent_name; }

private:
    opencog::AtomSpacePtr _atomspace;
    std::string _agent_name;
    std::unique_ptr<DialogueManager> _dialogue;

    std::string trim_string(const std::string& s) const;
    std::string format_response(const std::string& raw_response) const;
};

} // namespace communication
} // namespace agentzero


#endif // _AGENTZERO_HUMAN_INTERFACE_H
