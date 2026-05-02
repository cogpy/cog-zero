/*
 * opencog/agentzero/ReasoningEngine.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ReasoningEngine Implementation
 * PLN-based inference and reasoning for Agent-Zero
 * Part of the AGENT-ZERO-GENESIS project
 */

#include "opencog/agentzero/ReasoningEngine.h"
#include "opencog/agentzero/AgentZeroCore.h"

#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/random.h>

#include <sstream>
#include <algorithm>
#include <cmath>
#include <random>

// URE includes (only if URE is available)
#ifdef HAVE_URE
#include <opencog/ure/UREConfig.h>
#include <opencog/ure/Rule.h>
#include <opencog/ure/forwardchainer/ForwardChainer.h>
#include <opencog/ure/backwardchainer/BackwardChainer.h>
#endif

using namespace opencog;
using namespace opencog::agentzero;

ReasoningEngine::ReasoningEngine(AgentZeroCore* agent_core, AtomSpacePtr atomspace)
    : _agent_core(agent_core)
    , _atomspace(atomspace)
    , _enable_pln_reasoning(true)
    , _enable_ure_integration(true)
    , _enable_uncertainty_propagation(true)
    , _max_inference_steps(15)
    , _confidence_threshold(0.6)
    , _truth_value_threshold(0.5)
{
    logger().info() << "[ReasoningEngine] Initializing PLN-based reasoning engine";
    
    // Initialize core reasoning structures
    initializePLNIntegration();
    initializeUREIntegration();
    loadDefaultReasoningRules();
    
    logger().info() << "[ReasoningEngine] Reasoning engine initialized with " 
                   << _reasoning_rules.size() << " rules";
}

ReasoningEngine::~ReasoningEngine()
{
    logger().info() << "[ReasoningEngine] Shutting down reasoning engine";
    clearReasoningCache();
}

void ReasoningEngine::initializePLNIntegration()
{
    logger().debug() << "[ReasoningEngine] Initializing PLN integration";
    
    // Create PLN reasoning context
    _pln_context = _atomspace->add_node(CONCEPT_NODE, "PLN_ReasoningContext");
    _pln_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    
    // Initialize inference history tracking
    _inference_history = _atomspace->add_node(CONCEPT_NODE, "InferenceHistory");
    
    // Create reasoning cache
    _reasoning_cache = _atomspace->add_node(CONCEPT_NODE, "ReasoningCache");
    
    logger().debug() << "[ReasoningEngine] PLN integration initialized";
}

void ReasoningEngine::initializeUREIntegration()
{
    logger().debug() << "[ReasoningEngine] Initializing URE integration";
    
    // Create URE rule base in AtomSpace
    _rule_base = _atomspace->add_node(CONCEPT_NODE, "URE_RuleBase");
    _rule_base->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    
#ifdef HAVE_URE
    try {
        // Create URE-specific rulebase handle
        _ure_rulebase_handle = _atomspace->add_node(CONCEPT_NODE, "AgentZero-URE-RuleBase");
        
        // Initialize URE configuration with the rulebase
        _ure_config = std::make_unique<UREConfig>(*_atomspace, _ure_rulebase_handle);
        
        // Set default URE parameters
        _ure_parameters["max_iterations"] = 100.0;
        _ure_parameters["complexity_penalty"] = 0.1;
        _ure_parameters["jobs"] = 1.0;
        _ure_parameters["expansion_pool_size"] = 10.0;
        
        // Apply default configuration
        _ure_config->set_maximum_iterations(static_cast<int>(_ure_parameters["max_iterations"]));
        _ure_config->set_complexity_penalty(_ure_parameters["complexity_penalty"]);
        _ure_config->set_jobs(static_cast<int>(_ure_parameters["jobs"]));
        _ure_config->set_expansion_pool_size(static_cast<int>(_ure_parameters["expansion_pool_size"]));
        
        logger().debug() << "[ReasoningEngine] URE configuration initialized with " 
                        << "max_iterations=" << _ure_parameters["max_iterations"] 
                        << ", complexity_penalty=" << _ure_parameters["complexity_penalty"];
        
    } catch (const std::exception& e) {
        logger().error() << "[ReasoningEngine] Failed to initialize URE configuration: " << e.what();
        _enable_ure_integration = false;
    }
#endif
    
    logger().debug() << "[ReasoningEngine] URE integration initialized (URE available: " 
#ifdef HAVE_URE
                    << "yes"
#else
                    << "no"
#endif
                    << ")";
}

