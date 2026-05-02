/**
 * PolicyOptimizer.h - MOSES-based Policy Optimization for Agent-Zero
 * 
 * Part of AZ-LEARN-003: MOSES Policy Optimization Integration
 * Implements core policy evolution using MOSES genetic programming
 * 
 * Copyright (C) 2024 OpenCog Foundation
 */

#ifndef AGENTZERO_POLICY_OPTIMIZER_H
#define AGENTZERO_POLICY_OPTIMIZER_H

#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "LearningTypes.h"

// MOSES core headers
#include <moses/moses/moses/moses_main.h>
#include <moses/moses/moses/metapopulation/metapopulation.h>
#include <moses/moses/moses/deme/deme_expander.h>
#include <moses/moses/moses/scoring/behave_cscore.h>

// ASMOSES AtomSpace integration
#include <opencog/asmoses/moses/main/moses_exec.h>
#include <opencog/asmoses/atomese/atomese_utils/constants.h>

namespace opencog {
namespace agentzero {
namespace learning {

/**
 * PolicyOptimizer - Main class for MOSES-based policy optimization
 * 
 * This class integrates MOSES evolutionary programming with the AtomSpace
 * to evolve policies for Agent-Zero. It provides:
 * - MOSES-based genetic programming for policy evolution
 * - AtomSpace integration for storing and retrieving policies
 * - Fitness function customization for domain-specific optimization
 * - Real-time policy evaluation and improvement
 */
class PolicyOptimizer {
public:
    /**
     * Constructor
     * @param atomspace Shared pointer to the AtomSpace
     * @param config Learning configuration parameters
     */
    explicit PolicyOptimizer(AtomSpacePtr atomspace, 
                           const LearningConfig& config = LearningConfig{});
    
    /**
     * Destructor - ensures proper cleanup of MOSES resources
     */
    ~PolicyOptimizer();
    
    // Disable copy construction and assignment
    PolicyOptimizer(const PolicyOptimizer&) = delete;
    PolicyOptimizer& operator=(const PolicyOptimizer&) = delete;
    
    /**
     * Initialize the policy optimizer with a fitness function
     * @param fitness_function Function to evaluate policy fitness
     * @return true if successful, false otherwise
     */
    bool initialize(std::shared_ptr<PolicyFitnessFunction> fitness_function);
    
    /**
     * Evolve a new policy using MOSES genetic programming
     * @param policy_id Unique identifier for the policy
     * @param initial_program Optional initial program to seed evolution
     * @param max_evaluations Maximum number of fitness evaluations
     * @return Shared pointer to the evolved policy, or nullptr if failed
     */
    std::shared_ptr<Policy> evolvePolicy(const PolicyId& policy_id,
                                       const opencog::combo::combo_tree* initial_program = nullptr,
                                       size_t max_evaluations = 0);
    
    /**
     * Improve an existing policy through additional evolution
     * @param policy The policy to improve
     * @param max_evaluations Maximum additional evaluations
     * @return true if improvement was successful
     */
    bool improvePolicy(std::shared_ptr<Policy> policy, size_t max_evaluations = 0);
    
    /**
     * Evaluate a policy's fitness using the current fitness function
     * @param policy The policy to evaluate
     * @param context Optional context for evaluation
     * @return Fitness score
     */
    double evaluatePolicy(const Policy& policy,
                         const std::map<std::string, Handle>& context = {});
    
    /**
     * Store a policy in the AtomSpace
     * @param policy The policy to store
     * @return AtomSpace handle to the policy atom
     */
    Handle storePolicyInAtomSpace(const Policy& policy);
    
    /**
     * Retrieve a policy from the AtomSpace
     * @param policy_id The policy identifier
     * @return Shared pointer to the policy, or nullptr if not found
     */
    std::shared_ptr<Policy> retrievePolicyFromAtomSpace(const PolicyId& policy_id);
    
    /**
     * Get all policies currently stored in the AtomSpace
     * @return Vector of policy pointers
     */
    std::vector<std::shared_ptr<Policy>> getAllPolicies();
    
    /**
     * Set the fitness function for policy evaluation
     * @param fitness_function New fitness function
     */
    void setFitnessFunction(std::shared_ptr<PolicyFitnessFunction> fitness_function);
    
    /**
     * Get the current fitness function
     * @return Shared pointer to the current fitness function
     */
    std::shared_ptr<PolicyFitnessFunction> getFitnessFunction() const;
    
