/*
 * src/MetaPlanner.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * MetaPlanner Implementation
 * Self-reflective planning optimization system for Agent-Zero
 * Part of AZ-PLAN-003: Implement MetaPlanner for self-optimization
 */

#include <sstream>
#include <algorithm>
#include <ctime>
#include <cmath>
#include <random>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

#include "opencog/agentzero/MetaPlanner.h"
#include "opencog/agentzero/AgentZeroCore.h"
#include "opencog/agentzero/TaskManager.h"
#include "opencog/agentzero/ActionScheduler.h"

using namespace opencog;
using namespace opencog::agentzero;

MetaPlanner::MetaPlanner(AgentZeroCore* agent_core, AtomSpacePtr atomspace)
    : _agent_core(agent_core)
    , _atomspace(atomspace)
    , _current_strategy(PlanningStrategy::ADAPTIVE)
    , _current_objective(OptimizationObjective::BALANCED)
    , _max_planning_episodes(1000)
    , _learning_rate(0.1)
    , _adaptation_threshold(0.15)
    , _enable_strategy_learning(true)
    , _enable_temporal_optimization(true)
    , _enable_pln_reasoning(true)
    , _reflection_interval(std::chrono::minutes(5))
{
    logger().info() << "[MetaPlanner] Initializing self-reflective planning optimization system";
    
    initializeMetaPlanning();
    initializeStrategies();
    
    logger().info() << "[MetaPlanner] MetaPlanner initialized with " 
                   << _strategy_evaluations.size() << " planning strategies";
}

MetaPlanner::~MetaPlanner()
{
    logger().info() << "[MetaPlanner] Shutting down meta-planning system";
}

void MetaPlanner::initializeMetaPlanning()
{
    logger().debug() << "[MetaPlanner] Initializing meta-planning contexts";
    
    // Create core AtomSpace contexts
    _metaplanning_context = _atomspace->add_node(CONCEPT_NODE, "MetaPlanningContext");
    _metaplanning_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    
    _strategy_context = _atomspace->add_node(CONCEPT_NODE, "PlanningStrategyContext");
    _optimization_context = _atomspace->add_node(CONCEPT_NODE, "OptimizationContext");
    _learning_context = _atomspace->add_node(CONCEPT_NODE, "LearningContext");
    _performance_context = _atomspace->add_node(CONCEPT_NODE, "PerformanceContext");
    
    // Link contexts to meta-planning
    _atomspace->add_link(MEMBER_LINK, {_strategy_context, _metaplanning_context});
    _atomspace->add_link(MEMBER_LINK, {_optimization_context, _metaplanning_context});
    _atomspace->add_link(MEMBER_LINK, {_learning_context, _metaplanning_context});
    _atomspace->add_link(MEMBER_LINK, {_performance_context, _metaplanning_context});
    
    // Initialize current metrics
    _current_metrics.reset();
    
    // Initialize context features
    _context_features["complexity"] = 0.5;
    _context_features["time_pressure"] = 0.3;
    _context_features["resource_availability"] = 0.8;
    _context_features["uncertainty"] = 0.4;
    
    logger().debug() << "[MetaPlanner] Meta-planning contexts initialized";
}

void MetaPlanner::initializeStrategies()
{
    logger().debug() << "[MetaPlanner] Initializing planning strategies";
    
    // Initialize all planning strategies with default evaluations
    std::vector<MetaPlanner::PlanningStrategy> strategies = {
        MetaPlanner::PlanningStrategy::GREEDY,
        MetaPlanner::PlanningStrategy::HIERARCHICAL,
        MetaPlanner::PlanningStrategy::TEMPORAL,
        MetaPlanner::PlanningStrategy::ADAPTIVE,
        MetaPlanner::PlanningStrategy::LEARNING_BASED,
        MetaPlanner::PlanningStrategy::HYBRID
    };
    
    for (auto strategy : strategies) {
        StrategyEvaluation eval(strategy);
        eval.effectiveness_score = 0.5; // Start with neutral effectiveness
        eval.strategy_atom = createStrategyAtom(strategy);
        
        // Set initial context suitability based on strategy characteristics
        switch (strategy) {
            case MetaPlanner::PlanningStrategy::GREEDY:
                eval.context_suitability["simple_tasks"] = 0.8;
                eval.context_suitability["time_critical"] = 0.7;
                eval.context_suitability["complex_goals"] = 0.3;
                break;
            case MetaPlanner::PlanningStrategy::HIERARCHICAL:
                eval.context_suitability["complex_goals"] = 0.9;
                eval.context_suitability["structured_domains"] = 0.8;
                eval.context_suitability["simple_tasks"] = 0.4;
                break;
            case MetaPlanner::PlanningStrategy::TEMPORAL:
                eval.context_suitability["time_critical"] = 0.9;
                eval.context_suitability["scheduling_heavy"] = 0.8;
                eval.context_suitability["resource_constrained"] = 0.6;
                break;
            case MetaPlanner::PlanningStrategy::ADAPTIVE:
                eval.context_suitability["uncertain_environments"] = 0.8;
                eval.context_suitability["dynamic_goals"] = 0.7;
                eval.context_suitability["learning_required"] = 0.6;
                break;
            case MetaPlanner::PlanningStrategy::LEARNING_BASED:
                eval.context_suitability["repetitive_tasks"] = 0.9;
                eval.context_suitability["pattern_rich"] = 0.8;
                eval.context_suitability["experience_available"] = 0.9;
                break;
            case MetaPlanner::PlanningStrategy::HYBRID:
                eval.context_suitability["mixed_complexity"] = 0.8;
                eval.context_suitability["multi_objective"] = 0.9;
                eval.context_suitability["balanced_requirements"] = 0.8;
                break;
        }
        
        _strategy_evaluations[strategy] = eval;
    }
    
    logger().debug() << "[MetaPlanner] Initialized " << _strategy_evaluations.size() << " planning strategies";
}

