/*
 * opencog/agentzero/knowledge/PLNRuleLibrary.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * PLNRuleLibrary - PLN rule library for common agent-reasoning patterns.
 * Part of the AGENT-ZERO-GENESIS project - Phase 3 Knowledge Module
 */

#ifndef _OPENCOG_AGENTZERO_KNOWLEDGE_PLN_RULE_LIBRARY_H
#define _OPENCOG_AGENTZERO_KNOWLEDGE_PLN_RULE_LIBRARY_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <mutex>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

#ifdef HAVE_URE
#include <opencog/ure/UREConfig.h>
#include <opencog/ure/Rule.h>
#endif

#ifdef HAVE_PLN
#include <opencog/pln/rules/PLNRules.h>
#endif

namespace opencog {
namespace agentzero {
namespace knowledge {

/**
 * PLNRule - Descriptor for a single PLN rule registered in the library
 */
struct PLNRule {
    std::string name;         ///< Human-readable rule name
    std::string category;     ///< E.g. "deduction", "abduction", "analogy"
    Handle rule_atom;         ///< BindLink or ImplicationLink in AtomSpace
    std::string description;
    bool active{true};        ///< Whether the rule is enabled
    size_t fire_count{0};     ///< How many times the rule has fired
};

/**
 * InferenceResult - Output of a single rule application or chaining run
 */
struct InferenceResult {
    Handle conclusion;        ///< The derived atom
    double strength{0.0};     ///< Truth-value strength
    double confidence{0.0};   ///< Truth-value confidence
    std::string rule_used;    ///< Name of the rule that produced this
    std::vector<Handle> premises; ///< Handles of the inputs used
    bool success{false};
    std::string error_message;
};

/**
 * PLNRuleLibrary - PLN rule library for common agent-reasoning patterns
 *
 * Provides a curated set of PLN/URE rules specifically designed for
 * agent reasoning tasks, plus infrastructure to:
 * - Load and register rules into the AtomSpace
 * - Apply forward/backward chaining
 * - Query which conclusions follow from a given set of premises
 * - Persist and reload rule sets
 *
 * Built-in rule categories:
 *  - **Deduction**    : A→B, B→C ⊢ A→C  (transitivity)
 *  - **Inversion**    : P(A|B) ⊢ P(B|A) (Bayes' theorem proxy)
 *  - **Modus Ponens** : A, A→B ⊢ B
 *  - **Abduction**    : A→C, B→C ⊢ A→B  (explains common effect)
 *  - **Analogy**      : A~B, B→C ⊢ A→C  (similarity transfer)
 *  - **Causal**       : Cause→Effect with temporal ordering
 *  - **Goal**         : Goal(G), CanAchieve(A,G) ⊢ PursueAction(A)
 *
 * @code
 *   PLNRuleLibrary lib(atomspace);
 *   lib.initialize();
 *   lib.loadBuiltinRules();
 *   auto results = lib.forwardChain(my_premises, 3);
 * @endcode
 */
class PLNRuleLibrary
{
public:
    /**
     * Constructor
     * @param atomspace Shared pointer to the backing AtomSpace
     */
    explicit PLNRuleLibrary(AtomSpacePtr atomspace);

    /**
     * Destructor
     */
    ~PLNRuleLibrary() = default;

    // =========================================================
    // Lifecycle
    // =========================================================

    /**
     * Initialize and verify AtomSpace connectivity.
     */
    bool initialize();

    bool shutdown();

    bool isInitialized() const { return _initialized; }

    // =========================================================
    // Rule Registration
    // =========================================================

    /**
     * Load the built-in agent-reasoning rule set.
     *
     * Creates all built-in rule atoms in the AtomSpace and registers them.
     * Safe to call multiple times (idempotent).
     *
     * @return Number of rules loaded
     */
    size_t loadBuiltinRules();

    /**
     * Register a custom rule from an existing AtomSpace atom.
     *
     * @param rule_atom  A BindLink or ImplicationLink in the AtomSpace
     * @param name       Human-readable name
     * @param category   Rule category for filtering
     * @param description Brief description of what the rule does
     * @return True if the rule was registered
     */
    bool registerRule(const Handle& rule_atom,
                      const std::string& name,
                      const std::string& category = "custom",
                      const std::string& description = "");

    /**
     * Unregister (disable) a rule by name.
     * The atom is not removed from the AtomSpace.
     */
    bool unregisterRule(const std::string& name);

    /**
     * Enable or disable a rule.
     */
    bool setRuleActive(const std::string& name, bool active);

    /**
     * Return all registered rules.
     */
    std::vector<PLNRule> getRules() const;

    /**
     * Return rules filtered by category.
     */
    std::vector<PLNRule> getRulesByCategory(const std::string& category) const;