void ReasoningEngine::loadDefaultReasoningRules()
{
    logger().debug() << "[ReasoningEngine] Loading default reasoning rules";
    
    // Deduction rule: If A->B and B->C then A->C
    ReasoningRule deduction_rule;
    deduction_rule.name = "deduction";
    deduction_rule.weight = 0.9;
    deduction_rule.rule_type = "deduction";
    deduction_rule.preconditions = {"implication", "implication"};
    deduction_rule.applicability_check = [](const std::vector<Handle>& facts) -> bool {
        // Check if we have two implications that can chain
        int implication_count = 0;
        for (const auto& fact : facts) {
            if (fact->get_type() == IMPLICATION_LINK) {
                implication_count++;
            }
        }
        return implication_count >= 2;
    };
    addReasoningRule(deduction_rule);
    
    // Modus Ponens rule: If A->B and A then B
    ReasoningRule modus_ponens;
    modus_ponens.name = "modus_ponens";
    modus_ponens.weight = 0.95;
    modus_ponens.rule_type = "forward_chaining";
    modus_ponens.preconditions = {"implication", "antecedent"};
    modus_ponens.applicability_check = [](const std::vector<Handle>& facts) -> bool {
        bool has_implication = false;
        bool has_antecedent = false;
        for (const auto& fact : facts) {
            if (fact->get_type() == IMPLICATION_LINK) {
                has_implication = true;
            } else {
                has_antecedent = true;
            }
        }
        return has_implication && has_antecedent;
    };
    addReasoningRule(modus_ponens);
    
    // Abduction rule: If B and A->B then maybe A
    ReasoningRule abduction;
    abduction.name = "abduction";
    abduction.weight = 0.7;
    abduction.rule_type = "abductive";
    abduction.preconditions = {"consequent", "implication"};
    abduction.applicability_check = [](const std::vector<Handle>& facts) -> bool {
        return facts.size() >= 2; // Simple check for demonstration
    };
    addReasoningRule(abduction);
    
    // Inheritance rule: If A isa B and B has property P then A has property P
    ReasoningRule inheritance;
    inheritance.name = "inheritance";
    inheritance.weight = 0.8;
    inheritance.rule_type = "inheritance";
    inheritance.preconditions = {"inheritance", "property"};
    inheritance.applicability_check = [](const std::vector<Handle>& facts) -> bool {
        for (const auto& fact : facts) {
            if (fact->get_type() == INHERITANCE_LINK) {
                return true;
            }
        }
        return false;
    };
    addReasoningRule(inheritance);
    
    logger().info() << "[ReasoningEngine] Loaded " << _reasoning_rules.size() << " default rules";
}

std::vector<ReasoningEngine::ReasoningResult> 
ReasoningEngine::reason(const std::vector<Handle>& premises, ReasoningMode mode, int max_steps)
{
    logger().debug() << "[ReasoningEngine] Starting reasoning with " << premises.size() 
                    << " premises, mode=" << static_cast<int>(mode) << ", max_steps=" << max_steps;
    
    std::vector<ReasoningResult> results;
    
    try {
        // Check reasoning cache first
        auto cached_results = retrieveCachedResults(premises);
        if (!cached_results.empty()) {
            logger().debug() << "[ReasoningEngine] Found " << cached_results.size() << " cached results";
            return cached_results;
        }
        
        // Perform reasoning based on mode
        switch (mode) {
            case ReasoningMode::FORWARD_CHAINING:
                results = performForwardChaining(premises, max_steps);
                break;
            case ReasoningMode::BACKWARD_CHAINING:
                // For backward chaining without a specific goal, use premises as goals
                for (const auto& premise : premises) {
                    auto backward_results = performBackwardChaining(premise, max_steps);
                    results.insert(results.end(), backward_results.begin(), backward_results.end());
                }
                break;
            case ReasoningMode::MIXED_CHAINING:
                if (!premises.empty()) {
                    results = performMixedChaining(premises, premises[0], max_steps);
                }
                break;
            case ReasoningMode::ABDUCTIVE:
                // Abductive reasoning to find explanations
                results = generateHypotheses(premises);
                break;
            case ReasoningMode::ANALOGICAL:
                // For analogical reasoning, we need at least two sets to compare
                if (premises.size() >= 2) {
                    std::vector<Handle> source(premises.begin(), premises.begin() + premises.size()/2);
                    std::vector<Handle> target(premises.begin() + premises.size()/2, premises.end());
                    results = analogicalReasoning(source, target);
                }
                break;
            case ReasoningMode::CAUSAL:
                // Simple causal reasoning implementation
                results = performForwardChaining(premises, max_steps);
                // Filter for causal relationships
                results.erase(std::remove_if(results.begin(), results.end(),
                    [](const ReasoningResult& r) {
                        return r.reasoning_type.find("causal") == std::string::npos;
                    }), results.end());
                break;
        }
        
        // Cache successful results
        for (const auto& result : results) {
            cacheReasoningResult(result);
        }
        
        logger().debug() << "[ReasoningEngine] Reasoning completed with " << results.size() << " results";
        
    } catch (const std::exception& e) {
        logger().error() << "[ReasoningEngine] Error during reasoning: " << e.what();
    }
    
    return results;
}