void MetaPlanner::setOptimizationObjective(OptimizationObjective objective)
{
    if (_current_objective != objective) {
        logger().info() << "[MetaPlanner] Changing optimization objective from " 
                       << objectiveToString(_current_objective) << " to " 
                       << objectiveToString(objective);
        
        _current_objective = objective;
        
        // Record objective change in AtomSpace
        Handle objective_atom = _atomspace->add_node(CONCEPT_NODE, objectiveToString(objective));
        Handle change_pred = _atomspace->add_node(PREDICATE_NODE, "optimization_objective_changed");
        Handle change_eval = _atomspace->add_link(EVALUATION_LINK, {change_pred, objective_atom});
        change_eval->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));
        
        // Link to optimization context
        _atomspace->add_link(MEMBER_LINK, {objective_atom, _optimization_context});
    }
}

Handle MetaPlanner::analyzePlanningEffectiveness(const Handle& context_atom)
{
    logger().debug() << "[MetaPlanner] Analyzing planning effectiveness for context: " << context_atom;
    
    if (context_atom == Handle::UNDEFINED) {
        logger().error() << "[MetaPlanner] Cannot analyze effectiveness with undefined context";
        return Handle::UNDEFINED;
    }
    
    try {
        // Extract context features
        std::map<std::string, double> context_features = extractContextFeatures(context_atom);
        
        // Analyze current strategy performance
        double current_effectiveness = calculateEffectivenessScore(_current_metrics);
        
        // Compare with alternative strategies
        std::vector<std::pair<MetaPlanner::PlanningStrategy, double>> strategy_scores;
        for (const auto& [strategy, evaluation] : _strategy_evaluations) {
            double score = evaluation.effectiveness_score;
            
            // Weight by context suitability
            for (const auto& [feature, value] : context_features) {
                if (evaluation.context_suitability.count(feature)) {
                    score *= (0.8 + 0.4 * evaluation.context_suitability.at(feature));
                }
            }
            
            strategy_scores.push_back({strategy, score});
        }
        
        // Sort by effectiveness score
        std::sort(strategy_scores.begin(), strategy_scores.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        // Create analysis results atom
        Handle analysis_atom = _atomspace->add_node(CONCEPT_NODE, 
            "PlanningEffectivenessAnalysis_" + std::to_string(std::time(nullptr)));
        
        // Record current effectiveness
        Handle current_eff_pred = _atomspace->add_node(PREDICATE_NODE, "current_effectiveness");
        Handle current_eff_value = _atomspace->add_node(NUMBER_NODE, std::to_string(current_effectiveness));
        Handle current_eff_eval = _atomspace->add_link(EVALUATION_LINK, 
            {current_eff_pred, analysis_atom, current_eff_value});
        
        // Record best alternative strategy
        if (!strategy_scores.empty()) {
            Handle best_strategy_atom = createStrategyAtom(strategy_scores[0].first);
            Handle best_pred = _atomspace->add_node(PREDICATE_NODE, "best_alternative_strategy");
            Handle best_eval = _atomspace->add_link(EVALUATION_LINK, 
                {best_pred, analysis_atom, best_strategy_atom});
            
            Handle improvement_pred = _atomspace->add_node(PREDICATE_NODE, "potential_improvement");
            Handle improvement_value = _atomspace->add_node(NUMBER_NODE, 
                std::to_string(strategy_scores[0].second - current_effectiveness));
            Handle improvement_eval = _atomspace->add_link(EVALUATION_LINK, 
                {improvement_pred, analysis_atom, improvement_value});
        }
        
        // Link to performance context 
        _atomspace->add_link(MEMBER_LINK, {analysis_atom, _performance_context});
        
        logger().info() << "[MetaPlanner] Effectiveness analysis completed: current=" 
                       << current_effectiveness << ", best_alternative=" 
                       << (strategy_scores.empty() ? 0.0 : strategy_scores[0].second);
        
        return analysis_atom;
        
    } catch (const std::exception& e) {
        logger().error() << "[MetaPlanner] Error in effectiveness analysis: " << e.what();
        return Handle::UNDEFINED;
    }
}

MetaPlanner::PlanningStrategy MetaPlanner::optimizePlanningStrategy(const Handle& context_atom)
{
    logger().debug() << "[MetaPlanner] Optimizing planning strategy for context: " << context_atom;
    
    if (context_atom == Handle::UNDEFINED) {
        logger().warn() << "[MetaPlanner] Using current strategy due to undefined context";
        return _current_strategy;
    }
    
    try {
        // Extract context features
        std::map<std::string, double> context_features = extractContextFeatures(context_atom);
        
        // Evaluate all strategies for this context
        std::vector<std::pair<MetaPlanner::PlanningStrategy, double>> strategy_scores;
        
        for (const auto& [strategy, evaluation] : _strategy_evaluations) {
            double base_score = evaluation.effectiveness_score;
            double context_bonus = 0.0;
            
            // Calculate context suitability bonus
            for (const auto& [feature, value] : context_features) {
                if (evaluation.context_suitability.count(feature)) {
                    context_bonus += value * evaluation.context_suitability.at(feature);
                }
            }
            
            // Apply objective-specific weighting
            double objective_weight = 1.0;
            switch (_current_objective) {
                case MetaPlanner::OptimizationObjective::MINIMIZE_TIME:
                    if (strategy == MetaPlanner::PlanningStrategy::GREEDY || strategy == MetaPlanner::PlanningStrategy::TEMPORAL) {
                        objective_weight = 1.2;
                    }
                    break;
                case MetaPlanner::OptimizationObjective::MAXIMIZE_SUCCESS:
                    if (strategy == MetaPlanner::PlanningStrategy::HIERARCHICAL || strategy == MetaPlanner::PlanningStrategy::ADAPTIVE) {
                        objective_weight = 1.2;
                    }
                    break;
                case MetaPlanner::OptimizationObjective::MINIMIZE_COMPLEXITY:
                    if (strategy == MetaPlanner::PlanningStrategy::GREEDY) {
                        objective_weight = 1.3;
                    }
                    break;
                case MetaPlanner::OptimizationObjective::BALANCED:
                    if (strategy == MetaPlanner::PlanningStrategy::HYBRID || strategy == MetaPlanner::PlanningStrategy::ADAPTIVE) {
                        objective_weight = 1.1;
                    }
                    break;
            }
            
            double total_score = (base_score + context_bonus * 0.3) * objective_weight;
            strategy_scores.push_back({strategy, total_score});
        }
        
        // Sort by score and select best strategy
        std::sort(strategy_scores.begin(), strategy_scores.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        MetaPlanner::PlanningStrategy optimal_strategy = strategy_scores.empty() ? 
            _current_strategy : strategy_scores[0].first;
        
        // Update current strategy if optimization suggests a change
        if (optimal_strategy != _current_strategy) {
            double improvement = strategy_scores[0].second - 
                _strategy_evaluations[_current_strategy].effectiveness_score;
            
            if (improvement > _adaptation_threshold) {
                logger().info() << "[MetaPlanner] Strategy optimization: " 
                               << strategyToString(_current_strategy) << " -> " 
                               << strategyToString(optimal_strategy) 
                               << " (improvement: " << improvement << ")";
                
                // Capture previous strategy before mutation for AtomSpace record
                PlanningStrategy previous_strategy = _current_strategy;
                _current_strategy = optimal_strategy;
                
                // Record strategy change in AtomSpace
                Handle old_strategy_atom = createStrategyAtom(previous_strategy);
                Handle new_strategy_atom = createStrategyAtom(optimal_strategy);
                Handle change_pred = _atomspace->add_node(PREDICATE_NODE, "strategy_optimized");
                Handle change_link = _atomspace->add_link(ORDERED_LINK, {old_strategy_atom, new_strategy_atom});
                Handle change_eval = _atomspace->add_link(EVALUATION_LINK, {change_pred, change_link});
                change_eval->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));
            }
        }
        
        return optimal_strategy;
        
    } catch (const std::exception& e) {
        logger().error() << "[MetaPlanner] Error in strategy optimization: " << e.what();
        return _current_strategy;
    }
}

