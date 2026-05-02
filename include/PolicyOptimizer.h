/*
 * opencog/agentzero/PolicyOptimizer.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * PolicyOptimizer — MOSES-based policy evolution
 * Part of Phase 5: Continuous Learning & Adaptation
 * Part of the AGENT-ZERO-GENESIS project
 */

#ifndef _OPENCOG_AGENTZERO_POLICY_OPTIMIZER_H
#define _OPENCOG_AGENTZERO_POLICY_OPTIMIZER_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/ExperienceManager.h"

namespace opencog {
namespace agentzero {

/**
 * Configuration for the MOSES-based policy optimiser
 */
struct PolicyOptimizerConfig {
    int    population_size;  ///< Number of candidate policies per generation
    int    max_generations;  ///< Maximum evolution cycles
    double mutation_rate;    ///< Fraction of population to mutate each generation
    double crossover_rate;   ///< Fraction of population produced by crossover
    double elite_fraction;   ///< Fraction of elites preserved unchanged
    int    tournament_size;  ///< Candidates compared in tournament selection

    PolicyOptimizerConfig()
        : population_size(100)
        , max_generations(50)
        , mutation_rate(0.1)
        , crossover_rate(0.7)
        , elite_fraction(0.1)
        , tournament_size(5)
    {}
};

/**
 * A single candidate policy in the evolutionary population
 */
struct PolicyCandidate {
    Handle policy_atom;    ///< AtomSpace representation of this policy
    double fitness;        ///< Fitness score from evaluation
    int    generation;     ///< Generation in which this policy was created

    PolicyCandidate() : fitness(0.0), generation(0) {}
};

/**
 * Signature for an external policy evaluation function
 * @param policy       Policy atom to evaluate
 * @param experiences  Experience records used for evaluation
 * @return             Fitness score (higher is better)
 */
using PolicyEvaluator = std::function<double(const Handle& policy,
                                             const std::vector<Experience>& experiences)>;

/**
 * PolicyOptimizer — evolves action-selection policies via a MOSES-inspired
 * programme-evolution approach
 *
 * When MOSES is installed the optimizer delegates to it; otherwise a built-in
 * genetic-algorithm fallback is used.  Either way the interface is identical.
 *
 * Key features:
 * - Population-based evolutionary search
 * - Tournament selection with elitism
 * - Mutation and crossover of AtomSpace policy representations
 * - Fitness evaluation using stored experience records
 * - Graceful MOSES-availability detection
 */
class PolicyOptimizer
{
public:
    /**
     * Construct a PolicyOptimizer.
     * @param atomspace  AtomSpace for policy storage and manipulation.
     * @param config     Evolutionary search configuration (optional).
     */
    explicit PolicyOptimizer(AtomSpacePtr atomspace,
                             const PolicyOptimizerConfig& config = PolicyOptimizerConfig());

    virtual ~PolicyOptimizer();

    // ----------------------------------------------------------------
    // Optimisation
    // ----------------------------------------------------------------

    /**
     * Evolve a better policy starting from a seed policy.
     * @param seed_policy   Initial policy atom.
     * @param experiences   Experience records used to evaluate policies.
     * @param evaluator     External evaluation function (optional — if nullptr,
     *                      a default reward-based evaluator is used).
     * @return Handle of the best policy found, or seed_policy if no improvement.
     */
    Handle optimizePolicy(const Handle& seed_policy,
                          const std::vector<Experience>& experiences,
                          PolicyEvaluator evaluator = nullptr);

    /**
     * Evaluate a single policy against the provided experiences.
     * @param policy       Policy to score.
     * @param experiences  Evaluation data.
     * @param evaluator    Scoring function (uses default if nullptr).
     * @return Fitness score.
     */
    double evaluatePolicy(const Handle& policy,
                          const std::vector<Experience>& experiences,
                          PolicyEvaluator evaluator = nullptr) const;

    // ----------------------------------------------------------------
    // Population management
    // ----------------------------------------------------------------

    /**
     * Return the best policy currently stored in the population.
     * @return Handle of the elite policy, or Handle::UNDEFINED if empty.
     */
    Handle getBestPolicy() const;

    /**
     * Return all current population candidates.
     */
    std::vector<PolicyCandidate> getPopulation() const { return _population; }

    /**
     * Seed the population with a set of initial policies.
     * Replaces any existing population.
     * @param initial_policies  Seed policy atoms.
     */
    void seedPopulation(const std::vector<Handle>& initial_policies);

    // ----------------------------------------------------------------
    // Statistics and diagnostics
    // ----------------------------------------------------------------

    /** @return True if MOSES is available on this system. */
    bool isMOSESAvailable() const { return _moses_available; }

    /** @return Root concept node representing the policy space in the AtomSpace. */
    Handle getPolicyBase() const { return _policy_base; }

    /**
     * Aggregate statistics about the current optimisation state.
     * @return Key/value map of statistic names to values.
     */
    std::map<std::string, double> getOptimizationStatistics() const;

    // ----------------------------------------------------------------
    // Configuration
    // ----------------------------------------------------------------
    void setConfig(const PolicyOptimizerConfig& config) { _config = config; }
    const PolicyOptimizerConfig& getConfig() const { return _config; }

private:
    AtomSpacePtr          _atomspace;
    PolicyOptimizerConfig _config;

    std::vector<PolicyCandidate> _population;
    Handle                       _policy_base;
    bool                         _moses_available;
    int                          _current_generation;

    // Internal helpers
    void   initializePolicyBase();
    void   checkMOSESAvailability();
    Handle mutatePolicy(const Handle& policy);
    Handle crossoverPolicies(const Handle& parent1, const Handle& parent2);
    Handle tournamentSelect(const std::vector<PolicyCandidate>& candidates) const;
    PolicyCandidate createCandidateFromHandle(const Handle& policy_handle,
                                              const std::vector<Experience>& experiences,
                                              PolicyEvaluator evaluator) const;
    double defaultEvaluator(const Handle& policy,
                            const std::vector<Experience>& experiences) const;
    Handle runMOSESOptimization(const Handle& seed,
                                const std::vector<Experience>& experiences,
                                PolicyEvaluator evaluator);
    Handle runGAOptimization(const Handle& seed,
                             const std::vector<Experience>& experiences,
                             PolicyEvaluator evaluator);
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_POLICY_OPTIMIZER_H