std::vector<ReasoningEngine::ReasoningResult>
ReasoningEngine::performForwardChaining(const std::vector<Handle>& premises, int max_steps)
{
    logger().debug() << "[ReasoningEngine] Performing forward chaining with " << premises.size() << " premises";
    
    std::vector<ReasoningResult> results;
    
#ifdef HAVE_URE
    // Try URE-based forward chaining first if enabled and configured
    if (_enable_ure_integration && _ure_config && _forward_chainer && !_ure_rules.empty()) {
        logger().debug() << "[ReasoningEngine] Using URE forward chainer";
        
        try {
            // Execute URE chaining
            auto ure_results = executeUREChaining(_ure_rulebase_handle, premises, max_steps);
            
            // Convert URE results to ReasoningResult format
            for (size_t i = 0; i < ure_results.size(); ++i) {
                ReasoningResult result;
                result.conclusion = ure_results[i];
                result.premises = premises;
                result.confidence = 0.8; // Default confidence for URE results
                result.reasoning_type = "ure_forward_chaining";
                result.inference_steps = static_cast<int>(i + 1);
                result.explanation = "URE forward chaining result";
                
                // Set truth value if not already set
                if (!result.conclusion->getTruthValue()) {
                    result.conclusion->setTruthValue(SimpleTruthValue::createTV(0.8, 0.8));
                }
                
                results.push_back(result);
            }
            
            if (!results.empty()) {
                logger().info() << "[ReasoningEngine] URE forward chaining produced " 
                               << results.size() << " results";
                return results;
            }
        } catch (const std::exception& e) {
            logger().warn() << "[ReasoningEngine] URE forward chaining failed: " << e.what()
                           << ", falling back to traditional approach";
        }
    }
#endif
    
    // Traditional forward chaining implementation (fallback or when URE not available)
    logger().debug() << "[ReasoningEngine] Using traditional forward chaining";
    
    std::set<Handle> derived_facts(premises.begin(), premises.end());
    std::set<Handle> new_facts = derived_facts;
    
    for (int step = 0; step < max_steps && !new_facts.empty(); ++step) {
        std::set<Handle> step_new_facts;
        
        // Find applicable rules
        std::vector<Handle> current_facts(new_facts.begin(), new_facts.end());
        auto applicable_rules = findApplicableRules(current_facts);
        
        // Apply each applicable rule
        for (const auto& rule_atom : applicable_rules) {
            // Find the corresponding reasoning rule
            auto rule_it = std::find_if(_reasoning_rules.begin(), _reasoning_rules.end(),
                [rule_atom](const ReasoningRule& rule) {
                    return rule.rule_atom == rule_atom;
                });
            
            if (rule_it != _reasoning_rules.end()) {
                try {
                    auto result = applyRule(*rule_it, current_facts);
                    
                    if (result.conclusion != Handle::UNDEFINED && 
                        result.confidence >= _confidence_threshold) {
                        
                        // Add to results
                        result.reasoning_type = "traditional_forward_chaining";
                        result.inference_steps = step + 1;
                        results.push_back(result);
                        
                        // Add new fact for next iteration
                        if (derived_facts.find(result.conclusion) == derived_facts.end()) {
                            step_new_facts.insert(result.conclusion);
                            derived_facts.insert(result.conclusion);
                        }
                    }
                } catch (const std::exception& e) {
                    logger().warn() << "[ReasoningEngine] Error applying rule " << rule_it->name 
                                   << ": " << e.what();
                }
            }
        }
        
        new_facts = step_new_facts;
        
        if (step_new_facts.empty()) {
            logger().debug() << "[ReasoningEngine] No new facts derived at step " << step;
            break;
        }
    }
    
    logger().debug() << "[ReasoningEngine] Forward chaining completed with " 
                    << results.size() << " inferences";
    
    return results;
}