void MetaPlanner::recordPlanningEpisode(const Handle& episode_atom, bool success, 
                                       std::chrono::milliseconds execution_time)
{
    if (episode_atom == Handle::UNDEFINED) {
        logger().error() << "[MetaPlanner] Cannot record undefined planning episode";
        return;
    }
    
    logger().debug() << "[MetaPlanner] Recording planning episode: " << episode_atom 
                    << " (success=" << success << ", time=" << execution_time.count() << "ms)";
    
    try {
        // Create episode metrics
        PlanningMetrics episode_metrics;
        episode_metrics.success_rate = success ? 1.0 : 0.0;
        episode_metrics.average_execution_time = execution_time.count();
        episode_metrics.last_updated = std::chrono::steady_clock::now();
        
        // Store episode metrics
        _episode_metrics[episode_atom] = episode_metrics;
        
        // Add to episode queue (with size limit)
        _planning_episodes.push(episode_atom);
        if (_planning_episodes.size() > _max_planning_episodes) {
            Handle old_episode = _planning_episodes.front();
            _planning_episodes.pop();
            _episode_metrics.erase(old_episode);
        }
        
        // Update current strategy performance
        updateStrategyPerformance(_current_strategy, episode_metrics);
        
        // Update overall metrics using exponential moving average
        double alpha = _learning_rate;
        _current_metrics.success_rate = alpha * episode_metrics.success_rate + 
                                       (1.0 - alpha) * _current_metrics.success_rate;
        _current_metrics.average_execution_time = alpha * episode_metrics.average_execution_time + 
                                                 (1.0 - alpha) * _current_metrics.average_execution_time;
        _current_metrics.last_updated = std::chrono::steady_clock::now();
        
        // Record in AtomSpace
        Handle episode_pred = _atomspace->add_node(PREDICATE_NODE, "planning_episode_recorded");
        Handle success_value = _atomspace->add_node(CONCEPT_NODE, success ? "success" : "failure");
        Handle time_value = _atomspace->add_node(NUMBER_NODE, std::to_string(execution_time.count()));
        
        Handle episode_eval = _atomspace->add_link(EVALUATION_LINK, 
            {episode_pred, episode_atom, success_value, time_value});
        episode_eval->setTruthValue(SimpleTruthValue::createTV(1.0, 0.95));
        
        // Link to learning context
        _atomspace->add_link(MEMBER_LINK, {episode_atom, _learning_context});
        
        // Learn from episode if enabled
        if (_enable_strategy_learning) {
            learnFromPlanningEpisode(episode_atom);
        }
        
        logger().debug() << "[MetaPlanner] Planning episode recorded successfully";
        
    } catch (const std::exception& e) {
        logger().error() << "[MetaPlanner] Error recording planning episode: " << e.what();
    }
}

