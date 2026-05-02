/*
 * src/PolicyOptimizer.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * PolicyOptimizer — MOSES-based policy evolution
 * Part of Phase 5: Continuous Learning & Adaptation
 * Part of the AGENT-ZERO-GENESIS project
 */

#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/PolicyOptimizer.h"

using namespace opencog;
using namespace opencog::agentzero;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

PolicyOptimizer::PolicyOptimizer(AtomSpacePtr atomspace,
                                 const PolicyOptimizerConfig& config)
    : _atomspace(atomspace)
    , _config(config)
    , _policy_base(Handle::UNDEFINED)
    , _moses_available(false)
    , _current_generation(0)
{
    if (!_atomspace) {
        throw std::runtime_error("[PolicyOptimizer] AtomSpace pointer must not be null");
    }
    logger().info() << "[PolicyOptimizer] Initialising policy optimization system";
    initializePolicyBase();
    checkMOSESAvailability();
    logger().info() << "[PolicyOptimizer] Initialised (MOSES available: "
                    << (_moses_available ? "yes" : "no") << ")";
}

PolicyOptimizer::~PolicyOptimizer()
{
    logger().info() << "[PolicyOptimizer] Shutting down after "
                    << _current_generation << " generation(s)";
}

// ---------------------------------------------------------------------------
// Internal initialisation
// ---------------------------------------------------------------------------

void PolicyOptimizer::initializePolicyBase()
{
    _policy_base = _atomspace->add_node(CONCEPT_NODE, "PolicySpace");
    logger().info() << "[PolicyOptimizer] Policy space initialised in AtomSpace";
}

void PolicyOptimizer::checkMOSESAvailability()
{
    // MOSES is optional; detect availability at runtime via its known AtomSpace
    // node type.  If the library is not linked, the feature simply is absent.
#ifdef HAVE_MOSES
    _moses_available = true;
    logger().info() << "[PolicyOptimizer] MOSES library detected";
#else
    _moses_available = false;
    logger().info() << "[PolicyOptimizer] MOSES library not detected; using GA fallback";
#endif
}

// ---------------------------------------------------------------------------
// Policy optimisation
// ---------------------------------------------------------------------------

Handle PolicyOptimizer::optimizePolicy(const Handle& seed_policy,
                                        const std::vector<Experience>& experiences,
                                        PolicyEvaluator evaluator)
{
    if (seed_policy == Handle::UNDEFINED) {
        logger().error() << "[PolicyOptimizer] optimizePolicy: seed policy is UNDEFINED";
        return Handle::UNDEFINED;
    }

    logger().info() << "[PolicyOptimizer] Starting policy optimisation ("
                    << (_moses_available ? "MOSES" : "GA") << " mode, "
                    << experiences.size() << " experience(s))";

    if (_moses_available) {
        return runMOSESOptimization(seed_policy, experiences, evaluator);
    } else {
        return runGAOptimization(seed_policy, experiences, evaluator);
    }
}

Handle PolicyOptimizer::runMOSESOptimization(const Handle& seed,
                                              const std::vector<Experience>& experiences,
                                              PolicyEvaluator evaluator)
{
    // When MOSES is available this would invoke the MOSES API.
    // For now we fall through to the GA implementation with a log note.
    logger().info() << "[PolicyOptimizer] MOSES optimisation delegating to GA (stub)";
    return runGAOptimization(seed, experiences, evaluator);
}