std::vector<ReasoningEngine::ReasoningResult>
ReasoningEngine::performBackwardChaining(const Handle& goal, int max_steps)
{
    logger().debug() << "[ReasoningEngine] Performing backward chaining for goal";
    
    std::vector<ReasoningResult> results;
    std::set<Handle> goals_to_prove = {goal};
    std::set<Handle> proven_goals;
    
    for (int step = 0; step < max_steps && !goals_to_prove.empty(); ++step) {
        std::set<Handle> new_goals;
        
        for (const auto& current_goal : goals_to_prove) {
            // Find rules that could derive this goal
            std::vector<Handle> facts_context = {current_goal};
            auto applicable_rules = findApplicableRules(facts_context);
            
            for (const auto& rule_atom : applicable_rules) {
                auto rule_it = std::find_if(_reasoning_rules.begin(), _reasoning_rules.end(),
                    [rule_atom](const ReasoningRule& rule) {
                        return rule.rule_atom == rule_atom;
                    });
                
                if (rule_it != _reasoning_rules.end()) {
                    try {
                        // For backward chaining, we need to find what premises would lead to the goal
                        ReasoningResult result;
                        result.conclusion = current_goal;
                        result.premises = {current_goal}; // Simplified for demonstration
                        result.confidence = 0.7; // Default backward chaining confidence
                        result.reasoning_type = "backward_chaining";
                        result.inference_steps = step + 1;
                        result.rule_chain = {rule_atom};
                        result.explanation = generateExplanation(result);
                        
                        if (result.confidence >= _confidence_threshold) {
                            results.push_back(result);
                            proven_goals.insert(current_goal);
                        }
                    } catch (const std::exception& e) {
                        logger().warn() << "[ReasoningEngine] Error in backward chaining: " << e.what();
                    }
                }
            }
        }
        
        // Update goals for next iteration
        goals_to_prove = new_goals;
    }
    
    return results;
}

std::vector<ReasoningEngine::ReasoningResult>
ReasoningEngine::performMixedChaining(const std::vector<Handle>& premises, const Handle& goal, int max_steps)
{
    logger().debug() << "[ReasoningEngine] Performing mixed chaining";
    
    std::vector<ReasoningResult> results;
    
    // Combine forward and backward chaining
    int forward_steps = max_steps / 2;
    int backward_steps = max_steps - forward_steps;
    
    // Forward chaining from premises
    auto forward_results = performForwardChaining(premises, forward_steps);
    results.insert(results.end(), forward_results.begin(), forward_results.end());
    
    // Backward chaining from goal
    auto backward_results = performBackwardChaining(goal, backward_steps);
    results.insert(results.end(), backward_results.begin(), backward_results.end());
    
    // Mark as mixed chaining
    for (auto& result : results) {
        result.reasoning_type = "mixed_chaining";
    }
    
    return results;
}

std::vector<Handle> ReasoningEngine::findApplicableRules(const std::vector<Handle>& facts)
{
    std::vector<Handle> applicable_rules;
    
    for (const auto& rule : _reasoning_rules) {
        try {
            if (rule.applicability_check(facts)) {
                if (rule.rule_atom != Handle::UNDEFINED) {
                    applicable_rules.push_back(rule.rule_atom);
                }
            }
        } catch (const std::exception& e) {
            logger().warn() << "[ReasoningEngine] Error checking rule applicability: " << e.what();
        }
    }
    
    return applicable_rules;
}