    /**
     * Find a rule by name.
     */
    std::optional<PLNRule> findRule(const std::string& name) const;

    // =========================================================
    // Inference
    // =========================================================

    /**
     * Apply all active rules in forward-chaining mode starting from
     * `premises` for up to `max_steps` iterations.
     *
     * Each step applies every enabled rule to the current knowledge set
     * and adds newly derived atoms to it.  Stops when no new atoms are
     * derived or `max_steps` is reached.
     *
     * @param premises   Set of premise handles to reason from
     * @param max_steps  Maximum chaining depth (default: 5)
     * @return All conclusions derived, in order of discovery
     */
    std::vector<InferenceResult> forwardChain(
        const std::vector<Handle>& premises,
        size_t max_steps = 5);

    /**
     * Apply rules in backward-chaining mode to prove `goal`.
     *
     * @param goal       The target atom to prove
     * @param max_steps  Maximum chaining depth
     * @return Inference results leading to the goal (if proved)
     */
    std::vector<InferenceResult> backwardChain(
        const Handle& goal,
        size_t max_steps = 5);

    /**
     * Apply a single named rule to the given premises.
     *
     * @param rule_name  Name of the rule to apply
     * @param premises   Premise handles to unify with rule antecedents
     * @return The inference result (success=false if the rule cannot fire)
     */
    InferenceResult applyRule(const std::string& rule_name,
                              const std::vector<Handle>& premises);

    /**
     * Return all conclusions that can be derived from `premises` using
     * exactly one rule application.
     */
    std::vector<InferenceResult> shallowInfer(
        const std::vector<Handle>& premises);

    // =========================================================
    // Truth-Value Computation
    // =========================================================

    /**
     * Compute the PLN deduction truth value for:
     *   P(A→C) given P(A→B) and P(B→C)
     *
     * Uses the standard PLN deduction formula.
     *
     * @param strength_ab  Strength of A→B
     * @param conf_ab      Confidence of A→B
     * @param strength_bc  Strength of B→C
     * @param conf_bc      Confidence of B→C
     * @param[out] strength_ac  Resulting strength of A→C
     * @param[out] conf_ac      Resulting confidence of A→C
     */
    static void deductionTV(double strength_ab, double conf_ab,
                             double strength_bc, double conf_bc,
                             double& strength_ac, double& conf_ac);

    /**
     * Compute the PLN modus ponens truth value for:
     *   P(B) given P(A) and P(A→B)
     *
     * @param strength_a   Strength of A
     * @param conf_a       Confidence of A
     * @param strength_ab  Strength of A→B
     * @param conf_ab      Confidence of A→B
     * @param[out] strength_b  Resulting strength of B
     * @param[out] conf_b      Resulting confidence of B
     */
    static void modusPonensTV(double strength_a, double conf_a,
                               double strength_ab, double conf_ab,
                               double& strength_b, double& conf_b);

    /**
     * Compute the PLN inversion (Bayes) truth value for:
     *   P(B|A) given P(A|B), P(A), P(B)
     */
    static void inversionTV(double strength_ab, double conf_ab,
                             double strength_a, double conf_a,
                             double strength_b, double conf_b,
                             double& strength_ba, double& conf_ba);

    // =========================================================
    // Statistics and Monitoring
    // =========================================================

    /**
     * Return a human-readable statistics summary.
     */
    std::string getStatsSummary() const;

    bool isHealthy() const;

private:
    // -------------------------
    // Rule atom builders
    // -------------------------

    Handle buildDeductionRule();
    Handle buildInversionRule();
    Handle buildModusPonensRule();
    Handle buildAbductionRule();
    Handle buildAnalogyRule();
    Handle buildCausalRule();
    Handle buildGoalPursuitRule();

    /**
     * Attempt to unify `rule_atom` with `premises`.
     * Returns the set of conclusion atoms, or an empty set on failure.
     */
    std::vector<Handle> tryUnify(const Handle& rule_atom,
                                 const std::vector<Handle>& premises) const;

    /**
     * Apply PLN deduction formula to a concrete A→B + B→C pair in the
     * AtomSpace and return the resulting A→C atom.
     */
    Handle applyDeduction(const Handle& ab, const Handle& bc) const;

    /**
     * Apply modus ponens: A + A→B → B.
     */
    Handle applyModusPonens(const Handle& a, const Handle& ab) const;

    // -------------------------
    // State
    // -------------------------

    AtomSpacePtr _atomspace;
    bool _initialized{false};
    mutable std::mutex _mutex;

    std::map<std::string, PLNRule> _rules;      ///< name → PLNRule
    std::map<std::string, std::set<std::string>> _category_index;
};

} // namespace knowledge
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_KNOWLEDGE_PLN_RULE_LIBRARY_H