Handle PolicyOptimizer::runGAOptimization(const Handle& seed,
                                           const std::vector<Experience>& experiences,
                                           PolicyEvaluator evaluator)
{
    // Seed the population with variants of the seed policy
    if (_population.empty()) {
        seedPopulation({seed});
    }

    // Evaluate initial population
    for (auto& candidate : _population) {
        candidate.fitness = createCandidateFromHandle(
            candidate.policy_atom, experiences, evaluator).fitness;
    }

    Handle best_policy = seed;
    double best_fitness = evaluatePolicy(seed, experiences, evaluator);

    for (int gen = 0; gen < _config.max_generations; ++gen) {
        ++_current_generation;

        // Sort descending by fitness
        std::sort(_population.begin(), _population.end(),
                  [](const PolicyCandidate& a, const PolicyCandidate& b) {
                      return a.fitness > b.fitness;
                  });

        int elite_count = std::max(1, static_cast<int>(
            _config.elite_fraction * static_cast<double>(_population.size())));

        std::vector<PolicyCandidate> next_gen;
        // Keep elites
        for (int i = 0; i < elite_count && i < static_cast<int>(_population.size()); ++i) {
            next_gen.push_back(_population[static_cast<size_t>(i)]);
        }

        // Fill remainder via crossover + mutation
        std::mt19937 rng(static_cast<unsigned>(_current_generation * 31 + gen));
        std::uniform_real_distribution<double> dist(0.0, 1.0);

        while (static_cast<int>(next_gen.size()) < _config.population_size) {
            Handle parent1 = tournamentSelect(_population);

            PolicyCandidate offspring;
            if (dist(rng) < _config.crossover_rate) {
                Handle parent2 = tournamentSelect(_population);
                offspring.policy_atom = crossoverPolicies(parent1, parent2);
            } else {
                offspring.policy_atom = parent1;
            }

            if (dist(rng) < _config.mutation_rate) {
                offspring.policy_atom = mutatePolicy(offspring.policy_atom);
            }

            offspring.generation = _current_generation;
            offspring.fitness = evaluatePolicy(offspring.policy_atom, experiences, evaluator);
            next_gen.push_back(offspring);
        }

        _population = std::move(next_gen);

        // Track best
        for (const auto& c : _population) {
            if (c.fitness > best_fitness) {
                best_fitness = c.fitness;
                best_policy  = c.policy_atom;
            }
        }
    }

    logger().info() << "[PolicyOptimizer] GA optimisation complete after "
                    << _config.max_generations << " generation(s), best_fitness="
                    << best_fitness;
    return best_policy;
}

// ---------------------------------------------------------------------------
// Evaluation
// ---------------------------------------------------------------------------

double PolicyOptimizer::evaluatePolicy(const Handle& policy,
                                        const std::vector<Experience>& experiences,
                                        PolicyEvaluator evaluator) const
{
    if (evaluator) {
        return evaluator(policy, experiences);
    }
    return defaultEvaluator(policy, experiences);
}

double PolicyOptimizer::defaultEvaluator(const Handle& policy,
                                          const std::vector<Experience>& experiences) const
{
    if (experiences.empty()) return 0.0;

    // Default: score = fraction of SUCCESS experiences weighted by importance
    double score = 0.0, total_weight = 0.0;
    for (const auto& exp : experiences) {
        double weight = exp.importance;
        total_weight += weight;
        if (exp.type == ExperienceType::SUCCESS) score += weight;
    }
    return (total_weight > 0.0) ? score / total_weight : 0.0;
}

PolicyCandidate PolicyOptimizer::createCandidateFromHandle(
    const Handle& policy_handle,
    const std::vector<Experience>& experiences,
    PolicyEvaluator evaluator) const
{
    PolicyCandidate c;
    c.policy_atom  = policy_handle;
    c.generation   = _current_generation;
    c.fitness      = evaluatePolicy(policy_handle, experiences, evaluator);
    return c;
}

// ---------------------------------------------------------------------------
// Population management
// ---------------------------------------------------------------------------

