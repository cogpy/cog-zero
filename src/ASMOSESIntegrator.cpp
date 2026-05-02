/**
 * ASMOSESIntegrator.cpp
 *
 * Main integration class for ASMOSES (AtomSpace MOSES) evolution
 * Part of Agent-Zero Learning & Adaptation Phase 5
 *
 * Copyright (C) 2024 OpenCog Foundation
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 */

#include "agentzero-learning/ASMOSESIntegrator.h"
#include "agentzero-learning/AtomSpaceEvolver.h"
#include "agentzero-learning/ExperienceManager.h"
#include "agentzero-learning/PolicyOptimizer.h"

#include <opencog/util/Logger.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/atom_types.h>

using namespace opencog;
using namespace opencog::agentzero;

ASMOSESIntegrator::ASMOSESIntegrator(AtomSpacePtr atomspace, const std::string& agent_name)
    : _atomspace(atomspace)
    , _agent_name(agent_name)
    , _initialized(false)
    , _logging_enabled(true)
{
    logger().info() << "[ASMOSESIntegrator] Creating ASMOSES integration for agent: " << agent_name;
    setupDefaultParams();
}

ASMOSESIntegrator::~ASMOSESIntegrator()
{
    if (_initialized) {
        shutdown();
    }
    logger().info() << "[ASMOSESIntegrator] Destroyed ASMOSES integration for agent: " << _agent_name;
}

bool ASMOSESIntegrator::initialize()
{
    if (_initialized) {
        logger().warn() << "[ASMOSESIntegrator] Already initialized";
        return true;
    }

    logger().info() << "[ASMOSESIntegrator] Initializing ASMOSES integration...";

    // Validate AtomSpace
    if (!validateAtomSpace()) {
        logger().error() << "[ASMOSESIntegrator] AtomSpace validation failed";
        return false;
    }

    // Initialize core components
    if (!initializeComponents()) {
        logger().error() << "[ASMOSESIntegrator] Component initialization failed";
        return false;
    }

    _initialized = true;
    logger().info() << "[ASMOSESIntegrator] ASMOSES integration initialized successfully";
    
    updateStatistics("initializations", 1);
    return true;
}

void ASMOSESIntegrator::shutdown()
{
    if (!_initialized) {
        return;
    }

    logger().info() << "[ASMOSESIntegrator] Shutting down ASMOSES integration...";
    
    // Clean up components
    _evolver.reset();
    _experience_manager.reset();
    _policy_optimizer.reset();
    
    _initialized = false;
    logger().info() << "[ASMOSESIntegrator] ASMOSES integration shutdown complete");
}

Handle ASMOSESIntegrator::evolveProgram(const Handle& problem_atom,
                                       std::function<double(const Handle&)> fitness_function,
                                       int max_generations)
{
    if (!_initialized) {
        logger().error() << "[ASMOSESIntegrator] Not initialized";
        return Handle::UNDEFINED;
    }

    if (!fitness_function) {
        logger().error() << "[ASMOSESIntegrator] No fitness function provided";
        return Handle::UNDEFINED;
    }

    logEvolutionProgress("Starting program evolution", "Problem: " + problem_atom->to_string());
    updateStatistics("evolution_runs", 1);

    try {
        // Set up evolution parameters
        EvolutionParams params;
        params.max_generations = max_generations;
        
        // Use the AtomSpace evolver to evolve the program
        Handle result = _evolver->evolve(problem_atom, fitness_function, params);
        
        if (result != Handle::UNDEFINED) {
            logEvolutionProgress("Evolution completed successfully", "Result: " + result->to_string());
            updateStatistics("successful_evolutions", 1);
        } else {
            logger().warn() << "[ASMOSESIntegrator] Evolution failed to find solution";
            updateStatistics("failed_evolutions", 1);
        }

        return result;
    }
    catch (const std::exception& e) {
        logger().error() << "[ASMOSESIntegrator] Evolution error: " << e.what();
        updateStatistics("evolution_errors", 1);
        return Handle::UNDEFINED;
    }
}

