/*
 * opencog/agentzero/MetaPlanner.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * MetaPlanner for Self-Optimization
 * Self-reflective planning optimization system for Agent-Zero
 * Part of AZ-PLAN-003: Implement MetaPlanner for self-optimization
 */

#ifndef _OPENCOG_AGENTZERO_META_PLANNER_H
#define _OPENCOG_AGENTZERO_META_PLANNER_H

#include <memory>
#include <vector>
#include <queue>
#include <string>
#include <map>
#include <chrono>
#include <functional>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {

class AgentZeroCore;
class TaskManager; 
class ActionScheduler;

/**
 * MetaPlanner - Self-reflective planning optimization system
 *
 * This class provides meta-cognitive capabilities for analyzing and
 * optimizing the planning process itself. It uses AtomSpace for
 * representing planning strategies, performance metrics, and learned
 * optimization patterns.
 *
 * Key features:
 * - Planning strategy analysis and optimization
 * - Performance metric tracking and learning
 * - Self-reflective planning improvement
 * - Integration with spacetime for temporal planning
 * - PLN-based reasoning about planning effectiveness
 */
class MetaPlanner
{
public:
    // Planning strategy types
    enum class PlanningStrategy {
        GREEDY,              // Simple greedy planning
        HIERARCHICAL,        // Hierarchical task network planning
        TEMPORAL,            // Temporal reasoning based planning
        ADAPTIVE,            // Adaptive strategy selection
        LEARNING_BASED,      // Experience-driven planning
        HYBRID               // Combination of multiple strategies
    };
    
    // Optimization objectives
    enum class OptimizationObjective {
        MINIMIZE_TIME,       // Optimize for execution time
        MINIMIZE_RESOURCES,  // Optimize for resource usage
        MAXIMIZE_SUCCESS,    // Optimize for success probability
        MINIMIZE_COMPLEXITY, // Optimize for plan simplicity
        BALANCED            // Multi-objective optimization
    };
    
    // Planning performance metrics
    struct PlanningMetrics {
        double success_rate{0.0};              // Plan success percentage
        double average_execution_time{0.0};    // Average plan execution time
        double resource_efficiency{0.0};       // Resource utilization efficiency
        double adaptability_score{0.0};        // How well plans adapt to changes
        double learning_rate{0.0};             // Rate of planning improvement
        std::chrono::steady_clock::time_point last_updated;
        
        void reset() {
            success_rate = 0.0;
            average_execution_time = 0.0;
            resource_efficiency = 0.0;
            adaptability_score = 0.0;
            learning_rate = 0.0;
            last_updated = std::chrono::steady_clock::now();
        }
    };
    
    // Planning strategy evaluation result
    struct StrategyEvaluation {
        PlanningStrategy strategy;
        double effectiveness_score{0.0};
        PlanningMetrics metrics;
        std::map<std::string, double> context_suitability;
        Handle strategy_atom;
        
        StrategyEvaluation() : strategy(PlanningStrategy::ADAPTIVE) {}
        StrategyEvaluation(PlanningStrategy s) : strategy(s) {}
    };

private:
    // Core references
    AgentZeroCore* _agent_core;
    AtomSpacePtr _atomspace;
    std::shared_ptr<TaskManager> _task_manager;
    std::shared_ptr<ActionScheduler> _action_scheduler;
    
    // Meta-planning state
    PlanningStrategy _current_strategy;
    OptimizationObjective _current_objective;
    PlanningMetrics _current_metrics;
    std::map<PlanningStrategy, StrategyEvaluation> _strategy_evaluations;
    
    // Learning and optimization structures
    std::queue<Handle> _planning_episodes;
    std::map<Handle, PlanningMetrics> _episode_metrics;
    std::vector<Handle> _optimization_patterns;
    std::map<std::string, double> _context_features;
    
    // AtomSpace handles for meta-planning
    Handle _metaplanning_context;
    Handle _strategy_context;
    Handle _optimization_context;
    Handle _learning_context;
    Handle _performance_context;
    