Handle MetaPlanner::triggerReflection()
{
    logger().info() << "[MetaPlanner] Triggering meta-planning reflection cycle";
    
    try {
        Handle reflection_atom = _atomspace->add_node(CONCEPT_NODE, 
            "MetaPlanningReflection_" + std::to_string(std::time(nullptr)));
        
        // Analyze overall performance trends
        double recent_success_rate = 0.0;
        double recent_avg_time = 0.0;
        int recent_episodes = 0;
        
        auto now = std::chrono::steady_clock::now();
        auto reflection_window = now - _reflection_interval;
        
        for (const auto& [episode, metrics] : _episode_metrics) {
            if (metrics.last_updated > reflection_window) {
                recent_success_rate += metrics.success_rate;
                recent_avg_time += metrics.average_execution_time;
                recent_episodes++;
            }
        }
        
        if (recent_episodes > 0) {
            recent_success_rate /= recent_episodes;
            recent_avg_time /= recent_episodes;
            
            // Calculate performance change
            double success_change = recent_success_rate - _current_metrics.success_rate;
            double time_change = recent_avg_time - _current_metrics.average_execution_time;
            
            // Record reflection results
            Handle success_change_pred = _atomspace->add_node(PREDICATE_NODE, "success_rate_change");
            Handle success_change_value = _atomspace->add_node(NUMBER_NODE, std::to_string(success_change));
            Handle success_change_eval = _atomspace->add_link(EVALUATION_LINK, 
                {success_change_pred, reflection_atom, success_change_value});
            
            Handle time_change_pred = _atomspace->add_node(PREDICATE_NODE, "execution_time_change");
            Handle time_change_value = _atomspace->add_node(NUMBER_NODE, std::to_string(time_change));
            Handle time_change_eval = _atomspace->add_link(EVALUATION_LINK, 
                {time_change_pred, reflection_atom, time_change_value});
            
            // Trigger adaptation if needed
            if (shouldTriggerOptimization()) {
                adaptPlanningStrategies();
                
                Handle adaptation_pred = _atomspace->add_node(PREDICATE_NODE, "adaptation_triggered");
                Handle adaptation_eval = _atomspace->add_link(EVALUATION_LINK, 
                    {adaptation_pred, reflection_atom});
                adaptation_eval->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));
            }
            
            logger().info() << "[MetaPlanner] Reflection completed: " << recent_episodes 
                           << " episodes analyzed, success_change=" << success_change 
                           << ", time_change=" << time_change;
        }
        
        // Link to learning context
        _atomspace->add_link(MEMBER_LINK, {reflection_atom, _learning_context});
        
        return reflection_atom;
        
    } catch (const std::exception& e) {
        logger().error() << "[MetaPlanner] Error in reflection cycle: " << e.what();
        return Handle::UNDEFINED;
    }
}

