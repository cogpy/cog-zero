/**
 * AtomSpaceEvolver.cpp
 *
 * Core evolutionary algorithms for AtomSpace program evolution
 * Part of Agent-Zero Learning & Adaptation Phase 5
 *
 * Copyright (C) 2024 OpenCog Foundation
 */

#include "agentzero-learning/AtomSpaceEvolver.h"

#include <algorithm>
#include <random>
#include <cmath>
#include <ctime>

#include <opencog/util/Logger.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/atom_types.h>

using namespace opencog;
using namespace opencog::agentzero;

AtomSpaceEvolver::AtomSpaceEvolver(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
    , _random_seed(static_cast<unsigned int>(std::time(nullptr)))
    , _initialized(false)
{
    logger().info() << "[AtomSpaceEvolver] Creating AtomSpace evolver";
}

AtomSpaceEvolver::~AtomSpaceEvolver()
{
    logger().info() << "[AtomSpaceEvolver] Destroyed AtomSpace evolver";
}

bool AtomSpaceEvolver::initialize()
{
    if (_initialized) {
        logger().warn() << "[AtomSpaceEvolver] Already initialized";
        return true;
    }

    logger().info() << "[AtomSpaceEvolver] Initializing AtomSpace evolver...";

    if (!_atomspace) {
        logger().error() << "[AtomSpaceEvolver] AtomSpace is null";
        return false;
    }

    // Initialize random number generator
    std::srand(_random_seed);
    
    _initialized = true;
    logger().info() << "[AtomSpaceEvolver] AtomSpace evolver initialized successfully";
    
    return true;
}

Handle AtomSpaceEvolver::evolve(const Handle& initial_program,
                               std::function<double(const Handle&)> fitness_function,
                               const EvolutionParams& params)
{
    if (!_initialized) {
        logger().error() << "[AtomSpaceEvolver] Not initialized";
        return Handle::UNDEFINED;
    }

    if (!fitness_function) {
        logger().error() << "[AtomSpaceEvolver] No fitness function provided";
        return Handle::UNDEFINED;
    }

    logger().info() << "[AtomSpaceEvolver] Starting evolution with " 
                    << params.population_size << " individuals for " 
                    << params.max_generations << " generations";

    resetStats();

    try {
        // Create initial population
        std::vector<EvolutionaryProgram> population = 
            createInitialPopulation(initial_program, params.population_size, params);

        Handle best_program = Handle::UNDEFINED;
        double best_fitness = -std::numeric_limits<double>::infinity();

        // Evolution loop
        for (int generation = 0; generation < params.max_generations; ++generation) {
            updateStats("current_generation", generation);

            // Evolve one generation
            population = evolveGeneration(population, fitness_function, params);

            // Find best individual in current generation
            for (const auto& individual : population) {
                if (individual.evaluated && individual.fitness > best_fitness) {
                    best_fitness = individual.fitness;
                    best_program = individual.program_atom;
                }
            }

            updateStats("best_fitness", best_fitness);

            // Check termination criteria
            if (best_fitness >= params.fitness_threshold) {
                logger().info() << "[AtomSpaceEvolver] Fitness threshold reached at generation " 
                               << generation << " with fitness " << best_fitness;
                break;
            }

            // Log progress every 10 generations
            if (generation % 10 == 0) {
                double diversity = calculatePopulationDiversity(population);
                logger().info() << "[AtomSpaceEvolver] Generation " << generation 
                               << ": best_fitness=" << best_fitness 
                               << ", diversity=" << diversity;
                updateStats("population_diversity", diversity);
            }
        }

        logger().info() << "[AtomSpaceEvolver] Evolution completed. Best fitness: " << best_fitness;
        updateStats("final_best_fitness", best_fitness);
        updateStats("evolution_completed", 1);

        return best_program;
    }
    catch (const std::exception& e) {
        logger().error() << "[AtomSpaceEvolver] Evolution error: " << e.what();
        updateStats("evolution_errors", 1);
        return Handle::UNDEFINED;
    }
}

Handle AtomSpaceEvolver::evolveWithAtomspaceFitness(const Handle& initial_program,
                                                   const Handle& fitness_atom,
                                                   const EvolutionParams& params)
{
    if (!_initialized) {
        logger().error() << "[AtomSpaceEvolver] Not initialized";
        return Handle::UNDEFINED;
    }

    logger().info() << "[AtomSpaceEvolver] Starting AtomSpace-based evolution";

    // Create a fitness function that uses the AtomSpace fitness atom
    auto atomspace_fitness_function = [this, fitness_atom](const Handle& program) -> double {
        try {
            // For now, use a simple evaluation based on program complexity
            // In a full implementation, this would evaluate the fitness_atom
            // with the program as input
            
            if (program == Handle::UNDEFINED) {
                return 0.0;
            }

            // Simple fitness based on program structure
            // This is a placeholder - real implementation would use the fitness_atom
            return 1.0 / (1.0 + program->get_incoming_set().size());
        }
        catch (const std::exception& e) {
            logger().error() << "[AtomSpaceEvolver] Fitness evaluation error: " << e.what();
            return 0.0;
        }
    };

    return evolve(initial_program, atomspace_fitness_function, params);
}

std::vector<EvolutionaryProgram> AtomSpaceEvolver::createInitialPopulation(
    const Handle& template_program,
    int population_size,
    const EvolutionParams& params)
{
    logger().info() << "[AtomSpaceEvolver] Creating initial population of size " << population_size;

    std::vector<EvolutionaryProgram> population;
    population.reserve(population_size);

    for (int i = 0; i < population_size; ++i) {
        Handle program;
        
        if (template_program != Handle::UNDEFINED && i == 0) {
            // Keep the template as the first individual
            program = template_program;
        } else if (template_program != Handle::UNDEFINED) {
            // Mutate the template for other individuals
            program = mutate(template_program, params);
        } else {
            // Generate random programs
            program = generateRandomProgram(params.max_program_size, 
                                          params.allowed_types, 
                                          params.max_tree_depth);
        }

        if (program != Handle::UNDEFINED && validateProgram(program, params)) {
            population.emplace_back(program, 0);
        } else {
            // If generation failed, try again with a simpler approach
            program = _atomspace->add_node(CONCEPT_NODE, "program_" + std::to_string(i));
            population.emplace_back(program, 0);
        }
    }

    logger().info() << "[AtomSpaceEvolver] Created initial population with " 
                    << population.size() << " valid individuals";
    
    return population;
}

std::vector<EvolutionaryProgram> AtomSpaceEvolver::evolveGeneration(
    const std::vector<EvolutionaryProgram>& population,
    std::function<double(const Handle&)> fitness_function,
    const EvolutionParams& params)
{
    std::vector<EvolutionaryProgram> new_population;
    new_population.reserve(population.size());

    // Evaluate fitness for all individuals
    std::vector<EvolutionaryProgram> evaluated_population = population;
    for (auto& individual : evaluated_population) {
        if (!individual.evaluated) {
            individual.fitness = fitness_function(individual.program_atom);
            individual.evaluated = true;
        }
    }

    // Sort by fitness (descending)
    std::sort(evaluated_population.begin(), evaluated_population.end(),
              [](const EvolutionaryProgram& a, const EvolutionaryProgram& b) {
                  return a.fitness > b.fitness;
              });

    // Elitism - keep best individuals
    if (params.use_elitism) {
        int elite_count = std::min(params.elite_size, static_cast<int>(evaluated_population.size()));
        for (int i = 0; i < elite_count; ++i) {
            new_population.push_back(evaluated_population[i]);
        }
    }

    // Generate offspring to fill the rest of the population
    while (new_population.size() < population.size()) {
        // Select parents
        auto parents = selectParents(evaluated_population, 2, params);
        
        if (parents.size() >= 2) {
            // Crossover
            Handle offspring = crossover(parents[0].program_atom, parents[1].program_atom, params);
            
            // Mutation
            if (static_cast<double>(std::rand()) / RAND_MAX < params.mutation_rate) {
                offspring = mutate(offspring, params);
            }
            
            // Validate and add to population
            if (offspring != Handle::UNDEFINED && validateProgram(offspring, params)) {
                new_population.emplace_back(offspring, evaluated_population[0].generation + 1);
            } else {
                // If offspring is invalid, add a random parent
                new_population.push_back(parents[0]);
            }
        } else {
            // Fallback: add a mutated version of a random individual
            if (!evaluated_population.empty()) {
                int random_idx = std::rand() % evaluated_population.size();
                Handle mutated = mutate(evaluated_population[random_idx].program_atom, params);
                if (mutated != Handle::UNDEFINED && validateProgram(mutated, params)) {
                    new_population.emplace_back(mutated, evaluated_population[0].generation + 1);
                } else {
                    new_population.push_back(evaluated_population[random_idx]);
                }
            }
        }
    }

    // Ensure population size is exactly as requested
    new_population.resize(population.size());

    return new_population;
}

std::vector<EvolutionaryProgram> AtomSpaceEvolver::selectParents(
    const std::vector<EvolutionaryProgram>& population,
    int num_parents,
    const EvolutionParams& params)
{
    std::vector<EvolutionaryProgram> parents;
    parents.reserve(num_parents);

    for (int i = 0; i < num_parents; ++i) {
        // Tournament selection
        EvolutionaryProgram selected = tournamentSelection(population, 3);
        parents.push_back(selected);
    }

    return parents;
}

Handle AtomSpaceEvolver::crossover(const Handle& parent1, const Handle& parent2,
                                  const EvolutionParams& params)
{
    // Simple crossover: randomly choose between parents
    // In a full implementation, this would perform subtree crossover
    
    if (static_cast<double>(std::rand()) / RAND_MAX < 0.5) {
        return parent1;
    } else {
        return parent2;
    }
}

Handle AtomSpaceEvolver::mutate(const Handle& program, const EvolutionParams& params)
{
    // Simple mutation: create a new related atom
    // In a full implementation, this would perform various mutation operations
    
    if (program == Handle::UNDEFINED) {
        return Handle::UNDEFINED;
    }

    try {
        // Create a mutated version by changing the name slightly
        std::string original_name = program->get_name();
        std::string mutated_name = original_name + "_mut" + std::to_string(std::rand() % 1000);
        
        return _atomspace->add_node(program->get_type(), mutated_name);
    }
    catch (const std::exception& e) {
        logger().error() << "[AtomSpaceEvolver] Mutation error: " << e.what();
        return program; // Return original if mutation fails
    }
}

bool AtomSpaceEvolver::validateProgram(const Handle& program, const EvolutionParams& params)
{
    if (program == Handle::UNDEFINED) {
        return false;
    }

    // Basic validation
    if (params.use_type_constraints && !params.allowed_types.empty()) {
        // Check if program type is allowed
        Type program_type = program->get_type();
        bool type_allowed = std::find(params.allowed_types.begin(), 
                                     params.allowed_types.end(), 
                                     program_type) != params.allowed_types.end();
        if (!type_allowed) {
            return false;
        }
    }

    // Check program size constraints
    if (params.max_program_size > 0) {
        // Simple size check - in full implementation would count all atoms in tree
        if (program->get_incoming_set().size() > static_cast<size_t>(params.max_program_size)) {
            return false;
        }
    }

    return true;
}

std::map<std::string, double> AtomSpaceEvolver::getEvolutionStats() const
{
    return _stats;
}

void AtomSpaceEvolver::resetStats()
{
    _stats.clear();
}

void AtomSpaceEvolver::setRandomSeed(unsigned int seed)
{
    _random_seed = seed;
    std::srand(seed);
}

Handle AtomSpaceEvolver::generateRandomProgram(int max_size, 
                                              const std::vector<Type>& allowed_types,
                                              int max_depth)
{
    try {
        // Simple random program generation
        std::string program_name = "random_program_" + std::to_string(std::rand());
        
        Type program_type = CONCEPT_NODE;
        if (!allowed_types.empty()) {
            program_type = allowed_types[std::rand() % allowed_types.size()];
        }
        
        return _atomspace->add_node(program_type, program_name);
    }
    catch (const std::exception& e) {
        logger().error() << "[AtomSpaceEvolver] Random program generation error: " << e.what();
        return Handle::UNDEFINED;
    }
}

double AtomSpaceEvolver::calculatePopulationDiversity(const std::vector<EvolutionaryProgram>& population)
{
    if (population.size() < 2) {
        return 0.0;
    }

    // Simple diversity measure: count unique program names
    std::set<std::string> unique_names;
    for (const auto& individual : population) {
        if (individual.program_atom != Handle::UNDEFINED) {
            unique_names.insert(individual.program_atom->get_name());
        }
    }

    return static_cast<double>(unique_names.size()) / population.size();
}

EvolutionaryProgram AtomSpaceEvolver::tournamentSelection(const std::vector<EvolutionaryProgram>& population,
                                                         int tournament_size)
{
    if (population.empty()) {
        return EvolutionaryProgram();
    }

    EvolutionaryProgram best = population[std::rand() % population.size()];
    
    for (int i = 1; i < tournament_size && i < static_cast<int>(population.size()); ++i) {
        EvolutionaryProgram candidate = population[std::rand() % population.size()];
        if (candidate.fitness > best.fitness) {
            best = candidate;
        }
    }

    return best;
}

void AtomSpaceEvolver::updateStats(const std::string& metric, double value)
{
    _stats[metric] = value;
}

Handle AtomSpaceEvolver::getSubtree(const Handle& atom, int position)
{
    // Placeholder implementation
    // Full implementation would traverse tree to find subtree at position
    return atom;
}

Handle AtomSpaceEvolver::replaceSubtree(const Handle& atom, int position, const Handle& replacement)
{
    // Placeholder implementation
    // Full implementation would replace subtree at position with replacement
    return replacement;
}