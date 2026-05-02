/*
 * opencog/agentzero/ReasoningEngine.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ReasoningEngine Implementation
 * PLN-based inference and reasoning for Agent-Zero
 * Part of the AGENT-ZERO-GENESIS project
 */

#ifndef _OPENCOG_AGENTZERO_REASONING_ENGINE_H
#define _OPENCOG_AGENTZERO_REASONING_ENGINE_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

// URE Integration includes
#ifdef HAVE_URE
#include <opencog/ure/UREConfig.h>
#include <opencog/ure/Rule.h>
#include <opencog/ure/forwardchainer/ForwardChainer.h>
#include <opencog/ure/backwardchainer/BackwardChainer.h>
#include <opencog/ure/Utils.h>
#endif

namespace opencog {
namespace agentzero {

class AgentZeroCore;

/**
 * ReasoningEngine - PLN-based inference and reasoning
 *
 * This class implements advanced reasoning capabilities for Agent-Zero
 * using OpenCog's PLN (Probabilistic Logic Networks) and URE (Unified
 * Rule Engine). It provides flexible reasoning patterns for knowledge
 * inference, hypothesis generation, and pattern-based reasoning.
 *
 * Key features:
 * - PLN-based probabilistic reasoning
 * - URE rule application and management
 * - Forward and backward chaining inference
 * - Uncertainty handling with truth values
 * - Context-aware reasoning
 * - Multi-step inference chains
 */
class ReasoningEngine
{
public:
    // Reasoning modes for different inference strategies
    enum class ReasoningMode {
        FORWARD_CHAINING,    // Forward inference from premises to conclusions
        BACKWARD_CHAINING,   // Backward inference from goals to premises
        MIXED_CHAINING,      // Combined forward and backward chaining
        ABDUCTIVE,          // Abductive reasoning for hypothesis generation
        ANALOGICAL,         // Analogical reasoning based on similarities
        CAUSAL              // Causal reasoning for cause-effect relationships
    };
    
    // Confidence levels for reasoning results
    enum class ConfidenceLevel {
        VERY_LOW = 0,
        LOW = 25,
        MEDIUM = 50,
        HIGH = 75,
        VERY_HIGH = 100
    };
    
    // Reasoning result structure
    struct ReasoningResult {
        Handle conclusion;          // The inferred conclusion atom
        std::vector<Handle> premises; // The premises used in inference
        double confidence;          // Confidence in the result (0.0-1.0)
        std::string reasoning_type; // Type of reasoning applied
        std::vector<Handle> rule_chain; // Rules used in the inference
        int inference_steps;       // Number of reasoning steps
        std::string explanation;   // Human-readable explanation
    };
    
    // Rule structure for URE integration
    struct ReasoningRule {
        std::string name;           // Rule identifier
        Handle rule_atom;          // AtomSpace representation of the rule
        double weight;             // Rule application weight
        std::string rule_type;     // Type of rule (deduction, induction, etc.)
        std::vector<std::string> preconditions; // Required preconditions
        std::function<bool(const std::vector<Handle>&)> applicability_check;
    };

private:
    // Core references
    AgentZeroCore* _agent_core;
    AtomSpacePtr _atomspace;
    
    // Reasoning configuration
    bool _enable_pln_reasoning;
    bool _enable_ure_integration;
    bool _enable_uncertainty_propagation;
    int _max_inference_steps;
    double _confidence_threshold;
    double _truth_value_threshold;
    
    // PLN and URE integration handles
    Handle _pln_context;
    Handle _rule_base;
    Handle _inference_history;
    Handle _reasoning_cache;
    std::map<std::string, Handle> _active_reasoning_tasks;
    
    // Rule management
    std::vector<ReasoningRule> _reasoning_rules;
    std::map<std::string, std::vector<ReasoningRule>> _rules_by_type;
    std::set<Handle> _applied_rules_cache;
    
    // URE-specific member variables
#ifdef HAVE_URE
    std::unique_ptr<UREConfig> _ure_config;
    std::unique_ptr<ForwardChainer> _forward_chainer;
    std::unique_ptr<BackwardChainer> _backward_chainer;
    std::vector<RulePtr> _ure_rules;
    Handle _ure_rulebase_handle;
    std::map<std::string, double> _ure_parameters;
#endif
    