void PolicyOptimizer::seedPopulation(const std::vector<Handle>& initial_policies)
{
    _population.clear();
    for (const auto& h : initial_policies) {
        PolicyCandidate c;
        c.policy_atom  = h;
        c.generation   = 0;
        c.fitness      = 0.0;
        _population.push_back(c);
    }

    // Fill to population_size by mutating the seeds
    std::mt19937 rng(42);
    while (static_cast<int>(_population.size()) < _config.population_size
           && !initial_policies.empty()) {
        std::uniform_int_distribution<size_t> idx_dist(0, initial_policies.size() - 1);
        PolicyCandidate c;
        c.policy_atom = mutatePolicy(initial_policies[idx_dist(rng)]);
        c.generation  = 0;
        c.fitness     = 0.0;
        _population.push_back(c);
    }
    logger().info() << "[PolicyOptimizer] Population seeded with "
                    << _population.size() << " candidate(s)";
}

Handle PolicyOptimizer::getBestPolicy() const
{
    if (_population.empty()) return Handle::UNDEFINED;
    auto it = std::max_element(_population.begin(), _population.end(),
                               [](const PolicyCandidate& a, const PolicyCandidate& b) {
                                   return a.fitness < b.fitness;
                               });
    return it->policy_atom;
}

// ---------------------------------------------------------------------------
// Genetic operators
// ---------------------------------------------------------------------------

Handle PolicyOptimizer::mutatePolicy(const Handle& policy)
{
    // Use C++11 random facilities to generate an unpredictable suffix
    static std::mt19937 rng{std::random_device{}()};
    static std::uniform_int_distribution<unsigned> dist(0, 99999);

    std::string name = "Policy_mut_" + std::to_string(_current_generation)
                       + "_" + std::to_string(dist(rng));
    Handle mutated = _atomspace->add_node(CONCEPT_NODE, name);
    _atomspace->add_link(INHERITANCE_LINK, mutated, policy);
    _atomspace->add_link(MEMBER_LINK, mutated, _policy_base);
    return mutated;
}

Handle PolicyOptimizer::crossoverPolicies(const Handle& parent1, const Handle& parent2)
{
    static std::mt19937 rng{std::random_device{}()};
    static std::uniform_int_distribution<unsigned> dist(0, 99999);

    std::string name = "Policy_cross_" + std::to_string(_current_generation)
                       + "_" + std::to_string(dist(rng));
    Handle child = _atomspace->add_node(CONCEPT_NODE, name);
    _atomspace->add_link(INHERITANCE_LINK, child, parent1);
    _atomspace->add_link(INHERITANCE_LINK, child, parent2);
    _atomspace->add_link(MEMBER_LINK, child, _policy_base);
    return child;
}

Handle PolicyOptimizer::tournamentSelect(const std::vector<PolicyCandidate>& candidates) const
{
    if (candidates.empty()) return Handle::UNDEFINED;

    std::mt19937 rng(static_cast<unsigned>(_current_generation));
    std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);

    size_t best_idx = dist(rng);
    for (int k = 1; k < _config.tournament_size; ++k) {
        size_t idx = dist(rng);
        if (candidates[idx].fitness > candidates[best_idx].fitness) {
            best_idx = idx;
        }
    }
    return candidates[best_idx].policy_atom;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

std::map<std::string, double> PolicyOptimizer::getOptimizationStatistics() const
{
    std::map<std::string, double> stats;
    stats["population_size"]    = static_cast<double>(_population.size());
    stats["current_generation"] = static_cast<double>(_current_generation);
    stats["moses_available"]    = _moses_available ? 1.0 : 0.0;

    if (!_population.empty()) {
        double best = -1e9, worst = 1e9, sum = 0.0;
        for (const auto& c : _population) {
            best  = std::max(best,  c.fitness);
            worst = std::min(worst, c.fitness);
            sum  += c.fitness;
        }
        stats["best_fitness"]    = best;
        stats["worst_fitness"]   = worst;
        stats["average_fitness"] = sum / static_cast<double>(_population.size());
    } else {
        stats["best_fitness"]    = 0.0;
        stats["worst_fitness"]   = 0.0;
        stats["average_fitness"] = 0.0;
    }
    return stats;
}