ReasoningEngine::ReasoningResult 
ReasoningEngine::applyRule(const ReasoningRule& rule, const std::vector<Handle>& facts)
{
    ReasoningResult result;
    result.premises = facts;
    result.reasoning_type = rule.rule_type;
    result.rule_chain = {rule.rule_atom};
    
    try {
        // Simple rule application - create a conclusion based on rule type
        if (rule.name == "modus_ponens" && facts.size() >= 2) {
            // Find implication and antecedent
            Handle implication, antecedent;
            for (const auto& fact : facts) {
                if (fact->get_type() == IMPLICATION_LINK) {
                    implication = fact;
                } else {
                    antecedent = fact;
                }
            }
            
            if (implication != Handle::UNDEFINED && antecedent != Handle::UNDEFINED) {
                // Create conclusion - this is simplified for demonstration
                result.conclusion = _atomspace->add_node(CONCEPT_NODE, "modus_ponens_conclusion");
                result.confidence = 0.85;
            }
        } else if (rule.name == "deduction" && facts.size() >= 2) {
            // Chain implications
            result.conclusion = _atomspace->add_node(CONCEPT_NODE, "deduction_conclusion");
            result.confidence = 0.8;
        } else if (rule.name == "abduction") {
            // Generate hypothesis
            result.conclusion = _atomspace->add_node(CONCEPT_NODE, "abductive_hypothesis");
            result.confidence = 0.6;
        } else {
            // Default rule application
            result.conclusion = _atomspace->add_node(CONCEPT_NODE, "rule_conclusion");
            result.confidence = rule.weight * 0.8;
        }
        
        // Set truth value for the conclusion
        if (result.conclusion != Handle::UNDEFINED) {
            auto tv = computeResultingTruthValue(facts, rule);
            result.conclusion->setTruthValue(tv);
        }
        
        result.explanation = generateExplanation(result);
        
    } catch (const std::exception& e) {
        logger().error() << "[ReasoningEngine] Error applying rule " << rule.name << ": " << e.what();
        result.conclusion = Handle::UNDEFINED;
        result.confidence = 0.0;
    }
    
    return result;
}

TruthValuePtr ReasoningEngine::computeResultingTruthValue(const std::vector<Handle>& premises,
                                                        const ReasoningRule& rule)
{
    // Compute truth value based on premises and rule weight
    double mean_strength = 0.0;
    double mean_confidence = 0.0;
    int valid_premises = 0;
    
    for (const auto& premise : premises) {
        auto tv = premise->getTruthValue();
        if (tv) {
            mean_strength += tv->get_mean();
            mean_confidence += tv->get_confidence();
            valid_premises++;
        }
    }
    
    if (valid_premises > 0) {
        mean_strength /= valid_premises;
        mean_confidence /= valid_premises;
    } else {
        mean_strength = 0.5;
        mean_confidence = 0.5;
    }
    
    // Apply rule weight
    double result_strength = mean_strength * rule.weight;
    double result_confidence = mean_confidence * rule.weight;
    
    // Ensure values are within valid bounds
    result_strength = std::max(0.0, std::min(1.0, result_strength));
    result_confidence = std::max(0.0, std::min(1.0, result_confidence));
    
    return SimpleTruthValue::createTV(result_strength, result_confidence);
}

std::vector<ReasoningEngine::ReasoningResult> 
ReasoningEngine::generateHypotheses(const std::vector<Handle>& observations,
                                   const std::vector<Handle>& hypothesis_templates)
{
    logger().debug() << "[ReasoningEngine] Generating hypotheses from " 
                    << observations.size() << " observations";
    
    std::vector<ReasoningResult> hypotheses;
    
    // Use abductive reasoning to generate explanations
    for (const auto& observation : observations) {
        ReasoningResult hypothesis;
        hypothesis.conclusion = _atomspace->add_node(CONCEPT_NODE, 
                                                   "hypothesis_for_" + observation->get_name());
        hypothesis.premises = {observation};
        hypothesis.confidence = 0.6; // Hypotheses have moderate confidence
        hypothesis.reasoning_type = "abductive";
        hypothesis.inference_steps = 1;
        hypothesis.explanation = "Hypothesis generated to explain observation: " + observation->get_name();
        
        // Set truth value
        hypothesis.conclusion->setTruthValue(SimpleTruthValue::createTV(0.6, 0.8));
        
        hypotheses.push_back(hypothesis);
    }
    
    return hypotheses;
}

std::vector<ReasoningEngine::ReasoningResult>
ReasoningEngine::analogicalReasoning(const std::vector<Handle>& source_case,
                                    const std::vector<Handle>& target_case,
                                    const std::vector<Handle>& mapping_rules)
{
    logger().debug() << "[ReasoningEngine] Performing analogical reasoning";
    
    std::vector<ReasoningResult> results;
    
    // Simple analogical reasoning: if source and target share structure, 
    // transfer properties
    if (source_case.size() == target_case.size()) {
        ReasoningResult analogy;
        analogy.conclusion = _atomspace->add_node(CONCEPT_NODE, "analogical_conclusion");
        analogy.premises = source_case;
        analogy.premises.insert(analogy.premises.end(), target_case.begin(), target_case.end());
        analogy.confidence = 0.7;
        analogy.reasoning_type = "analogical";
        analogy.inference_steps = 1;
        analogy.explanation = "Analogical transfer from source case to target case";
        
        results.push_back(analogy);
    }
    
    return results;
}

