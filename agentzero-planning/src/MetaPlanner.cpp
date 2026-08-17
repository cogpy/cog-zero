/*
 * opencog/agentzero/planning/MetaPlanner.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Plan quality metrics and self-optimisation.
 */

#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/planning/MetaPlanner.h"

using namespace opencog;
using namespace opencog::agentzero::planning;

MetaPlanner::MetaPlanner(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
{
    if (!_atomspace)
        throw std::runtime_error("MetaPlanner requires a valid AtomSpace");
}

MetaPlanner::MetaPlanner(AtomSpacePtr atomspace,
                         std::shared_ptr<PlanningEngine> engine)
    : _atomspace(atomspace)
    , _engine(std::move(engine))
{
    if (!_atomspace)
        throw std::runtime_error("MetaPlanner requires a valid AtomSpace");
}

MetaPlanner::~MetaPlanner() = default;

bool MetaPlanner::initialize()
{
    if (_initialized) return true;
    createContexts();
    initializeStrategies();
    _current_metrics.reset();
    _initialized = true;
    logger().info() << "[planning::MetaPlanner] Initialized with "
                    << _strategy_evaluations.size() << " strategies";
    return true;
}

bool MetaPlanner::shutdown()
{
    _initialized = false;
    return true;
}

void MetaPlanner::createContexts()
{
    _metaplanning_context =
        _atomspace->add_node(CONCEPT_NODE, "PlanningMetaContext");
    _strategy_context =
        _atomspace->add_node(CONCEPT_NODE, "PlanningStrategyContext");
    _performance_context =
        _atomspace->add_node(CONCEPT_NODE, "PlanningPerformanceContext");
    _metaplanning_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    _atomspace->add_link(MEMBER_LINK, {_strategy_context, _metaplanning_context});
    _atomspace->add_link(MEMBER_LINK, {_performance_context, _metaplanning_context});
}

void MetaPlanner::initializeStrategies()
{
    using PS = PlanningEngine::PlanningStrategy;
    std::vector<PS> strategies = {
        PS::HIERARCHICAL,
        PS::STRIPS,
        PS::FORWARD_SEARCH,
        PS::BACKWARD_SEARCH,
        PS::HYBRID,
        PS::TEMPORAL_FIRST,
        PS::RESOURCE_OPTIMAL
    };
    for (PS s : strategies) {
        StrategyEvaluation ev;
        ev.strategy = s;
        ev.effectiveness_score = 0.5;
        ev.metrics.reset();
        ev.sample_count = 0;
        _strategy_evaluations[s] = ev;
        createStrategyAtom(s);
    }
}

Handle MetaPlanner::createStrategyAtom(PlanningEngine::PlanningStrategy s)
{
    Handle node = _atomspace->add_node(CONCEPT_NODE, "Strategy_" + strategyToString(s));
    _atomspace->add_link(MEMBER_LINK, {node, _strategy_context});
    return node;
}

void MetaPlanner::setOptimizationObjective(OptimizationObjective objective)
{
    _current_objective = objective;
}

void MetaPlanner::setStrategy(PlanningEngine::PlanningStrategy strategy)
{
    _current_strategy = strategy;
}

double MetaPlanner::calculateEffectivenessScore(const PlanningMetrics& m) const
{
    switch (_current_objective) {
        case OptimizationObjective::MINIMIZE_TIME:
            return std::max(0.0, 1.0 - m.average_execution_time / 1000.0) * 0.7
                   + m.success_rate * 0.3;
        case OptimizationObjective::MINIMIZE_RESOURCES:
            return m.resource_efficiency * 0.7 + m.success_rate * 0.3;
        case OptimizationObjective::MAXIMIZE_SUCCESS:
            return m.success_rate;
        case OptimizationObjective::MINIMIZE_COMPLEXITY:
            return m.resource_efficiency * 0.5 + m.success_rate * 0.5;
        case OptimizationObjective::BALANCED:
        default:
            return (m.success_rate + m.resource_efficiency + m.adaptability_score) / 3.0;
    }
}

void MetaPlanner::updateMetrics(const PlanningMetrics& m)
{
    double a = _learning_rate;
    _current_metrics.success_rate =
        a * m.success_rate + (1.0 - a) * _current_metrics.success_rate;
    _current_metrics.average_execution_time =
        a * m.average_execution_time + (1.0 - a) * _current_metrics.average_execution_time;
    _current_metrics.resource_efficiency =
        a * m.resource_efficiency + (1.0 - a) * _current_metrics.resource_efficiency;
    _current_metrics.adaptability_score =
        a * m.adaptability_score + (1.0 - a) * _current_metrics.adaptability_score;
    _current_metrics.learning_rate = _learning_rate;
    _current_metrics.last_updated = std::chrono::steady_clock::now();
}

void MetaPlanner::updateStrategyPerformance(PlanningEngine::PlanningStrategy s,
                                            const PlanningMetrics& m)
{
    auto& ev = _strategy_evaluations[s];
    ev.strategy = s;
    double a = _learning_rate;
    if (ev.sample_count == 0) {
        ev.metrics = m;
        ev.effectiveness_score = calculateEffectivenessScore(m);
    } else {
        ev.metrics.success_rate =
            a * m.success_rate + (1.0 - a) * ev.metrics.success_rate;
        ev.metrics.average_execution_time =
            a * m.average_execution_time + (1.0 - a) * ev.metrics.average_execution_time;
        ev.metrics.resource_efficiency =
            a * m.resource_efficiency + (1.0 - a) * ev.metrics.resource_efficiency;
        ev.metrics.adaptability_score =
            a * m.adaptability_score + (1.0 - a) * ev.metrics.adaptability_score;
        ev.effectiveness_score = calculateEffectivenessScore(ev.metrics);
    }
    ++ev.sample_count;
}

void MetaPlanner::adaptPlanningStrategies()
{
    PlanningEngine::PlanningStrategy best = _current_strategy;
    double best_score = -1.0;
    for (const auto& kv : _strategy_evaluations) {
        if (kv.second.sample_count == 0) continue;
        if (kv.second.effectiveness_score > best_score) {
            best_score = kv.second.effectiveness_score;
            best = kv.first;
        }
    }
    if (best_score >= 0.0 && best != _current_strategy) {
        logger().info() << "[planning::MetaPlanner] Switching strategy "
                        << strategyToString(_current_strategy) << " -> "
                        << strategyToString(best)
                        << " (score=" << best_score << ")";
        _current_strategy = best;
    }
}

PlanningEngine::PlanningStrategy
MetaPlanner::optimizePlanningStrategy(const Handle& context_atom)
{
    (void)context_atom;
    if (!_initialized) initialize();

    // Prefer strategies with samples; otherwise keep current
    PlanningEngine::PlanningStrategy best = _current_strategy;
    double best_score = -1.0;
    for (const auto& kv : _strategy_evaluations) {
        double score = kv.second.effectiveness_score;
        // slight prior boost for HYBRID / HIERARCHICAL
        if (kv.second.sample_count == 0) {
            if (kv.first == PlanningEngine::PlanningStrategy::HYBRID) score = 0.55;
            else if (kv.first == PlanningEngine::PlanningStrategy::HIERARCHICAL) score = 0.52;
            else score = 0.45;
        }
        if (score > best_score) {
            best_score = score;
            best = kv.first;
        }
    }
    _current_strategy = best;
    return best;
}

Handle MetaPlanner::analyzePlanningEffectiveness(const Handle& context_atom)
{
    if (!_initialized) initialize();
    Handle analysis = _atomspace->add_node(
        CONCEPT_NODE,
        "PlanAnalysis_" + std::to_string(std::time(nullptr)));
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "planning_effectiveness");
    Handle val = _atomspace->add_node(
        NUMBER_NODE, std::to_string(_current_metrics.success_rate));
    _atomspace->add_link(EVALUATION_LINK, {pred, analysis, val});
    if (context_atom != Handle::UNDEFINED)
        _atomspace->add_link(MEMBER_LINK, {analysis, context_atom});
    _atomspace->add_link(MEMBER_LINK, {analysis, _performance_context});
    return analysis;
}

