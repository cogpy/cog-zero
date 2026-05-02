/*
 * opencog/agentzero/PlanningEngine.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Planning Engine with Temporal Reasoning
 * Hierarchical planning and goal decomposition system for Agent-Zero
 * Part of AZ-PLAN-002: Create PlanningEngine with temporal reasoning
 */

#ifndef _OPENCOG_AGENTZERO_PLANNING_ENGINE_H
#define _OPENCOG_AGENTZERO_PLANNING_ENGINE_H

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <functional>
#include <queue>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

#ifdef HAVE_SPACETIME
#include <opencog/spacetime/TimeServer.h>
#include <opencog/spacetime/SpaceServer.h>
#endif

namespace opencog {
namespace agentzero {

class GoalHierarchy;
class TemporalReasoner;

/**
 * PlanningEngine - Hierarchical planning with temporal reasoning
 *
 * This class provides comprehensive planning capabilities with temporal
 * reasoning, goal decomposition, and AtomSpace integration. It generates
 * and executes action sequences with timing constraints and dependencies.
 *
 * Key features:
 * - Hierarchical goal decomposition
 * - Temporal planning with spacetime integration
 * - Action sequence generation and optimization
 * - Real-time plan adaptation and re-planning
 * - AtomSpace integration for knowledge representation
 * - Performance optimized for < 100ms routine decisions
 */
class PlanningEngine
{
public:
    // Planning result codes
    enum class PlanResult {
        SUCCESS,           // Plan generated successfully
        NO_SOLUTION,       // No valid plan found
        TIMEOUT,          // Planning timeout exceeded
        GOAL_INVALID,     // Goal specification is invalid
        RESOURCES_UNAVAILABLE, // Required resources not available
        TEMPORAL_CONFLICT, // Temporal constraints cannot be satisfied
        MEMORY_LIMIT      // Memory usage limit exceeded
    };
    
    // Plan execution status
    enum class ExecutionStatus {
        NOT_STARTED,      // Plan has not begun execution
        EXECUTING,        // Plan is currently executing
        COMPLETED,        // Plan completed successfully
        FAILED,           // Plan execution failed
        CANCELLED,        // Plan was cancelled
        REPLANNING       // Plan is being adapted/replanned
    };
    
    // Planning strategy types
    enum class PlanningStrategy {
        HIERARCHICAL,     // Top-down hierarchical decomposition
        FORWARD_SEARCH,   // Forward state-space search
        BACKWARD_SEARCH,  // Goal-directed backward search
        HYBRID,           // Combination of strategies
        TEMPORAL_FIRST,   // Temporal constraints prioritized
        RESOURCE_OPTIMAL  // Resource optimization prioritized
    };

    // Plan representation
    struct Plan {
        Handle plan_atom;                    // AtomSpace representation
        Handle goal_atom;                    // Target goal
        std::vector<Handle> action_sequence; // Ordered actions
        std::vector<Handle> preconditions;   // Required preconditions
        std::vector<Handle> effects;         // Expected effects
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;
        std::chrono::milliseconds duration;
        ExecutionStatus status;
        float confidence;                    // Plan success confidence
        int revision_count;                  // Number of adaptations
        
        Plan() : duration(0), status(ExecutionStatus::NOT_STARTED), 
                confidence(0.5f), revision_count(0) {}
    };

    /**
     * Plan quality metrics for MetaPlanner integration.
     *
     * These metrics characterise the quality of a generated plan and are
     * consumed by the MetaPlanner to drive self-optimisation of planning
     * strategies over time.
     */
    struct PlanQualityMetrics {
        float optimality_score{0.0f};        // How close to optimal (0–1)
        float goal_coverage{0.0f};           // Fraction of goal effects covered (0–1)
        float temporal_satisfaction{0.0f};   // Fraction of temporal constraints met (0–1)
        float resource_efficiency{0.0f};     // Resource usage efficiency (0–1)
        float robustness_score{0.0f};        // Resilience to perturbations (0–1)
        int   action_count{0};               // Number of actions in the plan
        float average_action_confidence{0.0f}; // Mean confidence across actions
        std::chrono::milliseconds planning_time{0}; // Wall-clock time to generate plan

        /** Compute a single scalar quality score (0–1). */
        float overallScore() const {
            return (optimality_score + goal_coverage + temporal_satisfaction
                    + resource_efficiency + robustness_score) / 5.0f;
        }