void MetaPlanner::setComponentReferences(std::shared_ptr<TaskManager> task_manager,
                                        std::shared_ptr<ActionScheduler> action_scheduler)
{
    _task_manager = task_manager;
    _action_scheduler = action_scheduler;
    
    logger().info() << "[MetaPlanner] Component references set - integration ready";
}

bool MetaPlanner::isInitialized() const
{
    return _atomspace != nullptr && 
           _metaplanning_context != Handle::UNDEFINED &&
           !_strategy_evaluations.empty();
}

// Strategy evaluation implementation
MetaPlanner::StrategyEvaluation MetaPlanner::evaluateStrategy(MetaPlanner::PlanningStrategy strategy, const Handle& context)
{
    auto it = _strategy_evaluations.find(strategy);
    if (it != _strategy_evaluations.end()) {
        return it->second;
    }
    
    // Return default evaluation if strategy not found
    StrategyEvaluation default_eval(strategy);
    return default_eval;
}

void MetaPlanner::updateStrategyPerformance(MetaPlanner::PlanningStrategy strategy, const PlanningMetrics& metrics)
{
    auto it = _strategy_evaluations.find(strategy);
    if (it != _strategy_evaluations.end()) {
        // Update effectiveness score using exponential moving average
        double alpha = _learning_rate;
        double new_score = calculateEffectivenessScore(metrics);
        
        it->second.effectiveness_score = alpha * new_score + 
                                        (1.0 - alpha) * it->second.effectiveness_score;
        it->second.metrics = metrics;
        
        logger().debug() << "[MetaPlanner] Updated " << strategyToString(strategy) 
                        << " effectiveness: " << it->second.effectiveness_score;
    }
}

double MetaPlanner::calculateEffectivenessScore(const PlanningMetrics& metrics)
{
    // Multi-objective scoring based on current optimization objective
    double score = 0.0;
    
    switch (_current_objective) {
        case MetaPlanner::OptimizationObjective::MINIMIZE_TIME:
            score = std::max(0.0, 1.0 - metrics.average_execution_time / 10000.0); // Normalize by 10s
            break;
        case MetaPlanner::OptimizationObjective::MAXIMIZE_SUCCESS:
            score = metrics.success_rate;
            break;
        case MetaPlanner::OptimizationObjective::MINIMIZE_COMPLEXITY:
            score = 0.8 * metrics.success_rate + 0.2 * metrics.resource_efficiency;
            break;
        case MetaPlanner::OptimizationObjective::BALANCED:
            score = 0.4 * metrics.success_rate + 
                   0.3 * std::max(0.0, 1.0 - metrics.average_execution_time / 5000.0) +
                   0.2 * metrics.resource_efficiency +
                   0.1 * metrics.adaptability_score;
            break;
    }
    
    return std::min(1.0, std::max(0.0, score));
}

std::map<std::string, double> MetaPlanner::extractContextFeatures(const Handle& context)
{
    std::map<std::string, double> features = _context_features; // Start with defaults
    
    if (context == Handle::UNDEFINED) {
        return features;
    }
    
    // TODO: Implement sophisticated context feature extraction
    // This would analyze the AtomSpace context to extract relevant features
    // For now, return default features with some variation
    
    return features;
}

void MetaPlanner::learnFromPlanningEpisode(const Handle& episode)
{
    // Simple learning implementation - update context features based on episode outcome
    if (_episode_metrics.count(episode)) {
        const auto& metrics = _episode_metrics[episode];
        
        // Adjust context features based on episode success
        double adjustment = (metrics.success_rate - 0.5) * _learning_rate * 0.1;
        
        for (auto& [feature, value] : _context_features) {
            value = std::min(1.0, std::max(0.0, value + adjustment));
        }
    }
}

void MetaPlanner::adaptPlanningStrategies()
{
    logger().info() << "[MetaPlanner] Adapting planning strategies based on performance";
    
    // Find best performing strategy
    MetaPlanner::PlanningStrategy best_strategy = _current_strategy;
    double best_score = _strategy_evaluations[_current_strategy].effectiveness_score;
    
    for (const auto& [strategy, evaluation] : _strategy_evaluations) {
        if (evaluation.effectiveness_score > best_score) {
            best_strategy = strategy;
            best_score = evaluation.effectiveness_score;
        }
    }
    
    // Switch to best strategy if significantly better
    if (best_strategy != _current_strategy && 
        best_score > _strategy_evaluations[_current_strategy].effectiveness_score + _adaptation_threshold) {
        
        logger().info() << "[MetaPlanner] Adapting strategy: " 
                       << strategyToString(_current_strategy) << " -> " 
                       << strategyToString(best_strategy);
        
        _current_strategy = best_strategy;
    }
}