bool ReasoningEngine::addReasoningRule(const ReasoningRule& rule)
{
    logger().debug() << "[ReasoningEngine] Adding reasoning rule: " << rule.name;
    
    try {
        // Create AtomSpace representation of the rule if not provided
        ReasoningRule new_rule = rule;
        if (new_rule.rule_atom == Handle::UNDEFINED) {
            new_rule.rule_atom = _atomspace->add_node(CONCEPT_NODE, "rule_" + rule.name);
        }
        
        _reasoning_rules.push_back(new_rule);
        _rules_by_type[rule.rule_type].push_back(new_rule);
        
#ifdef HAVE_URE
        // If URE is enabled, also create URE rule representation
        if (_enable_ure_integration && _ure_config) {
            try {
                // Create URE Rule from the ReasoningRule
                if (new_rule.rule_atom != Handle::UNDEFINED) {
                    RulePtr ure_rule = std::make_shared<Rule>(new_rule.rule_atom);
                    _ure_rules.push_back(ure_rule);
                    
                    logger().debug() << "[ReasoningEngine] Created URE rule for: " << rule.name;
                }
            } catch (const std::exception& e) {
                logger().warn() << "[ReasoningEngine] Failed to create URE rule for " 
                               << rule.name << ": " << e.what();
            }
        }
#endif
        
        return true;
    } catch (const std::exception& e) {
        logger().error() << "[ReasoningEngine] Error adding rule " << rule.name << ": " << e.what();
        return false;
    }
}

std::string ReasoningEngine::generateExplanation(const ReasoningResult& result)
{
    std::ostringstream explanation;
    
    explanation << "Reasoning result: ";
    if (result.conclusion != Handle::UNDEFINED) {
        explanation << result.conclusion->get_name();
    } else {
        explanation << "undefined";
    }
    
    explanation << " (confidence: " << result.confidence << ")";
    explanation << " using " << result.reasoning_type << " reasoning";
    explanation << " in " << result.inference_steps << " steps";
    
    if (!result.premises.empty()) {
        explanation << " from premises: ";
        for (size_t i = 0; i < result.premises.size(); ++i) {
            if (i > 0) explanation << ", ";
            explanation << result.premises[i]->get_name();
        }
    }
    
    return explanation.str();
}

void ReasoningEngine::cacheReasoningResult(const ReasoningResult& result)
{
    // Simple caching - in a full implementation, this would use the AtomSpace
    logger().debug() << "[ReasoningEngine] Caching reasoning result";
}

std::vector<ReasoningEngine::ReasoningResult>
ReasoningEngine::retrieveCachedResults(const std::vector<Handle>& query)
{
    // Simple implementation - return empty for now
    return std::vector<ReasoningResult>();
}

std::map<std::string, int> ReasoningEngine::getReasoningStatistics()
{
    std::map<std::string, int> stats;
    
    stats["total_rules"] = static_cast<int>(_reasoning_rules.size());
    stats["pln_enabled"] = _enable_pln_reasoning ? 1 : 0;
    stats["ure_enabled"] = _enable_ure_integration ? 1 : 0;
    stats["max_inference_steps"] = _max_inference_steps;
    stats["cached_results"] = 0; // Would implement proper caching
    
    // Count rules by type
    for (const auto& type_rules : _rules_by_type) {
        stats["rules_" + type_rules.first] = static_cast<int>(type_rules.second.size());
    }
    
    return stats;
}

void ReasoningEngine::configurePLN(bool enable_pln, double confidence_threshold, double truth_threshold)
{
    logger().info() << "[ReasoningEngine] Configuring PLN: enabled=" << enable_pln
                   << ", confidence_threshold=" << confidence_threshold
                   << ", truth_threshold=" << truth_threshold;
    
    _enable_pln_reasoning = enable_pln;
    _confidence_threshold = confidence_threshold;
    _truth_value_threshold = truth_threshold;
}