        void reset() {
            optimality_score = goal_coverage = temporal_satisfaction = 0.0f;
            resource_efficiency = robustness_score = average_action_confidence = 0.0f;
            action_count = 0;
            planning_time = std::chrono::milliseconds{0};
        }
    };

private:
    // Core components
    AtomSpacePtr _atomspace;
    std::shared_ptr<GoalHierarchy> _goal_hierarchy;
    std::shared_ptr<TemporalReasoner> _temporal_reasoner;
    
#ifdef HAVE_SPACETIME
    spacetime::TimeServer* _time_server;
    spacetime::SpaceServer* _space_server;
#endif

    // Planning state
    std::map<Handle, Plan> _active_plans;
    std::map<Handle, std::vector<Handle>> _goal_dependencies;
    std::queue<Handle> _planning_queue;
    std::vector<Handle> _resource_atoms;
    
    // AtomSpace contexts
    Handle _planning_context;
    Handle _temporal_context;
    Handle _goal_context;
    Handle _action_context;
    
    // Configuration parameters
    std::chrono::milliseconds _planning_timeout;
    int _max_plan_depth;
    int _max_actions_per_plan;
    float _min_confidence_threshold;
    PlanningStrategy _default_strategy;
    bool _enable_replanning;
    bool _enable_temporal_optimization;
    
    // Performance monitoring
    std::chrono::steady_clock::time_point _last_planning_time;
    std::chrono::milliseconds _average_planning_time;
    int _plans_generated;
    int _plans_successful;
    
    // Internal methods
    void initializePlanningEngine();
    void createPlanningContexts();
    bool validateGoal(const Handle& goal_atom);
    PlanResult generatePlan(const Handle& goal_atom, Plan& result_plan);
    PlanResult hierarchicalPlanning(const Handle& goal_atom, Plan& plan);
    PlanResult forwardSearchPlanning(const Handle& goal_atom, Plan& plan);
    PlanResult temporalPlanning(const Handle& goal_atom, Plan& plan);
    std::vector<Handle> decomposeGoal(const Handle& goal_atom);
    std::vector<Handle> findApplicableActions(const Handle& goal_atom);
    bool checkPreconditions(const Handle& action_atom);
    bool checkTemporalConstraints(const Plan& plan);
    bool checkResourceConstraints(const Plan& plan);
    void optimizePlan(Plan& plan);
    Handle createPlanAtom(const Plan& plan);
    void recordPlanningMetrics(const Plan& plan, 
                              std::chrono::milliseconds planning_time);

public:
    /**
     * Constructor
     * @param atomspace Shared pointer to the AtomSpace
     */
    PlanningEngine(AtomSpacePtr atomspace);
    
    /**
     * Destructor - ensures cleanup of resources
     */
    ~PlanningEngine();
    
    // Core planning interface
    /**
     * Generate a plan for the specified goal
     * @param goal_atom Handle to the goal atom
     * @param strategy Planning strategy to use
     * @return PlanResult indicating success or failure
     */
    PlanResult createPlan(const Handle& goal_atom, 
                         PlanningStrategy strategy = PlanningStrategy::HYBRID);
    
    /**
     * Generate a plan with temporal constraints
     * @param goal_atom Handle to the goal atom
     * @param deadline When the goal must be achieved
     * @param strategy Planning strategy to use
     * @return PlanResult indicating success or failure
     */
    PlanResult createTemporalPlan(const Handle& goal_atom,
                                 const std::chrono::steady_clock::time_point& deadline,
                                 PlanningStrategy strategy = PlanningStrategy::TEMPORAL_FIRST);
    
    /**
     * Generate a plan with resource constraints
     * @param goal_atom Handle to the goal atom
     * @param available_resources Vector of available resource atoms
     * @param strategy Planning strategy to use
     * @return PlanResult indicating success or failure
     */
    PlanResult createResourceConstrainedPlan(const Handle& goal_atom,
                                           const std::vector<Handle>& available_resources,
                                           PlanningStrategy strategy = PlanningStrategy::RESOURCE_OPTIMAL);
    
    /**
     * Adapt an existing plan based on changed conditions
     * @param plan_atom Handle to the plan atom
     * @return PlanResult indicating success or failure
     */
    PlanResult adaptPlan(const Handle& plan_atom);
    
    /**
     * Cancel an active plan
     * @param plan_atom Handle to the plan atom
     * @return true if plan was cancelled successfully
     */
    bool cancelPlan(const Handle& plan_atom);
    
    // Plan access and monitoring
    /**
     * Get plan by goal atom
     * @param goal_atom Handle to the goal atom
     * @return Plan structure or nullptr if not found
     */
    const Plan* getPlan(const Handle& goal_atom) const;
    