bool MetaPlanner::shouldTriggerOptimization()
{
    // Trigger optimization if performance has degraded significantly
    return _current_metrics.success_rate < 0.6 || 
           _current_metrics.average_execution_time > 5000.0; // 5 seconds
}

// AtomSpace integration methods
Handle MetaPlanner::createStrategyAtom(MetaPlanner::PlanningStrategy strategy)
{
    std::string strategy_name = "PlanningStrategy_" + strategyToString(strategy);
    Handle strategy_atom = _atomspace->add_node(CONCEPT_NODE, std::move(strategy_name));
    
    // Link to strategy context
    _atomspace->add_link(MEMBER_LINK, {strategy_atom, _strategy_context});
    
    return strategy_atom;
}

Handle MetaPlanner::createMetricAtom(const PlanningMetrics& metrics)
{
    Handle metrics_atom = _atomspace->add_node(CONCEPT_NODE, 
        "PlanningMetrics_" + std::to_string(std::time(nullptr)));
    
    // Add metric values
    Handle success_pred = _atomspace->add_node(PREDICATE_NODE, "success_rate");
    Handle success_value = _atomspace->add_node(NUMBER_NODE, std::to_string(metrics.success_rate));
    _atomspace->add_link(EVALUATION_LINK, {success_pred, metrics_atom, success_value});
    
    Handle time_pred = _atomspace->add_node(PREDICATE_NODE, "average_execution_time");
    Handle time_value = _atomspace->add_node(NUMBER_NODE, std::to_string(metrics.average_execution_time));
    _atomspace->add_link(EVALUATION_LINK, {time_pred, metrics_atom, time_value});
    
    return metrics_atom;
}

// Utility methods
std::string MetaPlanner::strategyToString(MetaPlanner::PlanningStrategy strategy)
{
    switch (strategy) {
        case MetaPlanner::PlanningStrategy::GREEDY: return "Greedy";
        case MetaPlanner::PlanningStrategy::HIERARCHICAL: return "Hierarchical";
        case MetaPlanner::PlanningStrategy::TEMPORAL: return "Temporal";
        case MetaPlanner::PlanningStrategy::ADAPTIVE: return "Adaptive";
        case MetaPlanner::PlanningStrategy::LEARNING_BASED: return "LearningBased";
        case MetaPlanner::PlanningStrategy::HYBRID: return "Hybrid";
        default: return "Unknown";
    }
}

std::string MetaPlanner::objectiveToString(MetaPlanner::OptimizationObjective objective)
{
    switch (objective) {
        case MetaPlanner::OptimizationObjective::MINIMIZE_TIME: return "MinimizeTime";
        case MetaPlanner::OptimizationObjective::MINIMIZE_RESOURCES: return "MinimizeResources";
        case MetaPlanner::OptimizationObjective::MAXIMIZE_SUCCESS: return "MaximizeSuccess";
        case MetaPlanner::OptimizationObjective::MINIMIZE_COMPLEXITY: return "MinimizeComplexity";
        case MetaPlanner::OptimizationObjective::BALANCED: return "Balanced";
        default: return "Unknown";
    }
}

// Additional implementation stubs for comprehensive interface
MetaPlanner::PlanningStrategy MetaPlanner::selectOptimalStrategy(const Handle& context)
{
    return optimizePlanningStrategy(context);
}

void MetaPlanner::setStrategy(MetaPlanner::PlanningStrategy strategy)
{
    if (_current_strategy != strategy) {
        logger().info() << "[MetaPlanner] Manually setting strategy: " 
                       << strategyToString(_current_strategy) << " -> " 
                       << strategyToString(strategy);
        _current_strategy = strategy;
    }
}

MetaPlanner::StrategyEvaluation MetaPlanner::getStrategyEvaluation(MetaPlanner::PlanningStrategy strategy) const
{
    auto it = _strategy_evaluations.find(strategy);
    if (it != _strategy_evaluations.end()) {
        return it->second;
    }
    return StrategyEvaluation(strategy); // Return default if not found
}

int MetaPlanner::learnOptimizationPatterns(int max_episodes)
{
    // Simplified pattern learning - count successful episodes by strategy
    int patterns_learned = 0;
    std::map<MetaPlanner::PlanningStrategy, int> success_counts;
    
    // Analyze recent episodes (up to max_episodes)
    int analyzed = 0;
    for (const auto& [episode, metrics] : _episode_metrics) {
        if (analyzed >= max_episodes) break;
        
        if (metrics.success_rate > 0.8) { // Consider successful episodes
            success_counts[_current_strategy]++;
            patterns_learned++;
        }
        analyzed++;
    }
    
    logger().info() << "[MetaPlanner] Learned " << patterns_learned << " optimization patterns";
    return patterns_learned;
}

