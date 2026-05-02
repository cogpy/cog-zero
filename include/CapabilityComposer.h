/*
 * opencog/agentzero/tools/CapabilityComposer.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * CapabilityComposer Implementation
 * Combines tools and capabilities for complex task execution
 * Part of the AGENT-ZERO-GENESIS project - Phase 8: Tool Integration
 * Task ID: AZ-TOOL-003
 */

#ifndef _OPENCOG_AGENTZERO_TOOLS_CAPABILITY_COMPOSER_H
#define _OPENCOG_AGENTZERO_TOOLS_CAPABILITY_COMPOSER_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <mutex>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {
namespace tools {

/**
 * CapabilityComposer - Combines tools for complex task execution
 *
 * This class provides advanced capability composition, enabling the agent
 * to combine multiple tools and capabilities to accomplish complex tasks
 * that require coordinated execution of multiple simpler capabilities.
 *
 * Key features:
 * - Tool capability registration and management
 * - Automatic composition planning for complex tasks
 * - AtomSpace integration for capability knowledge representation
 * - Dependency resolution between capabilities
 * - Execution flow coordination
 * - Resource allocation and optimization
 * - Capability caching and reuse
 * - Integration with external-tools and ros-behavior-scripting
 */
class CapabilityComposer
{
public:
    // Forward declarations
    struct Capability;
    struct CompositionPlan;
    struct ExecutionContext;
    
    // Capability function signature
    using CapabilityFunction = std::function<bool(const ExecutionContext&)>;
    
    // Capability result type
    enum class CapabilityResult {
        SUCCESS,
        FAILURE,
        PARTIAL_SUCCESS,
        REQUIRES_RETRY,
        MISSING_DEPENDENCY
    };
    
    // Capability structure representing a single tool/capability
    struct Capability {
        std::string capability_id;
        std::string name;
        std::string description;
        std::vector<std::string> required_capabilities;  // Dependencies
        std::vector<std::string> provided_capabilities;  // What this provides
        std::map<std::string, std::string> parameters;
        CapabilityFunction execute;
        Handle capability_atom;                          // AtomSpace representation
        double success_rate;
        double average_execution_time;
        int execution_count;
        bool is_atomic;                                  // Can't be decomposed further
        
        Capability() : success_rate(1.0), average_execution_time(0.0), 
                      execution_count(0), is_atomic(true) {}
    };
    
    // Composition plan for complex task execution
    struct CompositionPlan {
        std::string plan_id;
        std::string goal_description;
        std::vector<std::string> capability_sequence;    // Ordered execution sequence
        std::map<std::string, std::vector<std::string>> dependency_graph;
        std::vector<std::string> parallel_groups;        // Groups that can run in parallel
        Handle plan_atom;                                // AtomSpace representation
        double estimated_success_probability;
        double estimated_execution_time;
        bool is_valid;
        
        CompositionPlan() : estimated_success_probability(0.0), 
                           estimated_execution_time(0.0), is_valid(false) {}
    };
    
    // Execution context for capability execution
    struct ExecutionContext {
        std::string context_id;
        std::string current_capability;
        std::map<std::string, std::string> input_parameters;
        std::map<std::string, std::string> output_results;
        std::map<std::string, Handle> context_atoms;
        std::vector<std::string> execution_log;
        AtomSpacePtr atomspace;
        double resource_allocation;
        bool allow_parallel_execution;
        
        ExecutionContext() : resource_allocation(1.0), allow_parallel_execution(false) {}
    };
    
    // Task requirements for composition planning
    struct TaskRequirements {
        std::string task_description;
        std::vector<std::string> required_outputs;
        std::vector<std::string> available_inputs;
        std::map<std::string, std::string> constraints;
        double max_execution_time;
        double min_success_probability;
        
        TaskRequirements() : max_execution_time(-1.0), min_success_probability(0.5) {}
    };

private:
    // Core references
    AtomSpacePtr _atomspace;
    
    // Capability registry
    std::map<std::string, Capability> _capabilities;
    std::map<std::string, std::set<std::string>> _capability_providers;  // What provides what
    std::map<std::string, std::vector<std::string>> _capability_dependencies;
    
    // Composition plans
    std::map<std::string, CompositionPlan> _plans;
    std::map<std::string, std::vector<std::string>> _task_to_plans;  // Task -> applicable plans
    
    // AtomSpace handles for organization
    Handle _capability_registry_base;
    Handle _composition_plans_node;
    Handle _execution_history_node;
    
    // Configuration
    size_t _max_cached_plans;
    double _composition_timeout_seconds;
    bool _enable_automatic_composition;
    bool _enable_parallel_execution;
    size_t _max_composition_depth;
    
    // Thread safety
    mutable std::mutex _capabilities_mutex;
    mutable std::mutex _plans_mutex;
    
    // Internal methods
    void initializeCapabilityStructures();
    Handle createCapabilityAtom(const Capability& capability);
    Handle createCompositionPlanAtom(const CompositionPlan& plan);
    
    // Composition planning algorithms
    CompositionPlan planComposition(const TaskRequirements& requirements);
    bool validateComposition(const CompositionPlan& plan);
    double estimatePlanSuccess(const CompositionPlan& plan);
    double estimatePlanExecutionTime(const CompositionPlan& plan);
    std::vector<std::vector<std::string>> findParallelGroups(const CompositionPlan& plan);
    
