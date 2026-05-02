/*
 * standalone/include/cog0/PerceptualProcessor.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * PerceptualProcessor — converts raw PerceptInput into AtomStore representations.
 * Part of Phase 2: Perception & Sensory Processing.
 */

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "AtomStore.h"
#include "CognitiveLoop.h"  // PerceptInput

namespace cog0 {

// -----------------------------------------------------------------------
// PerceptualProcessor

class PerceptualProcessor {
public:
    explicit PerceptualProcessor(std::shared_ptr<AtomStore> store);

    // Process a percept and add corresponding atoms to the store.
    // Returns the primary Handle created (a CONCEPT node for the percept content).
    Handle process(const PerceptInput& p);

    // Tokenize text and add one CONCEPT atom per token with salience as STI.
    // Returns all Handle objects created.
    std::vector<Handle> processText(const std::string& text, double salience = 0.5,
                                    const std::string& source = "");

    // Process a numeric value: adds a CONCEPT atom named "numeric:<value>".
    Handle processNumeric(double value, double salience = 0.5,
                          const std::string& source = "");

    // Process an event name: adds a CONCEPT atom named "event:<name>".
    Handle processEvent(const std::string& eventName, double salience = 0.5,
                        const std::string& source = "");

    // Return all percept atoms processed since the last clearHistory().
    std::vector<Handle> recentPercepts() const;

    // Clear processing history (does not remove atoms from the store).
    void clearHistory();

    size_t processedCount() const;

private:
    std::shared_ptr<AtomStore> _store;
    mutable std::mutex         _mutex;
    std::vector<Handle>        _history;

    // Split text into lower-case word tokens.
    static std::vector<std::string> tokenize(const std::string& text);

    // Normalize a single word token (lower-case, strip punctuation).
    static std::string normalize(const std::string& word);
};

} // namespace cog0
