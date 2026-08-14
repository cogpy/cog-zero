/*
 * opencog/agentzero/PerceptualProcessor.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Converts raw SensoryInput into AtomSpace representations.
 * Part of AGENT-ZERO-GENESIS Phase 2 (Perception & Sensory Processing).
 */
#ifndef _OPENCOG_AGENTZERO_PERCEPTUAL_PROCESSOR_H
#define _OPENCOG_AGENTZERO_PERCEPTUAL_PROCESSOR_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

#include "opencog/agentzero/MultiModalSensor.h"

namespace opencog {
namespace agentzero {

/**
 * PerceptualProcessor — encodes multi-modal sensory samples as ConceptNodes
 * and EvaluationLinks in AtomSpace, associated with the agent self atom.
 */
class PerceptualProcessor {
public:
    PerceptualProcessor(AtomSpacePtr atomspace, Handle agent_self);
    ~PerceptualProcessor();

    AtomSpacePtr getAtomSpace() const { return _atomspace; }
    Handle getAgentSelf() const { return _agent_self; }

    /**
     * Encode one SensoryInput. Returns primary CONCEPT handle, or
     * Handle::UNDEFINED on validation failure.
     */
    Handle processInput(const SensoryInput& input);

    /** Process a batch; one Handle per input (may be UNDEFINED on error). */
    std::vector<Handle> processBatch(const std::vector<SensoryInput>& inputs);

    /** Optional context atom linked to subsequent percepts. */
    void setPerceptionContext(const Handle& context);

    /** JSON-ish stats: processed_count, error_count, … */
    std::string getProcessingStats() const;

    /**
     * Healthy when total attempts are zero, or error rate is below 50%.
     */
    bool isHealthy() const;

    void resetStats();

private:
    bool validateInput(const SensoryInput& input) const;
    Handle encodeInput(const SensoryInput& input);

    AtomSpacePtr _atomspace;
    Handle _agent_self;
    Handle _context = Handle::UNDEFINED;
    Handle _percepts_root = Handle::UNDEFINED;

    mutable std::mutex _mu;
    size_t _processed_count = 0;
    size_t _error_count = 0;
    size_t _attempt_count = 0;
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_PERCEPTUAL_PROCESSOR_H