void ReasoningEngine::configureURE(bool enable_ure, int max_iterations, double complexity_penalty)
{
    logger().info() << "[ReasoningEngine] Configuring URE: enabled=" << enable_ure
                   << ", max_iterations=" << max_iterations
                   << ", complexity_penalty=" << complexity_penalty;
    
    _enable_ure_integration = enable_ure;
    
#ifdef HAVE_URE
    if (enable_ure && _ure_config) {
        try {
            // Update URE parameters
            _ure_parameters["max_iterations"] = static_cast<double>(max_iterations);
            _ure_parameters["complexity_penalty"] = complexity_penalty;
            
            // Apply configuration to URE
            _ure_config->set_maximum_iterations(max_iterations);
            _ure_config->set_complexity_penalty(complexity_penalty);
            
            // Recreate chainers with new configuration
            if (max_iterations > 0) {
                // Create a dummy source for the chainers - they need a source/target atom
                Handle dummy_source = _atomspace->add_node(CONCEPT_NODE, "URE-ChainerSource");
                Handle dummy_target = _atomspace->add_node(CONCEPT_NODE, "URE-ChainerTarget");
                
                _forward_chainer = std::make_unique<ForwardChainer>(*_atomspace, _ure_rulebase_handle, dummy_source);
                _backward_chainer = std::make_unique<BackwardChainer>(*_atomspace, _ure_rulebase_handle, dummy_target);
                
                logger().debug() << "[ReasoningEngine] URE chainers initialized with new configuration";
            }
            
            logger().info() << "[ReasoningEngine] URE configuration updated successfully";
            
        } catch (const std::exception& e) {
            logger().error() << "[ReasoningEngine] Failed to configure URE: " << e.what();
            _enable_ure_integration = false;
        }
    } else if (enable_ure && !_ure_config) {
        logger().warn() << "[ReasoningEngine] URE configuration requested but URE not properly initialized";
    }
#else
    if (enable_ure) {
        logger().warn() << "[ReasoningEngine] URE integration requested but not compiled with URE support";
    }
#endif
}

std::string ReasoningEngine::getStatusInfo() const
{
    std::ostringstream status;
    
    status << "{";
    status << "\"reasoning_engine_status\": \"active\",";
    status << "\"pln_enabled\": " << (_enable_pln_reasoning ? "true" : "false") << ",";
    status << "\"ure_enabled\": " << (_enable_ure_integration ? "true" : "false") << ",";
    status << "\"total_rules\": " << _reasoning_rules.size() << ",";
    status << "\"max_inference_steps\": " << _max_inference_steps << ",";
    status << "\"confidence_threshold\": " << _confidence_threshold;
    status << "}";
    
    return status.str();
}

bool ReasoningEngine::processReasoningCycle()
{
    logger().debug() << "[ReasoningEngine] Processing reasoning cycle";
    
    try {
        // In a full implementation, this would perform ongoing reasoning tasks
        // such as maintaining consistency, updating truth values, etc.
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[ReasoningEngine] Error in reasoning cycle: " << e.what();
        return false;
    }
}

void ReasoningEngine::clearReasoningCache()
{
    logger().debug() << "[ReasoningEngine] Clearing reasoning cache";
    _applied_rules_cache.clear();
    _active_reasoning_tasks.clear();
}

// URE-specific method implementations
#ifdef HAVE_URE

Handle ReasoningEngine::createURERuleBase(const std::vector<ReasoningRule>& rules)
{
    logger().debug() << "[ReasoningEngine] Creating URE rule base with " << rules.size() << " rules";
    
    if (!_ure_config) {
        logger().error() << "[ReasoningEngine] URE configuration not initialized";
        return Handle::UNDEFINED;
    }
    
    try {
        // Create a new rule base in the AtomSpace
        std::string rulebase_name = "URE-RuleBase-" + std::to_string(_active_reasoning_tasks.size());
        Handle rulebase_handle = _atomspace->add_node(CONCEPT_NODE, rulebase_name);
        
        // Convert AgentZero ReasoningRule format to URE Rule format
        std::vector<RulePtr> ure_rules;
        for (const auto& reasoning_rule : rules) {
            try {
                // Create URE Rule from ReasoningRule
                // For now, we create a simple rule based on the ReasoningRule data
                if (reasoning_rule.rule_atom != Handle::UNDEFINED) {
                    RulePtr ure_rule = std::make_shared<Rule>(reasoning_rule.rule_atom);
                    ure_rules.push_back(ure_rule);
                    
                    logger().debug() << "[ReasoningEngine] Converted rule: " << reasoning_rule.name;
                }
            } catch (const std::exception& e) {
                logger().warn() << "[ReasoningEngine] Failed to convert rule " 
                               << reasoning_rule.name << ": " << e.what();
            }
        }
        
        // Store the converted rules
        _ure_rules = ure_rules;
        
        logger().info() << "[ReasoningEngine] URE rule base created with " 
                       << ure_rules.size() << " converted rules";
        
        return rulebase_handle;
        
    } catch (const std::exception& e) {
        logger().error() << "[ReasoningEngine] Failed to create URE rule base: " << e.what();
        return Handle::UNDEFINED;
    }
}