    // Internal reasoning methods
    void initializePLNIntegration();
    void initializeUREIntegration();
    void loadDefaultReasoningRules();
    
    // Core inference methods
    std::vector<ReasoningResult> performForwardChaining(const std::vector<Handle>& premises,
                                                       int max_steps);
    std::vector<ReasoningResult> performBackwardChaining(const Handle& goal,
                                                        int max_steps);
    std::vector<ReasoningResult> performMixedChaining(const std::vector<Handle>& premises,
                                                     const Handle& goal,
                                                     int max_steps);
    
    // Rule application methods
    std::vector<Handle> findApplicableRules(const std::vector<Handle>& facts);
    ReasoningResult applyRule(const ReasoningRule& rule,
                            const std::vector<Handle>& facts);
    std::vector<Handle> selectBestRules(const std::vector<Handle>& candidate_rules,
                                       const std::vector<Handle>& context);
    
    // Truth value and uncertainty handling
    TruthValuePtr computeResultingTruthValue(const std::vector<Handle>& premises,
                                           const ReasoningRule& rule);
    double propagateUncertainty(const std::vector<double>& input_confidences,
                               const std::string& operation_type);
    bool isAboveThreshold(const TruthValuePtr& tv);
    
    // Reasoning optimization
    Handle createInferenceContext(const std::string& reasoning_task);
    void cacheReasoningResult(const ReasoningResult& result);
    std::vector<ReasoningResult> retrieveCachedResults(const std::vector<Handle>& query);
    void pruneInferenceSpace(int max_size);
    
    // Explanation generation
    std::string generateExplanation(const ReasoningResult& result);
    std::string formatRuleChain(const std::vector<Handle>& rules);
    
    // PLN-specific methods
    std::vector<Handle> performPLNInference(const std::vector<Handle>& premises,
                                          const std::string& target_pattern);
    Handle createPLNQuery(const std::string& query_pattern);
    std::vector<Handle> evaluatePLNRules(const std::vector<Handle>& facts);
    
    // URE-specific methods
    Handle createURERuleBase(const std::vector<ReasoningRule>& rules);
    std::vector<Handle> executeUREChaining(const Handle& rule_base,
                                         const std::vector<Handle>& premises,
                                         int max_steps);
    void updateUREConfiguration(const std::map<std::string, double>& parameters);

public:
    /**
     * Constructor
     * @param agent_core Pointer to the parent AgentZeroCore instance
     * @param atomspace Shared pointer to the AtomSpace
     */
    ReasoningEngine(AgentZeroCore* agent_core, AtomSpacePtr atomspace);
    
    /**
     * Destructor - cleans up reasoning resources
     */
    ~ReasoningEngine();
    
    // Main reasoning interface
    /**
     * Perform reasoning based on given premises
     * @param premises Vector of premise atoms
     * @param mode Reasoning mode to use
     * @param max_steps Maximum number of inference steps
     * @return Vector of reasoning results
     */
    std::vector<ReasoningResult> reason(const std::vector<Handle>& premises,
                                       ReasoningMode mode = ReasoningMode::MIXED_CHAINING,
                                       int max_steps = 10);
    
    /**
     * Reason towards a specific goal
     * @param goal The goal atom to reason towards
     * @param known_facts Vector of known fact atoms
     * @param mode Reasoning mode to use
     * @return Vector of reasoning results leading to the goal
     */
    std::vector<ReasoningResult> reasonToGoal(const Handle& goal,
                                             const std::vector<Handle>& known_facts,
                                             ReasoningMode mode = ReasoningMode::BACKWARD_CHAINING);
    
    /**
     * Generate hypotheses from observations
     * @param observations Vector of observation atoms
     * @param hypothesis_templates Optional hypothesis patterns
     * @return Vector of hypothesis reasoning results
     */
    std::vector<ReasoningResult> generateHypotheses(const std::vector<Handle>& observations,
                                                   const std::vector<Handle>& hypothesis_templates = {});
    
    /**
     * Perform abductive reasoning to explain observations
     * @param observations Vector of atoms to explain
     * @param background_knowledge Vector of background knowledge atoms
     * @return Vector of possible explanations
     */
    std::vector<ReasoningResult> explainObservations(const std::vector<Handle>& observations,
                                                    const std::vector<Handle>& background_knowledge);
    