    /**
     * Start continuous policy optimization in background thread
     * @param callback Optional callback for optimization events
     * @return true if started successfully
     */
    bool startContinuousOptimization(std::shared_ptr<LearningEventCallback> callback = nullptr);
    
    /**
     * Stop continuous policy optimization
     */
    void stopContinuousOptimization();
    
    /**
     * Check if continuous optimization is running
     * @return true if running
     */
    bool isContinuousOptimizationRunning() const;
    
    /**
     * Update configuration parameters
     * @param config New configuration
     */
    void updateConfig(const LearningConfig& config);
    
    /**
     * Get current configuration
     * @return Current configuration
     */
    const LearningConfig& getConfig() const;
    
    /**
     * Get optimization statistics
     * @return Map of statistic names to values
     */
    std::map<std::string, double> getOptimizationStats() const;
    
    /**
     * Reset all optimization statistics
     */
    void resetStats();

private:
    // Core members
    AtomSpacePtr atomspace_;
    LearningConfig config_;
    std::shared_ptr<PolicyFitnessFunction> fitness_function_;
    
    // MOSES integration
    std::unique_ptr<opencog::moses::moses_parameters> moses_params_;
    std::unique_ptr<opencog::moses::metapop_parameters> metapop_params_;
    std::unique_ptr<opencog::moses::deme_parameters> deme_params_;
    std::unique_ptr<opencog::moses::optim_parameters> optim_params_;
    std::unique_ptr<opencog::moses::hc_parameters> hc_params_;
    
    // Policy storage
    std::map<PolicyId, std::shared_ptr<Policy>> policy_cache_;
    std::mutex policy_cache_mutex_;
    
    // Continuous optimization
    std::unique_ptr<std::thread> optimization_thread_;
    std::atomic<bool> optimization_running_;
    std::mutex optimization_mutex_;
    std::condition_variable optimization_cv_;
    std::shared_ptr<LearningEventCallback> optimization_callback_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    size_t total_evaluations_;
    size_t total_generations_;
    double best_fitness_ever_;
    double average_fitness_;
    std::chrono::time_point<std::chrono::steady_clock> optimization_start_time_;
    
    // Private methods
    
    /**
     * Initialize MOSES parameters from configuration
     */
    void initializeMOSESParameters();
    
    /**
     * Create a MOSES fitness evaluator from our fitness function
     */
    std::unique_ptr<opencog::moses::behave_cscore> createMOSESEvaluator();
    
    /**
     * Convert combo tree to Policy object
     */
    std::shared_ptr<Policy> comboTreeToPolicy(const PolicyId& policy_id,
                                             const opencog::combo::combo_tree& tree,
                                             double fitness);
    
    /**
     * Convert Policy to AtomSpace representation
     */
    Handle policyToAtomSpaceRepresentation(const Policy& policy);
    
    /**
     * Convert AtomSpace representation back to Policy
     */
    std::shared_ptr<Policy> atomSpaceRepresentationToPolicy(Handle policy_atom);
    
    /**
     * Main continuous optimization loop
     */
    void continuousOptimizationLoop();
    
    /**
     * Update optimization statistics
     */
    void updateStats(double fitness, size_t evaluations, size_t generations);
    
    /**
     * Validate policy for basic consistency
     */
    bool validatePolicy(const Policy& policy) const;
    
    /**
     * Create default AtomSpace nodes for policy storage
     */
    void initializeAtomSpaceStructures();
    
    /**
     * Clean up old policies based on performance and age
     */
    void cleanupOldPolicies();
    
    /**
     * Generate unique policy ID if not provided
     */
    PolicyId generatePolicyId() const;
    
    /**
     * Log policy evolution progress
     */
    void logEvolutionProgress(const PolicyId& policy_id, 
                            size_t generation, 
                            double best_fitness,
                            double avg_fitness) const;
};

/**
 * Factory function to create a PolicyOptimizer with common configurations
 */
std::unique_ptr<PolicyOptimizer> createPolicyOptimizer(
    AtomSpacePtr atomspace,
    std::shared_ptr<PolicyFitnessFunction> fitness_function,
    const std::string& config_preset = "default");

} // namespace learning
} // namespace agentzero
} // namespace opencog

#endif // AGENTZERO_POLICY_OPTIMIZER_H