void MetaPlanner::recordPlanningEpisode(const Handle& episode_atom, bool success,
                                        std::chrono::milliseconds execution_time)
{
    if (!_initialized) initialize();
    PlanningMetrics m;
    m.success_rate = success ? 1.0 : 0.0;
    m.average_execution_time = static_cast<double>(execution_time.count());
    m.resource_efficiency = success ? 0.8 : 0.2;
    m.adaptability_score = 0.5;
    m.last_updated = std::chrono::steady_clock::now();

    _episode_metrics[episode_atom] = m;
    _planning_episodes.push_back(episode_atom);
    if (static_cast<int>(_planning_episodes.size()) > _max_planning_episodes)
        _planning_episodes.erase(_planning_episodes.begin());

    updateMetrics(m);
    updateStrategyPerformance(_current_strategy, m);
}

void MetaPlanner::recordPlanQuality(const PlanQualityReport& report,
                                    const Handle& context_atom)
{
    if (!_initialized) initialize();
    float score = report.overallScore();

    if (_plan_quality_samples == 0)
        _average_plan_quality = score;
    else
        _average_plan_quality =
            static_cast<float>(1.0 - _learning_rate) * _average_plan_quality +
            static_cast<float>(_learning_rate) * score;
    ++_plan_quality_samples;

    PlanningMetrics derived;
    derived.success_rate = static_cast<double>(score);
    derived.resource_efficiency = static_cast<double>(report.resource_efficiency);
    derived.adaptability_score = static_cast<double>(report.robustness_score);
    derived.average_execution_time = static_cast<double>(report.planning_time.count());
    derived.last_updated = std::chrono::steady_clock::now();
    updateMetrics(derived);
    updateStrategyPerformance(_current_strategy, derived);

    Handle quality_node = _atomspace->add_node(
        CONCEPT_NODE, "PlanQuality_" + std::to_string(std::time(nullptr)));
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "plan_quality_score");
    Handle value = _atomspace->add_node(NUMBER_NODE, std::to_string(static_cast<double>(score)));
    _atomspace->add_link(EVALUATION_LINK, {pred, quality_node, value});
    _atomspace->add_link(MEMBER_LINK, {quality_node, _performance_context});
    if (context_atom != Handle::UNDEFINED)
        _atomspace->add_link(CONTEXT_LINK, {context_atom, quality_node});

    if (static_cast<double>(score) < _adaptation_threshold && _enable_strategy_learning) {
        logger().info() << "[planning::MetaPlanner] Quality below threshold ("
                        << score << " < " << _adaptation_threshold
                        << ") — adapting strategies";
        adaptPlanningStrategies();
        if (context_atom != Handle::UNDEFINED)
            optimizePlanningStrategy(context_atom);
    }
}