std::vector<Handle> ReasoningEngine::executeUREChaining(const Handle& rule_base,
                                                       const std::vector<Handle>& premises,
                                                       int max_steps)
{
    logger().debug() << "[ReasoningEngine] Executing URE chaining with " 
                    << premises.size() << " premises, max_steps=" << max_steps;
    
    std::vector<Handle> results;
    
    if (!_ure_config || rule_base == Handle::UNDEFINED) {
        logger().error() << "[ReasoningEngine] URE not properly configured for chaining";
        return results;
    }
    
    try {
        // Configure URE parameters for this chaining session
        auto saved_max_iter = _ure_config->get_maximum_iterations();
        _ure_config->set_maximum_iterations(max_steps);
        
        // For each premise, create a ForwardChainer and run it
        for (const Handle& premise : premises) {
            try {
                // Create ForwardChainer for this specific premise
                ForwardChainer fc(*_atomspace, rule_base, premise);
                
                // Execute forward chaining
                fc.do_chain();
                
                // Get results and add them to our result set
                Handle premise_results = fc.get_results();
                if (premise_results != Handle::UNDEFINED) {
                    results.push_back(premise_results);
                }
                
                // Also get the set of results for individual conclusions
                HandleSet premise_results_set = fc.get_results_set();
                for (const Handle& result : premise_results_set) {
                    if (result != premise) { // Don't include the original premise
                        results.push_back(result);
                    }
                }
                
            } catch (const std::exception& e) {
                logger().warn() << "[ReasoningEngine] URE chaining failed for premise: " << e.what();
            }
        }
        
        // Restore original max iterations
        _ure_config->set_maximum_iterations(saved_max_iter);
        
        // Remove duplicates
        std::sort(results.begin(), results.end());
        results.erase(std::unique(results.begin(), results.end()), results.end());
        
        logger().info() << "[ReasoningEngine] URE forward chaining produced " 
                       << results.size() << " unique conclusions";
        
    } catch (const std::exception& e) {
        logger().error() << "[ReasoningEngine] URE chaining failed: " << e.what();
    }
    
    return results;
}

void ReasoningEngine::updateUREConfiguration(const std::map<std::string, double>& parameters)
{
    logger().debug() << "[ReasoningEngine] Updating URE configuration with " 
                    << parameters.size() << " parameters";
    
    if (!_ure_config) {
        logger().error() << "[ReasoningEngine] URE configuration not initialized";
        return;
    }
    
    try {
        // Update local parameter storage
        for (const auto& param : parameters) {
            _ure_parameters[param.first] = param.second;
            logger().debug() << "[ReasoningEngine] Updated parameter: " 
                           << param.first << " = " << param.second;
        }
        
        // Apply standard URE parameters
        if (parameters.find("max_iterations") != parameters.end()) {
            _ure_config->set_maximum_iterations(static_cast<int>(parameters.at("max_iterations")));
        }
        
        if (parameters.find("complexity_penalty") != parameters.end()) {
            _ure_config->set_complexity_penalty(parameters.at("complexity_penalty"));
        }
        
        if (parameters.find("jobs") != parameters.end()) {
            _ure_config->set_jobs(static_cast<int>(parameters.at("jobs")));
        }
        
        if (parameters.find("expansion_pool_size") != parameters.end()) {
            _ure_config->set_expansion_pool_size(static_cast<int>(parameters.at("expansion_pool_size")));
        }
        
        // Forward chainer specific parameters
        if (parameters.find("retry_exhausted_sources") != parameters.end()) {
            _ure_config->set_retry_exhausted_sources(parameters.at("retry_exhausted_sources") > 0.5);
        }
        
        if (parameters.find("full_rule_application") != parameters.end()) {
            _ure_config->set_full_rule_application(parameters.at("full_rule_application") > 0.5);
        }
        
        logger().info() << "[ReasoningEngine] URE configuration updated successfully";
        
    } catch (const std::exception& e) {
        logger().error() << "[ReasoningEngine] Failed to update URE configuration: " << e.what();
    }
}

#endif // HAVE_URE