    /**
     * Perform analogical reasoning based on similarities
     * @param source_case Vector of atoms representing the source case
     * @param target_case Vector of atoms representing the target case
     * @param mapping_rules Optional rules for case mapping
     * @return Vector of analogical inferences
     */
    std::vector<ReasoningResult> analogicalReasoning(const std::vector<Handle>& source_case,
                                                    const std::vector<Handle>& target_case,
                                                    const std::vector<Handle>& mapping_rules = {});
    
    // Rule management interface
    /**
     * Add a reasoning rule to the engine
     * @param rule The reasoning rule to add
     * @return true if rule was added successfully
     */
    bool addReasoningRule(const ReasoningRule& rule);
    
    /**
     * Remove a reasoning rule
     * @param rule_name Name of the rule to remove
     * @return true if rule was removed successfully
     */
    bool removeReasoningRule(const std::string& rule_name);
    
    /**
     * Get all available reasoning rules
     * @param rule_type Optional filter by rule type
     * @return Vector of matching reasoning rules
     */
    std::vector<ReasoningRule> getReasoningRules(const std::string& rule_type = "");
    
    /**
     * Load reasoning rules from AtomSpace patterns
     * @param rule_atoms Vector of rule atoms to load
     * @return Number of rules successfully loaded
     */
    int loadRulesFromAtoms(const std::vector<Handle>& rule_atoms);
    
    // Query and validation interface
    /**
     * Validate the consistency of a set of beliefs
     * @param beliefs Vector of belief atoms to validate
     * @return Vector of inconsistencies found
     */
    std::vector<ReasoningResult> validateConsistency(const std::vector<Handle>& beliefs);
    
    /**
     * Find supporting evidence for a claim
     * @param claim The claim atom to support
     * @param evidence_base Vector of potential evidence atoms
     * @return Vector of supporting reasoning results
     */
    std::vector<ReasoningResult> findSupportingEvidence(const Handle& claim,
                                                       const std::vector<Handle>& evidence_base);
    
    /**
     * Evaluate the strength of a reasoning chain
     * @param reasoning_chain Vector of reasoning steps
     * @return Overall confidence score (0.0-1.0)
     */
    double evaluateReasoningStrength(const std::vector<ReasoningResult>& reasoning_chain);
    
    // Configuration interface
    /**
     * Configure PLN reasoning parameters
     * @param enable_pln Whether to enable PLN reasoning
     * @param confidence_threshold Minimum confidence for PLN results
     * @param truth_threshold Minimum truth value threshold
     */
    void configurePLN(bool enable_pln = true,
                     double confidence_threshold = 0.6,
                     double truth_threshold = 0.5);
    
    /**
     * Configure URE parameters
     * @param enable_ure Whether to enable URE integration
     * @param max_iterations Maximum URE iterations
     * @param complexity_penalty Penalty for complex rule chains
     */
    void configureURE(bool enable_ure = true,
                     int max_iterations = 100,
                     double complexity_penalty = 0.1);
    
    /**
     * Set reasoning limits and thresholds
     * @param max_inference_steps Maximum steps in inference chains
     * @param confidence_threshold Minimum confidence for accepting results
     */
    void setReasoningLimits(int max_inference_steps = 15,
                           double confidence_threshold = 0.6);
    
    // Status and monitoring interface
    /**
     * Get reasoning engine statistics
     * @return Map of statistics (rules_loaded, inferences_performed, etc.)
     */
    std::map<std::string, int> getReasoningStatistics();
    
    /**
     * Get the current reasoning context
     * @return Handle to the active reasoning context
     */
    Handle getReasoningContext() const { return _pln_context; }
    
    /**
     * Get status information for debugging
     * @return JSON string with detailed status
     */
    std::string getStatusInfo() const;
    
    /**
     * Clear reasoning caches and reset state
     */
    void clearReasoningCache();
    
    /**
     * Process one reasoning cycle
     * Called by the cognitive loop for ongoing reasoning
     * @return true if processing completed successfully
     */
    bool processReasoningCycle();
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_REASONING_ENGINE_H