int MetaPlanner::applyOptimizations(const Handle& context_atom)
{
    // Apply learned optimizations - trigger strategy optimization
    PlanningStrategy optimal = optimizePlanningStrategy(context_atom);
    return (optimal != _current_strategy) ? 1 : 0;
}

Handle MetaPlanner::getPerformanceTrend(std::chrono::hours time_window)
{
    Handle trend_atom = _atomspace->add_node(CONCEPT_NODE, 
        "PerformanceTrend_" + std::to_string(std::time(nullptr)));
    
    // Simple trend analysis - compare recent vs historical performance
    auto cutoff = std::chrono::steady_clock::now() - time_window;
    double recent_success = 0.0;
    int recent_count = 0;
    
    for (const auto& [episode, metrics] : _episode_metrics) {
        if (metrics.last_updated > cutoff) {
            recent_success += metrics.success_rate;
            recent_count++;
        }
    }
    
    if (recent_count > 0) {
        recent_success /= recent_count;
        double trend_value = recent_success - _current_metrics.success_rate;
        
        Handle trend_pred = _atomspace->add_node(PREDICATE_NODE, "performance_trend");
        Handle trend_val = _atomspace->add_node(NUMBER_NODE, std::to_string(trend_value));
        _atomspace->add_link(EVALUATION_LINK, {trend_pred, trend_atom, trend_val});
    }
    
    return trend_atom;
}

void MetaPlanner::resetMetrics()
{
    _current_metrics.reset();
    _episode_metrics.clear();
    while (!_planning_episodes.empty()) {
        _planning_episodes.pop();
    }
    
    logger().info() << "[MetaPlanner] Performance metrics and learning state reset";
}

void MetaPlanner::configure(double learning_rate, double adaptation_threshold, 
                           bool enable_temporal_optimization)
{
    _learning_rate = std::min(1.0, std::max(0.0, learning_rate));
    _adaptation_threshold = std::min(1.0, std::max(0.0, adaptation_threshold));
    _enable_temporal_optimization = enable_temporal_optimization;
    
    logger().info() << "[MetaPlanner] Configuration updated: learning_rate=" 
                   << _learning_rate << ", adaptation_threshold=" << _adaptation_threshold
                   << ", temporal_optimization=" << _enable_temporal_optimization;
}

void MetaPlanner::setReflectionInterval(std::chrono::milliseconds interval)
{
    _reflection_interval = interval;
    logger().info() << "[MetaPlanner] Reflection interval set to " << interval.count() << "ms";
}

// Temporal integration methods (stubs for spacetime integration)
void MetaPlanner::integrateWithSpacetime()
{
    logger().debug() << "[MetaPlanner] Integrating with spacetime for temporal planning";
    // TODO: Implement spacetime integration for temporal planning optimization
}

Handle MetaPlanner::analyzeTemporalPatterns()
{
    Handle patterns_atom = _atomspace->add_node(CONCEPT_NODE, 
        "TemporalPatterns_" + std::to_string(std::time(nullptr)));
    
    // TODO: Implement temporal pattern analysis using spacetime
    
    return patterns_atom;
}

void MetaPlanner::optimizeTemporalPlanning()
{
    logger().debug() << "[MetaPlanner] Optimizing temporal planning aspects";
    // TODO: Implement temporal planning optimizations
}

// Additional AtomSpace integration methods
Handle MetaPlanner::createOptimizationAtom(const std::string& optimization_type, double improvement)
{
    Handle opt_atom = _atomspace->add_node(CONCEPT_NODE, 
        "Optimization_" + optimization_type + "_" + std::to_string(std::time(nullptr)));
    
    Handle improvement_pred = _atomspace->add_node(PREDICATE_NODE, "improvement_amount");
    Handle improvement_value = _atomspace->add_node(NUMBER_NODE, std::to_string(improvement));
    _atomspace->add_link(EVALUATION_LINK, {improvement_pred, opt_atom, improvement_value});
    
    _atomspace->add_link(MEMBER_LINK, {opt_atom, _optimization_context});
    
    return opt_atom;
}

void MetaPlanner::recordPlanningDecision(MetaPlanner::PlanningStrategy strategy, const Handle& context, double outcome)
{
    Handle decision_atom = _atomspace->add_node(CONCEPT_NODE, 
        "PlanningDecision_" + std::to_string(std::time(nullptr)));
    
    Handle strategy_atom = createStrategyAtom(strategy);
    Handle outcome_value = _atomspace->add_node(NUMBER_NODE, std::to_string(outcome));
    
    Handle decision_pred = _atomspace->add_node(PREDICATE_NODE, "planning_decision");
    _atomspace->add_link(EVALUATION_LINK, {decision_pred, decision_atom, strategy_atom, context, outcome_value});
    
    _atomspace->add_link(MEMBER_LINK, {decision_atom, _learning_context});
}

