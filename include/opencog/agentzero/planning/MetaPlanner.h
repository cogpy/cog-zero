/*
 * opencog/agentzero/planning/MetaPlanner.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Plan quality metrics and self-optimisation for Phase 4 planning.
 * Complements agentzero-core MetaPlanner; can run standalone on a planning
 * engine without AgentZeroCore.
 */

#ifndef _OPENCOG_AGENTZERO_PLANNING_META_PLANNER_H
#define _OPENCOG_AGENTZERO_PLANNING_META_PLANNER_H

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#include "PlanningEngine.h"

namespace opencog {
namespace agentzero {
namespace planning {

/**
 * MetaPlanner — records plan quality, selects strategies, self-optimises.
 */
class MetaPlanner
{
public:
    enum class OptimizationObjective {
        MINIMIZE_TIME,
        MINIMIZE_RESOURCES,
        MAXIMIZE_SUCCESS,
        MINIMIZE_COMPLEXITY,
        BALANCED
    };

    struct PlanningMetrics {
        double success_rate{0.0};
        double average_execution_time{0.0};
        double resource_efficiency{0.0};
        double adaptability_score{0.0};
        double learning_rate{0.0};
        std::chrono::steady_clock::time_point last_updated;

        void reset() {
            success_rate = average_execution_time = resource_efficiency = 0.0;
            adaptability_score = learning_rate = 0.0;
            last_updated = std::chrono::steady_clock::now();
        }
    };

    struct StrategyEvaluation {
        PlanningEngine::PlanningStrategy strategy{PlanningEngine::PlanningStrategy::HYBRID};
        double effectiveness_score{0.0};
        PlanningMetrics metrics;
        int sample_count{0};
    };

    /** Mirrors PlanningEngine::PlanQualityMetrics for loose coupling. */
    struct PlanQualityReport {
        float optimality_score{0.0f};
        float goal_coverage{0.0f};
        float temporal_satisfaction{0.0f};
        float resource_efficiency{0.0f};
        float robustness_score{0.0f};
        int action_count{0};
        float average_action_confidence{0.0f};
        std::chrono::milliseconds planning_time{0};

        float overallScore() const {
            return (optimality_score + goal_coverage + temporal_satisfaction
                    + resource_efficiency + robustness_score) / 5.0f;
        }

        static PlanQualityReport fromMetrics(const PlanningEngine::PlanQualityMetrics& m) {
            PlanQualityReport r;
            r.optimality_score = m.optimality_score;
            r.goal_coverage = m.goal_coverage;
            r.temporal_satisfaction = m.temporal_satisfaction;
            r.resource_efficiency = m.resource_efficiency;
            r.robustness_score = m.robustness_score;
            r.action_count = m.action_count;
            r.average_action_confidence = m.average_action_confidence;
            r.planning_time = m.planning_time;
            return r;
        }
    };

    explicit MetaPlanner(AtomSpacePtr atomspace);
    MetaPlanner(AtomSpacePtr atomspace, std::shared_ptr<PlanningEngine> engine);
    ~MetaPlanner();

    bool initialize();
    bool shutdown();
    bool isInitialized() const { return _initialized; }

    void setPlanningEngine(std::shared_ptr<PlanningEngine> engine) {
        _engine = std::move(engine);
    }
    PlanningEngine* getPlanningEngine() const { return _engine.get(); }

    void setOptimizationObjective(OptimizationObjective objective);
    OptimizationObjective getOptimizationObjective() const { return _current_objective; }

    PlanningEngine::PlanningStrategy getCurrentStrategy() const { return _current_strategy; }
    void setStrategy(PlanningEngine::PlanningStrategy strategy);

    /** Recommend a strategy for the given context using learned evaluations. */
    PlanningEngine::PlanningStrategy optimizePlanningStrategy(const Handle& context_atom);

    Handle analyzePlanningEffectiveness(const Handle& context_atom);

    void recordPlanningEpisode(const Handle& episode_atom, bool success,
                               std::chrono::milliseconds execution_time);

    /**
     * Primary self-optimisation hook: feed PlanningEngine quality metrics
     * after every planning cycle.
     */
    void recordPlanQuality(const PlanQualityReport& report, const Handle& context_atom);
    void recordPlanQuality(const PlanningEngine::PlanQualityMetrics& metrics,
                           const Handle& context_atom) {
        recordPlanQuality(PlanQualityReport::fromMetrics(metrics), context_atom);
    }

    float getAveragePlanQuality() const { return _average_plan_quality; }
    int getPlanQualitySamples() const { return _plan_quality_samples; }
    PlanningMetrics getCurrentMetrics() const { return _current_metrics; }
    StrategyEvaluation getStrategyEvaluation(PlanningEngine::PlanningStrategy s) const;

    Handle triggerReflection();
    int learnOptimizationPatterns(int max_episodes = 100);
    int applyOptimizations(const Handle& context_atom);

    void configure(double learning_rate, double adaptation_threshold,
                   bool enable_temporal_optimization = true);
    void setReflectionInterval(std::chrono::milliseconds interval);
    void resetMetrics();

    /**
     * Plan with the engine using the currently preferred strategy, then
     * record quality and possibly adapt.
     */
    PlanningEngine::PlanResult planAndLearn(const Handle& goal_atom);

    static std::string objectiveToString(OptimizationObjective o);
    static std::string strategyToString(PlanningEngine::PlanningStrategy s);

private:
    AtomSpacePtr _atomspace;
    std::shared_ptr<PlanningEngine> _engine;
    bool _initialized{false};

    PlanningEngine::PlanningStrategy _current_strategy{PlanningEngine::PlanningStrategy::HYBRID};
    OptimizationObjective _current_objective{OptimizationObjective::BALANCED};
    PlanningMetrics _current_metrics;
    std::map<PlanningEngine::PlanningStrategy, StrategyEvaluation> _strategy_evaluations;

    float _average_plan_quality{0.0f};
    int _plan_quality_samples{0};
    double _learning_rate{0.1};
    double _adaptation_threshold{0.15};
    bool _enable_strategy_learning{true};
    bool _enable_temporal_optimization{true};
    std::chrono::milliseconds _reflection_interval{std::chrono::minutes(5)};
    int _max_planning_episodes{1000};

    Handle _metaplanning_context;
    Handle _strategy_context;
    Handle _performance_context;

    std::vector<Handle> _planning_episodes;
    std::map<Handle, PlanningMetrics> _episode_metrics;

    void initializeStrategies();
    void createContexts();
    void updateMetrics(const PlanningMetrics& m);
    void updateStrategyPerformance(PlanningEngine::PlanningStrategy s,
                                   const PlanningMetrics& m);
    void adaptPlanningStrategies();
    double calculateEffectivenessScore(const PlanningMetrics& m) const;
    Handle createStrategyAtom(PlanningEngine::PlanningStrategy s);
};

} // namespace planning
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_PLANNING_META_PLANNER_H
