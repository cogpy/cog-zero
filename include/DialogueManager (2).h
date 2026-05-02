/*
 * DialogueManager.h
 *
 * Copyright (C) 2024 Agent-Zero-Genesis Project
 * 
 * Conversational interaction management for Agent-Zero
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 */

#ifndef _AGENTZERO_DIALOGUE_MANAGER_H
#define _AGENTZERO_DIALOGUE_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <utility>

#include <opencog/atomspace/AtomSpace.h>

namespace agentzero {
namespace communication { 

/**
 * DialogueManager manages multi-turn conversational interactions.
 *
 * Maintains per-session conversation history and context, enabling
 * coherent multi-turn dialogue. Stores interaction records in the
 * AtomSpace for knowledge integration.
 */
class DialogueManager
{
public:
    /// A single dialogue exchange: (user_input, agent_response)
    using Turn = std::pair<std::string, std::string>;

    explicit DialogueManager(opencog::AtomSpacePtr atomspace);
    ~DialogueManager() = default;

    // Non-copyable
    DialogueManager(const DialogueManager&) = delete;
    DialogueManager& operator=(const DialogueManager&) = delete;

    /**
     * Process a user utterance and return the agent's response.
     * Updates conversation history and context.
     */
    std::string process_dialogue(const std::string& input);

    /**
     * Clear conversation history and all stored context.
     */
    void reset_context();

    /**
     * Get the number of completed dialogue turns.
     */
    size_t get_turn_count() const;

    /**
     * Access the full conversation history (read-only).
     */
    const std::vector<Turn>& get_history() const;

    /**
     * Store an arbitrary key/value pair in the dialogue context.
     */
    void set_context(const std::string& key, const std::string& value);

    /**
     * Retrieve a value from the dialogue context.
     * Returns an empty string if the key is not present.
     */
    std::string get_context(const std::string& key) const;

private:
    opencog::AtomSpacePtr _atomspace;
    std::vector<Turn> _history;
    std::map<std::string, std::string> _context;
    size_t _turn_count;

    std::string generate_response(const std::string& input);
    void store_turn_in_atomspace(const std::string& input, const std::string& response);
};

} // namespace communication
} // namespace agentzero

#endif // _AGENTZERO_DIALOGUE_MANAGER_H