    // Configuration
    int _max_planning_episodes;
    double _learning_rate;
    double _adaptation_threshold;
    bool _enable_strategy_learning;
    bool _enable_temporal_optimization;
    bool _enable_pln_reasoning;
    std::chrono::milliseconds _reflection_interval;

    // Plan quality tracking (MetaPlanner enhancements)
    float _average_plan_quality{0.0f};
    int   _plan_quality_samples{0};
    
    // Internal methods - Strategy Management
    void initializeMetaPlanning();
    void initializeStrategies();
    StrategyEvaluation evaluateStrategy(PlanningStrategy strategy, const Handle& context);
    PlanningStrategy selectOptimalStrategy(const Handle& context);
    void updateStrategyPerformance(PlanningStrategy strategy, const PlanningMetrics& metrics);
    
    // Internal methods - Performance Analysis
    PlanningMetrics analyzePlanningPerformance(const Handle& planning_episode);
    void updateMetrics(const PlanningMetrics& new_metrics);
    double calculateEffectivenessScore(const PlanningMetrics& metrics);
    std::map<std::string, double> extractContextFeatures(const Handle& context);
    
    // Internal methods - Learning and Adaptation
    void learnFromPlanningEpisode(const Handle& episode);
    Handle identifyOptimizationPattern(const std::vector<Handle>& episodes);
    void adaptPlanningStrategies();
    bool shouldTriggerOptimization();
    
    // Internal methods - AtomSpace Integration
    Handle createMetricAtom(const PlanningMetrics& metrics);
    Handle createStrategyAtom(PlanningStrategy strategy);
    Handle createOptimizationAtom(const std::string& optimization_type, double improvement);
    void recordPlanningDecision(PlanningStrategy strategy, const Handle& context, double outcome);
    
    // Internal methods - Temporal Integration
    void integrateWithSpacetime();
    Handle analyzeTemporalPatterns();
    void optimizeTemporalPlanning();

public:
    /**
     * Constructor
     * @param agent_core Pointer to the parent AgentZeroCore instance
     * @param atomspace Shared pointer to the AtomSpace
     */
    MetaPlanner(AgentZeroCore* agent_core, AtomSpacePtr atomspace);
    
    /**
     * Destructor - ensures cleanup of meta-planning resources
     */
    ~MetaPlanner();
    
    // Core meta-planning interface
    /**
     * Set the optimization objective for meta-planning
     * @param objective The optimization objective to pursue
     */
    void setOptimizationObjective(OptimizationObjective objective);
    
    /**
     * Get the current optimization objective
     * @return Current optimization objective
     */
    OptimizationObjective getOptimizationObjective() const { return _current_objective; }
    
    /**
     * Analyze current planning effectiveness and suggest improvements
     * @param context_atom Handle to current planning context
     * @return Handle to analysis results atom
     */
    Handle analyzePlanningEffectiveness(const Handle& context_atom);
    
    /**
     * Optimize planning strategy based on current context and objectives
     * @param context_atom Handle to planning context
     * @return Recommended planning strategy
     */
    PlanningStrategy optimizePlanningStrategy(const Handle& context_atom);
    
    /**
     * Record a completed planning episode for learning
     * @param episode_atom Handle to the planning episode
     * @param success Whether the episode was successful
     * @param execution_time Time taken to execute the plan
     */
    void recordPlanningEpisode(const Handle& episode_atom, bool success, 
                              std::chrono::milliseconds execution_time);
    
    // Strategy management
    /**
     * Get the current planning strategy
     * @return Current planning strategy
     */
    PlanningStrategy getCurrentStrategy() const { return _current_strategy; }
    
    /**
     * Force a specific planning strategy (overriding optimization)
     * @param strategy The strategy to use
     */
    void setStrategy(PlanningStrategy strategy);
    
    /**
     * Get performance metrics for a specific strategy
     * @param strategy The strategy to query
     * @return Strategy evaluation results
     */
    StrategyEvaluation getStrategyEvaluation(PlanningStrategy strategy) const;
    