Handle MetaPlanner::triggerReflection()
{
    if (!_initialized) initialize();
    adaptPlanningStrategies();
    Handle reflection = _atomspace->add_node(
        CONCEPT_NODE, "Reflection_" + std::to_string(std::time(nullptr)));
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "avg_plan_quality");
    Handle val = _atomspace->add_node(
        NUMBER_NODE, std::to_string(static_cast<double>(_average_plan_quality)));
    _atomspace->add_link(EVALUATION_LINK, {pred, reflection, val});
    _atomspace->add_link(MEMBER_LINK, {reflection, _metaplanning_context});
    return reflection;
}

int MetaPlanner::learnOptimizationPatterns(int max_episodes)
{
    int n = 0;
    int limit = std::min(max_episodes, static_cast<int>(_planning_episodes.size()));
    for (int i = static_cast<int>(_planning_episodes.size()) - limit;
         i < static_cast<int>(_planning_episodes.size()); ++i) {
        if (i < 0) continue;
        const Handle& ep = _planning_episodes[static_cast<size_t>(i)];
        auto it = _episode_metrics.find(ep);
        if (it == _episode_metrics.end()) continue;
        updateStrategyPerformance(_current_strategy, it->second);
        ++n;
    }
    adaptPlanningStrategies();
    return n;
}

int MetaPlanner::applyOptimizations(const Handle& context_atom)
{
    auto before = _current_strategy;
    optimizePlanningStrategy(context_atom);
    return before != _current_strategy ? 1 : 0;
}

void MetaPlanner::configure(double learning_rate, double adaptation_threshold,
                            bool enable_temporal_optimization)
{
    _learning_rate = std::min(1.0, std::max(0.0, learning_rate));
    _adaptation_threshold = std::min(1.0, std::max(0.0, adaptation_threshold));
    _enable_temporal_optimization = enable_temporal_optimization;
}

void MetaPlanner::setReflectionInterval(std::chrono::milliseconds interval)
{
    _reflection_interval = interval;
}

void MetaPlanner::resetMetrics()
{
    _current_metrics.reset();
    _average_plan_quality = 0.0f;
    _plan_quality_samples = 0;
    _episode_metrics.clear();
    _planning_episodes.clear();
    for (auto& kv : _strategy_evaluations) {
        kv.second.metrics.reset();
        kv.second.effectiveness_score = 0.5;
        kv.second.sample_count = 0;
    }
}

MetaPlanner::StrategyEvaluation
MetaPlanner::getStrategyEvaluation(PlanningEngine::PlanningStrategy s) const
{
    auto it = _strategy_evaluations.find(s);
    if (it == _strategy_evaluations.end()) return StrategyEvaluation{};
    return it->second;
}

PlanningEngine::PlanResult MetaPlanner::planAndLearn(const Handle& goal_atom)
{
    if (!_initialized) initialize();
    if (!_engine) return PlanningEngine::PlanResult::NO_SOLUTION;

    auto strategy = optimizePlanningStrategy(goal_atom);
    if (_enable_temporal_optimization &&
        strategy != PlanningEngine::PlanningStrategy::TEMPORAL_FIRST) {
        // keep chosen strategy
    }

    auto result = _engine->createPlan(goal_atom, strategy);
    const PlanningEngine::Plan* plan = _engine->getPlan(goal_atom);
    if (plan) {
        auto metrics = _engine->computePlanQuality(*plan);
        recordPlanQuality(metrics, goal_atom);
        recordPlanningEpisode(
            plan->plan_atom,
            result == PlanningEngine::PlanResult::SUCCESS,
            metrics.planning_time);
    } else {
        recordPlanningEpisode(
            _atomspace->add_node(CONCEPT_NODE, "FailedEpisode"),
            false, std::chrono::milliseconds(0));
    }
    return result;
}

std::string MetaPlanner::objectiveToString(OptimizationObjective o)
{
    switch (o) {
        case OptimizationObjective::MINIMIZE_TIME: return "minimize_time";
        case OptimizationObjective::MINIMIZE_RESOURCES: return "minimize_resources";
        case OptimizationObjective::MAXIMIZE_SUCCESS: return "maximize_success";
        case OptimizationObjective::MINIMIZE_COMPLEXITY: return "minimize_complexity";
        case OptimizationObjective::BALANCED: return "balanced";
    }
    return "balanced";
}

std::string MetaPlanner::strategyToString(PlanningEngine::PlanningStrategy s)
{
    return PlanningEngine::strategyToString(s);
}