    // Dependency resolution
    std::vector<std::string> resolveDependencies(const std::vector<std::string>& capabilities);
    bool checkDependencySatisfaction(const std::string& capability_id,
                                    const std::set<std::string>& available);
    std::vector<std::string> topologicalSort(const std::map<std::string, std::vector<std::string>>& deps);
    
    // Execution management
    CapabilityResult executeCapability(const std::string& capability_id,
                                      ExecutionContext& context);
    void updateCapabilityStatistics(const std::string& capability_id,
                                   CapabilityResult result,
                                   double execution_time);
    void logExecution(const std::string& capability_id,
                     CapabilityResult result,
                     const ExecutionContext& context);
    
    // Utility methods
    std::string generatePlanId() const;
    std::string generateContextId() const;

public:
    /**
     * Constructor
     * @param atomspace Shared pointer to the AtomSpace
     */
    CapabilityComposer(AtomSpacePtr atomspace);
    
    /**
     * Destructor - cleans up capability composer resources
     */
    ~CapabilityComposer();
    
    /**
     * Register a new capability
     * @param capability Capability to register
     * @return True if registration successful
     */
    bool registerCapability(const Capability& capability);
    
    /**
     * Register a simple capability by parameters
     * @param capability_id Unique identifier for the capability
     * @param name Human-readable name
     * @param description Description of what the capability does
     * @param execute Function to execute the capability
     * @param required_capabilities Dependencies this capability requires
     * @return True if registration successful
     */
    bool registerCapability(const std::string& capability_id,
                          const std::string& name,
                          const std::string& description,
                          CapabilityFunction execute,
                          const std::vector<std::string>& required_capabilities = {});
    
    /**
     * Unregister a capability
     * @param capability_id ID of capability to remove
     * @return True if successfully unregistered
     */
    bool unregisterCapability(const std::string& capability_id);
    
    /**
     * Get registered capability by ID
     * @param capability_id ID of the capability
     * @return Pointer to capability if found, nullptr otherwise
     */
    const Capability* getCapability(const std::string& capability_id) const;
    
    /**
     * Find capabilities that provide a specific output
     * @param output_type Type of output needed
     * @return Vector of capability IDs that provide this output
     */
    std::vector<std::string> findCapabilitiesProviding(const std::string& output_type);
    
    /**
     * Compose a plan to accomplish a task
     * @param requirements Task requirements and constraints
     * @return Composition plan if successful, invalid plan otherwise
     */
    CompositionPlan composeForTask(const TaskRequirements& requirements);
    
    /**
     * Execute a composition plan
     * @param plan Plan to execute
     * @param context Execution context (modified during execution)
     * @return Overall result of plan execution
     */
    CapabilityResult executePlan(const CompositionPlan& plan, ExecutionContext& context);
    
    /**
     * Compose and execute in one step
     * @param requirements Task requirements
     * @param context Execution context
     * @return Result of execution
     */
    CapabilityResult composeAndExecute(const TaskRequirements& requirements,
                                      ExecutionContext& context);
    
    /**
     * Get statistics about capabilities
     * @return Map of statistics
     */
    std::map<std::string, double> getCapabilityStatistics() const;
    
    /**
     * Get all registered capability IDs
     * @return Vector of capability IDs
     */
    std::vector<std::string> getRegisteredCapabilities() const;
    
    /**
     * Check if a capability is registered
     * @param capability_id ID to check
     * @return True if registered
     */
    bool isCapabilityRegistered(const std::string& capability_id) const;
    
    /**
     * Validate dependencies for a capability
     * @param capability_id ID of capability to validate
     * @return True if all dependencies are satisfied
     */
    bool validateCapabilityDependencies(const std::string& capability_id) const;
    
    /**
     * Get dependency tree for a capability
     * @param capability_id ID of the capability
     * @param max_depth Maximum depth to traverse
     * @return Map representing dependency tree
     */
    std::map<std::string, std::vector<std::string>> getCapabilityDependencyTree(
        const std::string& capability_id, size_t max_depth = 10) const;
    
    /**
     * Export capabilities and plans for analysis
     * @param format Export format ("json", "dot", "atomese")
     * @return Exported data as string
     */
    std::string exportCapabilities(const std::string& format = "json") const;
    
    /**
     * Clear cached composition plans
     * @return Number of plans cleared
     */
    size_t clearCachedPlans();
    
    // Configuration methods
    void setMaxCachedPlans(size_t max_plans) { _max_cached_plans = max_plans; }
    void setCompositionTimeout(double timeout_seconds) { _composition_timeout_seconds = timeout_seconds; }
    void enableAutomaticComposition(bool enable) { _enable_automatic_composition = enable; }
    void enableParallelExecution(bool enable) { _enable_parallel_execution = enable; }
    void setMaxCompositionDepth(size_t max_depth) { _max_composition_depth = max_depth; }
    
    // Getters
    size_t getCapabilityCount() const;
    size_t getPlanCount() const;
    Handle getCapabilityRegistryBase() const { return _capability_registry_base; }
};

} // namespace tools
} // namespace agentzero  
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_TOOLS_CAPABILITY_COMPOSER_H
