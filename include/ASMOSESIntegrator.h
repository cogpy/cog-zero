/**
 * ASMOSESIntegrator.h
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

#ifndef _AGENTZERO_ASMOSES_INTEGRATOR_H
#define _AGENTZERO_ASMOSES_INTEGRATOR_H

#include <memory>
#include <vector>
#include <string>
#include <functional>

#include <opencog/util/Logger.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

// Forward declarations for MOSES types
namespace opencog { namespace moses {
    class problem_params;
    class metapopulation;
    class scored_combo_tree;
} }

namespace opencog { namespace agentzero {

class AtomSpaceEvolver;
class ExperienceManager;
class PolicyOptimizer;

/**
 * ASMOSESIntegrator - Main integration class for AtomSpace evolution
 * 
 * This class provides the primary interface between Agent-Zero and the
 * ASMOSES (AtomSpace MOSES) evolutionary system. It enables:
 * 
 * 1. Evolution of Atomese programs directly in the AtomSpace
 * 2. Policy optimization through evolutionary algorithms
 * 3. Experience-driven learning and adaptation
 * 4. Integration with OpenCog's cognitive architecture
 */
class ASMOSESIntegrator
{
public:
    /**
     * Constructor
     * @param atomspace The AtomSpace to operate on
     * @param agent_name Name of the agent for logging/identification
     */
    explicit ASMOSESIntegrator(AtomSpacePtr atomspace, 
                              const std::string& agent_name = "agent-zero");

    /**
     * Destructor
     */
    virtual ~ASMOSESIntegrator();

    /**
     * Initialize the ASMOSES integration system
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * Shutdown the integration system
     */
    void shutdown();

    /**
     * Check if the system is initialized
     */
    bool isInitialized() const { return _initialized; }

    /**
     * Evolve a program to solve a specific problem
     * @param problem_atom Handle to atom describing the problem
     * @param fitness_function Evaluation function for candidate solutions
     * @param max_generations Maximum number of evolutionary generations
     * @return Handle to best evolved program, or Handle::UNDEFINED on failure
     */
    Handle evolveProgram(const Handle& problem_atom,
                        std::function<double(const Handle&)> fitness_function,
                        int max_generations = 100);

    /**
     * Evolve a program using AtomSpace-based problem specification
     * @param problem_spec AtomSpace representation of the problem
     * @param target_atom Goal/target atom to optimize towards
     * @param options Evolution parameters
     * @return Handle to best evolved program
     */
    Handle evolveAtomspaceProgram(const Handle& problem_spec,
                                 const Handle& target_atom,
                                 const std::map<std::string, std::string>& options = {});

    /**
     * Optimize a policy through evolutionary learning
     * @param policy_atom Current policy representation
     * @param experience_data Historical experience data
     * @param reward_function Function to evaluate policy performance
     * @return Improved policy atom
     */
    Handle optimizePolicy(const Handle& policy_atom,
                         const std::vector<Handle>& experience_data,
                         std::function<double(const Handle&, const std::vector<Handle>&)> reward_function);

    /**
     * Learn new skills from experience data
     * @param experience_atoms Vector of experience atoms
     * @param skill_template Template for skill structure
     * @return Vector of learned skill atoms
     */
    std::vector<Handle> learnSkills(const std::vector<Handle>& experience_atoms,
                                   const Handle& skill_template = Handle::UNDEFINED);

    /**
     * Get the AtomSpace evolver component
     */
    std::shared_ptr<AtomSpaceEvolver> getEvolver() const { return _evolver; }

    /**
     * Get the experience manager component
     */
    std::shared_ptr<ExperienceManager> getExperienceManager() const { return _experience_manager; }

    /**
     * Get the policy optimizer component
     */
    std::shared_ptr<PolicyOptimizer> getPolicyOptimizer() const { return _policy_optimizer; }

    /**
     * Set evolution parameters
     * @param params Map of parameter names to values
     */
    void setEvolutionParams(const std::map<std::string, std::string>& params);

    /**
     * Get current evolution parameters
     */
    std::map<std::string, std::string> getEvolutionParams() const;

    /**
     * Enable/disable logging
     */
    void setLogging(bool enabled) { _logging_enabled = enabled; }

    /**
     * Get statistics about evolution runs
     */
    std::map<std::string, int> getStatistics() const;

    /**
     * Reset statistics
     */
    void resetStatistics();

protected:
    // Core components
    AtomSpacePtr _atomspace;
    std::shared_ptr<AtomSpaceEvolver> _evolver;
    std::shared_ptr<ExperienceManager> _experience_manager;
    std::shared_ptr<PolicyOptimizer> _policy_optimizer;

    // Configuration
    std::string _agent_name;
    std::map<std::string, std::string> _evolution_params;
    bool _initialized;
    bool _logging_enabled;

    // Statistics
    mutable std::map<std::string, int> _statistics;

    /**
     * Initialize core components
     */
    bool initializeComponents();

    /**
     * Validate AtomSpace connection
     */
    bool validateAtomSpace();

    /**
     * Setup default evolution parameters
     */
    void setupDefaultParams();

    /**
     * Log evolution progress
     */
    void logEvolutionProgress(const std::string& stage, 
                            const std::string& details = "");

    /**
     * Convert MOSES results to AtomSpace representation
     */
    Handle convertMOSESResult(const opencog::moses::scored_combo_tree& result);

    /**
     * Update statistics
     */
    void updateStatistics(const std::string& metric, int delta = 1);

private:
    // Prevent copying
    ASMOSESIntegrator(const ASMOSESIntegrator&) = delete;
    ASMOSESIntegrator& operator=(const ASMOSESIntegrator&) = delete;
};

} // namespace agentzero
} // namespace opencog

#endif // _AGENTZERO_ASMOSES_INTEGRATOR_H