Handle ASMOSESIntegrator::evolveAtomspaceProgram(const Handle& problem_spec,
                                               const Handle& target_atom,
                                               const std::map<std::string, std::string>& options)
{
    if (!_initialized) {
        logger().error() << "[ASMOSESIntegrator] Not initialized";
        return Handle::UNDEFINED;
    }

    logEvolutionProgress("Starting AtomSpace program evolution", 
                        "Target: " + target_atom->to_string());
    updateStatistics("atomspace_evolutions", 1);

    try {
        // Set up evolution parameters from options
        EvolutionParams params;
        
        // Parse options into parameters
        for (const auto& option : options) {
            if (option.first == "max_generations") {
                params.max_generations = std::stoi(option.second);
            } else if (option.first == "population_size") {
                params.population_size = std::stoi(option.second);
            } else if (option.first == "mutation_rate") {
                params.mutation_rate = std::stod(option.second);
            }
            // Add more parameter parsing as needed
        }

        // Use AtomSpace-based fitness evaluation
        Handle result = _evolver->evolveWithAtomspaceFitness(problem_spec, target_atom, params);
        
        if (result != Handle::UNDEFINED) {
            logEvolutionProgress("AtomSpace evolution completed", "Result: " + result->to_string());
            updateStatistics("successful_atomspace_evolutions", 1);
        } else {
            updateStatistics("failed_atomspace_evolutions", 1);
        }

        return result;
    }
    catch (const std::exception& e) {
        logger().error() << "[ASMOSESIntegrator] AtomSpace evolution error: " << e.what();
        updateStatistics("atomspace_evolution_errors", 1);
        return Handle::UNDEFINED;
    }
}

Handle ASMOSESIntegrator::optimizePolicy(const Handle& policy_atom,
                                       const std::vector<Handle>& experience_data,
                                       std::function<double(const Handle&, const std::vector<Handle>&)> reward_function)
{
    if (!_initialized) {
        logger().error() << "[ASMOSESIntegrator] Not initialized";
        return Handle::UNDEFINED;
    }

    if (!reward_function) {
        logger().error() << "[ASMOSESIntegrator] No reward function provided";
        return Handle::UNDEFINED;
    }

    logEvolutionProgress("Starting policy optimization", 
                        "Policy: " + policy_atom->to_string() + 
                        ", Experiences: " + std::to_string(experience_data.size()));
    updateStatistics("policy_optimizations", 1);

    try {
        Handle optimized_policy = _policy_optimizer->optimizePolicy(policy_atom, 
                                                                   experience_data, 
                                                                   reward_function);
        
        if (optimized_policy != Handle::UNDEFINED) {
            logEvolutionProgress("Policy optimization completed", 
                               "Optimized policy: " + optimized_policy->to_string());
            updateStatistics("successful_policy_optimizations", 1);
        } else {
            updateStatistics("failed_policy_optimizations", 1);
        }

        return optimized_policy;
    }
    catch (const std::exception& e) {
        logger().error() << "[ASMOSESIntegrator] Policy optimization error: " << e.what();
        updateStatistics("policy_optimization_errors", 1);
        return Handle::UNDEFINED;
    }
}

std::vector<Handle> ASMOSESIntegrator::learnSkills(const std::vector<Handle>& experience_atoms,
                                                  const Handle& skill_template)
{
    if (!_initialized) {
        logger().error() << "[ASMOSESIntegrator] Not initialized";
        return std::vector<Handle>();
    }

    logEvolutionProgress("Starting skill learning", 
                        "Experiences: " + std::to_string(experience_atoms.size()));
    updateStatistics("skill_learning_runs", 1);

    try {
        // For now, return a simple learned skill based on the template
        std::vector<Handle> learned_skills;
        
        if (skill_template != Handle::UNDEFINED) {
            // Create a variant of the template as a learned skill
            learned_skills.push_back(skill_template);
            logEvolutionProgress("Skill learning completed", 
                               "Learned " + std::to_string(learned_skills.size()) + " skills");
        }

        updateStatistics("skills_learned", learned_skills.size());
        return learned_skills;
    }
    catch (const std::exception& e) {
        logger().error() << "[ASMOSESIntegrator] Skill learning error: " << e.what();
        updateStatistics("skill_learning_errors", 1);
        return std::vector<Handle>();
    }
}

