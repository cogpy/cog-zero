/*
 * opencog/agentzero/ReasoningEngine.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * PLN/URE-style reasoning with uncertainty via TruthValues.
 * Part of AGENT-ZERO-GENESIS Phase 1.
 */
#ifndef _OPENCOG_AGENTZERO_REASONING_ENGINE_H
#define _OPENCOG_AGENTZERO_REASONING_ENGINE_H

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

namespace opencog {
namespace agentzero {

class AgentZeroCore;

class ReasoningEngine {
public:
    enum class ReasoningMode {
        FORWARD_CHAINING,
        BACKWARD_CHAINING,
        MIXED,
        ABDUCTIVE,
        ANALOGICAL,
        CAUSAL
    };

    struct ReasoningResult {
        Handle conclusion = Handle::UNDEFINED;
        double confidence = 0.0;
        double strength = 0.0;
        std::string explanation;
        std::vector<Handle> support;
    };

    struct ReasoningRule {
        std::string name;
        std::string rule_type = "custom";
        double weight = 1.0;
        std::function<bool(const std::vector<Handle>&)> applicability_check;
        std::function<ReasoningResult(AtomSpacePtr, const std::vector<Handle>&)> apply;
    };

    struct Hypothesis {
        Handle atom = Handle::UNDEFINED;
        std::string explanation;
        double plausibility = 0.0;
    };

    ReasoningEngine(AgentZeroCore* agent_core, AtomSpacePtr atomspace);
    ~ReasoningEngine();

    std::vector<ReasoningResult> reason(const std::vector<Handle>& premises,
                                        ReasoningMode mode = ReasoningMode::FORWARD_CHAINING);

    std::vector<Hypothesis> generateHypotheses(const std::vector<Handle>& observations);

    bool addReasoningRule(const ReasoningRule& rule);
    size_t ruleCount() const;

    void configurePLN(bool enable, double confidence_threshold = 0.6,
                      double truth_threshold = 0.5);
    void configureURE(bool enable, int max_iterations = 100,
                      double complexity_penalty = 0.1);

    bool processReasoningCycle();
    size_t getConclusionCount() const { return _conclusion_count; }
    Handle getLastConclusion() const { return _last_conclusion; }

    std::string getStatusInfo() const;

private:
    std::vector<ReasoningResult> forwardChain(const std::vector<Handle>& premises);
    std::vector<ReasoningResult> backwardChain(const std::vector<Handle>& goals);
    void installDefaultRules();
    Handle recordConclusion(const ReasoningResult& result);

    AgentZeroCore* _agent_core;
    AtomSpacePtr _atomspace;

    mutable std::mutex _mu;
    std::vector<ReasoningRule> _rules;

    bool _pln_enabled = true;
    bool _ure_enabled = true;
    double _confidence_threshold = 0.6;
    double _truth_threshold = 0.5;
    int _max_iterations = 100;
    double _complexity_penalty = 0.1;

    size_t _conclusion_count = 0;
    Handle _last_conclusion = Handle::UNDEFINED;
    Handle _reasoning_root = Handle::UNDEFINED;
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_REASONING_ENGINE_H
