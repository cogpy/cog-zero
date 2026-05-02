/**
 * AtomSpaceEvolver.h
 *
 * Core evolutionary algorithms for AtomSpace program evolution
 * Part of Agent-Zero Learning & Adaptation Phase 5
 *
 * Copyright (C) 2024 OpenCog Foundation
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _AGENTZERO_ATOMSPACE_EVOLVER_H
#define _AGENTZERO_ATOMSPACE_EVOLVER_H

#include <memory>
#include <vector>
#include <functional>
#include <map>
#include <string>

#include <opencog/util/Logger.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

namespace opencog { namespace agentzero {

/**
 * Structure to hold evolution parameters
 */
struct EvolutionParams {
    int max_generations = 100;
    int population_size = 500;
    double crossover_rate = 0.8;
    double mutation_rate = 0.1;
    double selection_pressure = 2.0;
    int max_program_size = 50;
    bool use_elitism = true;
    int elite_size = 10;
    double fitness_threshold = 0.95;
    
    // AtomSpace-specific parameters
    bool preserve_semantics = true;
    bool use_type_constraints = true;
    int max_tree_depth = 10;
    std::vector<Type> allowed_types;
};

/**
 * Individual program in the evolutionary population
 */
struct EvolutionaryProgram {
    Handle program_atom;      // The program as an AtomSpace atom
    double fitness;           // Fitness score
    int generation;           // Generation when created
    bool evaluated;           // Whether fitness has been calculated
    
    EvolutionaryProgram() : fitness(0.0), generation(0), evaluated(false) {}
    EvolutionaryProgram(Handle atom, int gen) 
        : program_atom(atom), fitness(0.0), generation(gen), evaluated(false) {}
};

/**
 * AtomSpaceEvolver - Core evolutionary engine for AtomSpace programs
 * 
 * This class implements evolutionary algorithms specifically designed to
 * work with AtomSpace representations. It can evolve Atomese programs
 * while maintaining semantic validity and type constraints.
 */
class AtomSpaceEvolver
{
public:
    /**
     * Constructor
     * @param atomspace The AtomSpace to operate on
     */
    explicit AtomSpaceEvolver(AtomSpacePtr atomspace);

    /**
     * Destructor
     */
    virtual ~AtomSpaceEvolver();

    /**
     * Initialize the evolver
     * @return true if successful
     */
    bool initialize();

    /**
     * Evolve a program to solve a problem
     * @param initial_program Starting program (can be Handle::UNDEFINED for random start)
     * @param fitness_function Function to evaluate program fitness
     * @param params Evolution parameters
     * @return Best evolved program
     */
    Handle evolve(const Handle& initial_program,
                  std::function<double(const Handle&)> fitness_function,
                  const EvolutionParams& params = EvolutionParams());

    /**
     * Evolve with AtomSpace-based fitness evaluation
     * @param initial_program Starting program
     * @param fitness_atom Atom that defines fitness evaluation
     * @param params Evolution parameters
     * @return Best evolved program
     */
    Handle evolveWithAtomspaceFitness(const Handle& initial_program,
                                     const Handle& fitness_atom,
                                     const EvolutionParams& params = EvolutionParams());

    /**
     * Create initial population of programs
     * @param template_program Template or seed program
     * @param population_size Size of population to create
     * @param params Evolution parameters for constraints
     * @return Vector of initial programs
     */
    std::vector<EvolutionaryProgram> createInitialPopulation(
        const Handle& template_program,
        int population_size,
        const EvolutionParams& params = EvolutionParams());

    /**
     * Perform one generation of evolution
     * @param population Current population
     * @param fitness_function Fitness evaluation function
     * @param params Evolution parameters
     * @return New population after evolution
     */
    std::vector<EvolutionaryProgram> evolveGeneration(
        const std::vector<EvolutionaryProgram>& population,
        std::function<double(const Handle&)> fitness_function,
        const EvolutionParams& params);

    /**
     * Select parents for reproduction
     * @param population Current population
     * @param num_parents Number of parents to select
     * @param params Selection parameters
     * @return Selected parent programs
     */
    std::vector<EvolutionaryProgram> selectParents(
        const std::vector<EvolutionaryProgram>& population,
        int num_parents,
        const EvolutionParams& params);

    /**
     * Perform crossover between two parent programs
     * @param parent1 First parent program
     * @param parent2 Second parent program
     * @param params Crossover parameters
     * @return Offspring program
     */
    Handle crossover(const Handle& parent1, const Handle& parent2,
                    const EvolutionParams& params);

    /**
     * Mutate a program
     * @param program Program to mutate
     * @param params Mutation parameters
     * @return Mutated program
     */
    Handle mutate(const Handle& program, const EvolutionParams& params);

    /**
     * Validate program semantics and constraints
     * @param program Program to validate
     * @param params Validation parameters
     * @return true if program is valid
     */
    bool validateProgram(const Handle& program, const EvolutionParams& params);

    /**
     * Get evolution statistics
     */
    std::map<std::string, double> getEvolutionStats() const;

    /**
     * Reset evolution statistics
     */
    void resetStats();

    /**
     * Set random seed for reproducible evolution
     */
    void setRandomSeed(unsigned int seed);

protected:
    AtomSpacePtr _atomspace;
    mutable std::map<std::string, double> _stats;
    unsigned int _random_seed;
    bool _initialized;

    /**
     * Generate a random program within constraints
     * @param max_size Maximum program size
     * @param allowed_types Types that can be used
     * @param max_depth Maximum tree depth
     * @return Random program atom
     */
    Handle generateRandomProgram(int max_size, 
                               const std::vector<Type>& allowed_types,
                               int max_depth);

    /**
     * Calculate diversity in population
     * @param population Population to analyze
     * @return Diversity metric (0.0 to 1.0)
     */
    double calculatePopulationDiversity(const std::vector<EvolutionaryProgram>& population);

    /**
     * Tournament selection
     * @param population Population to select from
     * @param tournament_size Size of tournament
     * @return Selected program
     */
    EvolutionaryProgram tournamentSelection(const std::vector<EvolutionaryProgram>& population,
                                          int tournament_size);

    /**
     * Update evolution statistics
     */
    void updateStats(const std::string& metric, double value);

    /**
     * Get subtree from atom at specific position
     * @param atom Source atom
     * @param position Position in tree
     * @return Subtree handle
     */
    Handle getSubtree(const Handle& atom, int position);

    /**
     * Replace subtree in atom
     * @param atom Source atom
     * @param position Position to replace
     * @param replacement New subtree
     * @return Modified atom
     */
    Handle replaceSubtree(const Handle& atom, int position, const Handle& replacement);

private:
    // Prevent copying
    AtomSpaceEvolver(const AtomSpaceEvolver&) = delete;
    AtomSpaceEvolver& operator=(const AtomSpaceEvolver&) = delete;
};

} // namespace agentzero
} // namespace opencog

#endif // _AGENTZERO_ATOMSPACE_EVOLVER_H