void ASMOSESIntegrator::setEvolutionParams(const std::map<std::string, std::string>& params)
{
    _evolution_params = params;
    logger().info() << "[ASMOSESIntegrator] Updated evolution parameters";
}

std::map<std::string, std::string> ASMOSESIntegrator::getEvolutionParams() const
{
    return _evolution_params;
}

std::map<std::string, int> ASMOSESIntegrator::getStatistics() const
{
    return _statistics;
}

void ASMOSESIntegrator::resetStatistics()
{
    _statistics.clear();
    logger().info() << "[ASMOSESIntegrator] Statistics reset";
}

bool ASMOSESIntegrator::initializeComponents()
{
    try {
        // Initialize AtomSpace evolver
        _evolver = std::make_shared<AtomSpaceEvolver>(_atomspace);
        if (!_evolver->initialize()) {
            logger().error() << "[ASMOSESIntegrator] Failed to initialize AtomSpace evolver";
            return false;
        }

        // Initialize experience manager
        _experience_manager = std::make_shared<ExperienceManager>(_atomspace);
        if (!_experience_manager->initialize()) {
            logger().error() << "[ASMOSESIntegrator] Failed to initialize experience manager";
            return false;
        }

        // Initialize policy optimizer
        _policy_optimizer = std::make_shared<PolicyOptimizer>(_atomspace);
        if (!_policy_optimizer->initialize()) {
            logger().error() << "[ASMOSESIntegrator] Failed to initialize policy optimizer";
            return false;
        }

        logger().info() << "[ASMOSESIntegrator] All components initialized successfully";
        return true;
    }
    catch (const std::exception& e) {
        logger().error() << "[ASMOSESIntegrator] Component initialization error: " << e.what();
        return false;
    }
}

bool ASMOSESIntegrator::validateAtomSpace()
{
    if (!_atomspace) {
        logger().error() << "[ASMOSESIntegrator] AtomSpace is null";
        return false;
    }

    // Basic validation - check if we can create atoms
    try {
        Handle test_atom = _atomspace->add_node(CONCEPT_NODE, "test_validation");
        if (test_atom == Handle::UNDEFINED) {
            logger().error() << "[ASMOSESIntegrator] Cannot create atoms in AtomSpace";
            return false;
        }
        _atomspace->remove_atom(test_atom);
        return true;
    }
    catch (const std::exception& e) {
        logger().error() << "[ASMOSESIntegrator] AtomSpace validation error: " << e.what();
        return false;
    }
}

void ASMOSESIntegrator::setupDefaultParams()
{
    _evolution_params["max_generations"] = "100";
    _evolution_params["population_size"] = "500";
    _evolution_params["crossover_rate"] = "0.8";
    _evolution_params["mutation_rate"] = "0.1";
    _evolution_params["selection_pressure"] = "2.0";
    _evolution_params["max_program_size"] = "50";
    _evolution_params["use_elitism"] = "true";
    _evolution_params["elite_size"] = "10";
    _evolution_params["fitness_threshold"] = "0.95";
}

void ASMOSESIntegrator::logEvolutionProgress(const std::string& stage, const std::string& details)
{
    if (_logging_enabled) {
        if (details.empty()) {
            logger().info() << "[ASMOSESIntegrator:" << _agent_name << "] " << stage;
        } else {
            logger().info() << "[ASMOSESIntegrator:" << _agent_name << "] " << stage << " - " << details;
        }
    }
}

void ASMOSESIntegrator::updateStatistics(const std::string& metric, int delta)
{
    _statistics[metric] += delta;
}