    // Learning and adaptation
    /**
     * Trigger meta-planning reflection and optimization
     * @return Handle to optimization results atom
     */
    Handle triggerReflection();
    
    /**
     * Learn optimization patterns from planning history
     * @param max_episodes Maximum number of episodes to analyze
     * @return Number of patterns learned
     */
    int learnOptimizationPatterns(int max_episodes = 100);
    
    /**
     * Apply learned optimizations to current planning
     * @param context_atom Handle to current context
     * @return Number of optimizations applied
     */
    int applyOptimizations(const Handle& context_atom);
    
    // Performance monitoring
    /**
     * Get current planning performance metrics
     * @return Current performance metrics
     */
    PlanningMetrics getCurrentMetrics() const { return _current_metrics; }
    
    /**
     * Get planning performance trend over time
     * @param time_window Time window to analyze
     * @return Handle to trend analysis atom
     */
    Handle getPerformanceTrend(std::chrono::hours time_window);
    
    /**
     * Reset performance metrics and learning state
     */
    void resetMetrics();
    
    // Configuration
    /**
     * Configure meta-planning parameters
     * @param learning_rate Rate of learning adaptation (0.0-1.0)
     * @param adaptation_threshold Threshold for triggering adaptation
     * @param enable_temporal_optimization Enable spacetime integration
     */
    void configure(double learning_rate, double adaptation_threshold, 
                  bool enable_temporal_optimization = true);
    
    /**
     * Set reflection interval for periodic optimization
     * @param interval Time interval between reflection cycles
     */
    void setReflectionInterval(std::chrono::milliseconds interval);
    
    // Integration points
    /**
     * Set references to other Agent-Zero components
     * @param task_manager Shared pointer to TaskManager
     * @param action_scheduler Shared pointer to ActionScheduler
     */
    void setComponentReferences(std::shared_ptr<TaskManager> task_manager,
                               std::shared_ptr<ActionScheduler> action_scheduler);
    
    /**
     * Check if meta-planner is properly initialized
     * @return True if all components are ready
     */
    bool isInitialized() const;

    // Plan quality integration (MetaPlanner enhancements — AZ-PLAN-004)
    /**
     * Plan quality metrics structure received from PlanningEngine.
     *
     * Mirrors PlanningEngine::PlanQualityMetrics so that MetaPlanner can
     * consume quality data without a direct header dependency on the
     * planning module.
     */
    struct PlanQualityReport {
        float optimality_score{0.0f};
        float goal_coverage{0.0f};
        float temporal_satisfaction{0.0f};
        float resource_efficiency{0.0f};
        float robustness_score{0.0f};
        int   action_count{0};
        float average_action_confidence{0.0f};
        std::chrono::milliseconds planning_time{0};

        float overallScore() const {
            return (optimality_score + goal_coverage + temporal_satisfaction
                    + resource_efficiency + robustness_score) / 5.0f;
        }
    };

    /**
     * Record a plan quality report to drive self-optimisation.
     *
     * Call this after every planning cycle with the quality metrics
     * returned by PlanningEngine::computePlanQuality().  The MetaPlanner
     * uses this data to update strategy evaluations and trigger reflection
     * when quality degrades.
     *
     * @param report     Quality metrics for the completed plan
     * @param context_atom Handle to the planning context
     */
    void recordPlanQuality(const PlanQualityReport& report,
                           const Handle& context_atom);

    /**
     * Retrieve the moving-average plan quality seen so far.
     * @return Scalar score in [0, 1]
     */
    float getAveragePlanQuality() const { return _average_plan_quality; }

    // Utility methods
    /**
     * Convert strategy enum to string representation
     * @param strategy The strategy to convert
     * @return String representation
     */
    static std::string strategyToString(PlanningStrategy strategy);
    
    /**
     * Convert objective enum to string representation
     * @param objective The objective to convert
     * @return String representation
     */
    static std::string objectiveToString(OptimizationObjective objective);
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_META_PLANNER_H