MetaPlanner::PlanningMetrics MetaPlanner::analyzePlanningPerformance(const Handle& planning_episode)
{
    PlanningMetrics metrics;
    
    // Extract performance data from AtomSpace if episode exists
    if (_episode_metrics.count(planning_episode)) {
        metrics = _episode_metrics[planning_episode];
    }
    
    return metrics;
}

void MetaPlanner::updateMetrics(const PlanningMetrics& new_metrics)
{
    // Update current metrics using exponential moving average
    double alpha = _learning_rate;
    
    _current_metrics.success_rate = alpha * new_metrics.success_rate + 
                                   (1.0 - alpha) * _current_metrics.success_rate;
    _current_metrics.average_execution_time = alpha * new_metrics.average_execution_time + 
                                             (1.0 - alpha) * _current_metrics.average_execution_time;
    _current_metrics.resource_efficiency = alpha * new_metrics.resource_efficiency + 
                                          (1.0 - alpha) * _current_metrics.resource_efficiency;
    _current_metrics.adaptability_score = alpha * new_metrics.adaptability_score + 
                                         (1.0 - alpha) * _current_metrics.adaptability_score;
    _current_metrics.learning_rate = alpha * new_metrics.learning_rate + 
                                    (1.0 - alpha) * _current_metrics.learning_rate;
    
    _current_metrics.last_updated = std::chrono::steady_clock::now();
}

Handle MetaPlanner::identifyOptimizationPattern(const std::vector<Handle>& episodes)
{
    if (episodes.empty()) {
        return Handle::UNDEFINED;
    }
    
    Handle pattern_atom = _atomspace->add_node(CONCEPT_NODE, 
        "OptimizationPattern_" + std::to_string(std::time(nullptr)));
    
    // Analyze common characteristics of successful episodes
    double avg_success = 0.0;
    for (const Handle& episode : episodes) {
        if (_episode_metrics.count(episode)) {
            avg_success += _episode_metrics[episode].success_rate;
        }
    }
    avg_success /= episodes.size();
    
    Handle pattern_pred = _atomspace->add_node(PREDICATE_NODE, "pattern_success_rate");
    Handle pattern_value = _atomspace->add_node(NUMBER_NODE, std::to_string(avg_success));
    _atomspace->add_link(EVALUATION_LINK, {pattern_pred, pattern_atom, pattern_value});
    
    _atomspace->add_link(MEMBER_LINK, {pattern_atom, _optimization_context});
    
    return pattern_atom;
}
void MetaPlanner::recordPlanQuality(const PlanQualityReport& report,
                                    const Handle& context_atom)
{
    float score = report.overallScore();

    // Exponential moving average update
    if (_plan_quality_samples == 0) {
        _average_plan_quality = score;
    } else {
        _average_plan_quality = (1.0f - static_cast<float>(_learning_rate)) * _average_plan_quality
                                + static_cast<float>(_learning_rate) * score;
    }
    ++_plan_quality_samples;

    logger().debug() << "[MetaPlanner] recordPlanQuality: score=" << score
                     << " avg=" << _average_plan_quality
                     << " actions=" << report.action_count
                     << " coverage=" << report.goal_coverage
                     << " temporal=" << report.temporal_satisfaction;

    // Update the internal PlanningMetrics so existing learning logic picks it up
    PlanningMetrics derived_metrics;
    derived_metrics.success_rate        = double(score);
    derived_metrics.resource_efficiency = double(report.resource_efficiency);
    derived_metrics.adaptability_score  = double(report.robustness_score);
    derived_metrics.average_execution_time =
        double(report.planning_time.count());  // milliseconds
    derived_metrics.last_updated = std::chrono::steady_clock::now();
    updateMetrics(derived_metrics);

    // Persist quality info in AtomSpace for downstream reasoning
    Handle quality_node = _atomspace->add_node(CONCEPT_NODE,
        "PlanQuality_" + std::to_string(std::time(nullptr)));
    Handle pred_node = _atomspace->add_node(PREDICATE_NODE, "plan_quality_score");
    Handle value_node = _atomspace->add_node(NUMBER_NODE, std::to_string(double(score)));
    _atomspace->add_link(EVALUATION_LINK, {pred_node, quality_node, value_node});
    _atomspace->add_link(MEMBER_LINK, {quality_node, _performance_context});

    // Trigger self-optimisation if quality falls below adaptation threshold
    if (double(score) < _adaptation_threshold && _enable_strategy_learning) {
        logger().info() << "[MetaPlanner] Plan quality below threshold ("
                        << score << " < " << _adaptation_threshold
                        << ") — triggering strategy adaptation";
        adaptPlanningStrategies();
        if (context_atom != Handle::UNDEFINED) {
            optimizePlanningStrategy(context_atom);
        }
    }
}