    /**
     * Get all active plans
     * @return vector of active Plan structures
     */
    std::vector<Plan> getActivePlans() const;
    
    /**
     * Get plan execution status
     * @param plan_atom Handle to the plan atom
     * @return ExecutionStatus of the plan
     */
    ExecutionStatus getPlanStatus(const Handle& plan_atom) const;
    
    /**
     * Get next action in plan
     * @param plan_atom Handle to the plan atom
     * @return Handle to next action or Handle::UNDEFINED
     */
    Handle getNextAction(const Handle& plan_atom) const;
    
    /**
     * Mark action as completed in plan
     * @param plan_atom Handle to the plan atom
     * @param action_atom Handle to completed action
     * @return true if action was marked completed
     */
    bool markActionCompleted(const Handle& plan_atom, const Handle& action_atom);
    
    // Goal hierarchy integration
    /**
     * Set goal hierarchy manager
     * @param hierarchy Shared pointer to GoalHierarchy instance
     */
    void setGoalHierarchy(std::shared_ptr<GoalHierarchy> hierarchy) {
        _goal_hierarchy = hierarchy;
    }
    
    /**
     * Get goal hierarchy manager
     * @return pointer to GoalHierarchy instance
     */
    GoalHierarchy* getGoalHierarchy() const { return _goal_hierarchy.get(); }
    
    // Temporal reasoning integration
#ifdef HAVE_SPACETIME
    /**
     * Set spacetime servers for temporal reasoning
     * @param time_server Pointer to TimeServer instance
     * @param space_server Pointer to SpaceServer instance
     */
    void setSpacetimeServers(spacetime::TimeServer* time_server,
                           spacetime::SpaceServer* space_server = nullptr) {
        _time_server = time_server;
        _space_server = space_server;
    }
#endif
    
    /**
     * Update temporal reasoning with current time
     * @return number of plans affected by temporal updates
     */
    int updateTemporalReasoning();
    
    // Configuration
    /**
     * Set planning timeout
     * @param timeout_ms Maximum planning time in milliseconds
     */
    void setPlanningTimeout(int timeout_ms) {
        _planning_timeout = std::chrono::milliseconds(timeout_ms);
    }
    
    /**
     * Set maximum plan depth for hierarchical planning
     * @param max_depth Maximum recursion depth
     */
    void setMaxPlanDepth(int max_depth) {
        _max_plan_depth = max_depth;
    }
    
    /**
     * Set maximum actions per plan
     * @param max_actions Maximum number of actions
     */
    void setMaxActionsPerPlan(int max_actions) {
        _max_actions_per_plan = max_actions;
    }
    
    /**
     * Set minimum confidence threshold for plans
     * @param threshold Minimum confidence (0.0 - 1.0)
     */
    void setMinConfidenceThreshold(float threshold) {
        _min_confidence_threshold = threshold;
    }
    
    /**
     * Configure planning features
     * @param replanning Enable automatic replanning
     * @param temporal_opt Enable temporal optimization
     */
    void configureFeatures(bool replanning, bool temporal_opt) {
        _enable_replanning = replanning;
        _enable_temporal_optimization = temporal_opt;
    }
    
    // AtomSpace integration
    /**
     * Get the planning context atom
     * @return Handle to planning context
     */
    Handle getPlanningContext() const { return _planning_context; }
    
    /**
     * Get the temporal context atom
     * @return Handle to temporal context
     */
    Handle getTemporalContext() const { return _temporal_context; }
    
    // Performance monitoring
    /**
     * Get planning performance statistics
     * @return JSON string with performance metrics
     */
    std::string getPerformanceStats() const;
    
    /**
     * Get average planning time
     * @return average planning time in milliseconds
     */
    std::chrono::milliseconds getAveragePlanningTime() const {
        return _average_planning_time;
    }
    
    /**
     * Get planning success rate
     * @return success rate as percentage (0.0 - 100.0)
     */
    float getPlanningSuccessRate() const;
    
    /**
     * Compute quality metrics for a given plan.
     *
     * This is the primary hook for MetaPlanner self-optimisation: after
     * every planning cycle the caller should invoke this method and feed
     * the result to MetaPlanner::recordPlanningEpisode() so that strategy
     * selection improves over time.
     *
     * @param plan The plan whose quality should be assessed
     * @return Populated PlanQualityMetrics structure
     */
    PlanQualityMetrics computePlanQuality(const Plan& plan) const;

    /**
     * Reset performance statistics
     */
    void resetPerformanceStats();
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_PLANNING_ENGINE_H