/*
 * src/MetaLearning.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * MetaLearning — learning-to-learn improvements
 * Part of Phase 5: Continuous Learning & Adaptation
 * Part of the AGENT-ZERO-GENESIS project
 */

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/MetaLearning.h"

using namespace opencog;
using namespace opencog::agentzero;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

MetaLearning::MetaLearning(AtomSpacePtr atomspace, const MetaLearningConfig& config)
    : _atomspace(atomspace)
    , _config(config)
    , _current_strategy(LearningStrategy::EXPLORATION)
    , _meta_learning_base(Handle::UNDEFINED)
    , _episodes_since_last_adaptation(0)
    , _episodes_since_skill_update(0)
    , _episodes_since_policy_update(0)
{
    if (!_atomspace) {
        throw std::runtime_error("[MetaLearning] AtomSpace pointer must not be null");
    }
    logger().info() << "[MetaLearning] Initialising meta-learning system";
    initializeMetaLearningBase();
    initializeSubComponents();
    logger().info() << "[MetaLearning] Meta-learning system ready";
}

MetaLearning::~MetaLearning()
{
    logger().info() << "[MetaLearning] Shutting down meta-learning system";
}

// ---------------------------------------------------------------------------
// Internal initialisation
// ---------------------------------------------------------------------------

void MetaLearning::initializeMetaLearningBase()
{
    _meta_learning_base = _atomspace->add_node(CONCEPT_NODE, "MetaLearningSystem");
    logger().info() << "[MetaLearning] Meta-learning base node created in AtomSpace";
}

void MetaLearning::initializeSubComponents()
{
    _experience_manager = std::make_unique<ExperienceManager>(_atomspace);
    _skill_acquisition  = std::make_unique<SkillAcquisition>(_atomspace);
    _policy_optimizer   = std::make_unique<PolicyOptimizer>(_atomspace);

    // Initialise performance records for every strategy
    for (auto strategy : {LearningStrategy::SUPERVISED,
                           LearningStrategy::UNSUPERVISED,
                           LearningStrategy::REINFORCEMENT,
                           LearningStrategy::IMITATION,
                           LearningStrategy::EXPLORATION,
                           LearningStrategy::META_ADAPTIVE}) {
        LearningPerformance perf;
        perf.strategy = strategy;
        _strategy_performances[strategy] = perf;
    }

    logger().info() << "[MetaLearning] Sub-components (ExperienceManager, "
                       "SkillAcquisition, PolicyOptimizer) initialised";
}

// ---------------------------------------------------------------------------
// Core meta-learning cycle
// ---------------------------------------------------------------------------

bool MetaLearning::update()
{
    ++_episodes_since_last_adaptation;
    ++_episodes_since_skill_update;
    ++_episodes_since_policy_update;

    bool adapted = false;

    // Periodically trigger skill learning from accumulated experiences
    if (_episodes_since_skill_update >= _config.adaptation_window) {
        triggerSkillUpdate();
        _episodes_since_skill_update = 0;
        adapted = true;
    }

    // Periodically trigger policy optimisation
    if (_episodes_since_policy_update >= _config.adaptation_window * 2) {
        triggerPolicyUpdate();
        _episodes_since_policy_update = 0;
        adapted = true;
    }

    // Adapt learning parameters once we have enough samples
    if (_episodes_since_last_adaptation >= _config.min_samples_for_adaptation) {
        if (adaptLearningParameters()) {
            _episodes_since_last_adaptation = 0;
            adapted = true;
        }
    }

    return adapted;
}

bool MetaLearning::adaptLearningParameters()
{
    // Select best strategy based on recent performance scores
    LearningStrategy best = _current_strategy;
    double best_score = computeStrategyScore(_current_strategy);

    for (const auto& [strategy, perf] : _strategy_performances) {
        if (perf.episode_count < _config.min_samples_for_adaptation) continue;
        double score = computeStrategyScore(strategy);
        if (score > best_score + _config.adaptation_threshold) {
            best_score = score;
            best       = strategy;
        }
    }

    if (best != _current_strategy) {
        logger().info() << "[MetaLearning] Switching strategy from "
                        << strategyToString(_current_strategy)
                        << " to " << strategyToString(best)
                        << " (score improvement: "
                        << (best_score - computeStrategyScore(_current_strategy)) << ")";
        _current_strategy = best;
        return true;
    }
    return false;
}

LearningStrategy MetaLearning::selectStrategy(const Handle& task_context) const
{
    if (_current_strategy == LearningStrategy::META_ADAPTIVE) {
        // Dynamic selection: pick strategy with best recent score
        LearningStrategy best = LearningStrategy::EXPLORATION;
        double best_score = -1.0;
        for (const auto& [strategy, perf] : _strategy_performances) {
            if (perf.episode_count == 0) continue;
            double score = computeStrategyScore(strategy);
            if (score > best_score) {
                best_score = score;
                best       = strategy;
            }
        }
        return best;
    }
    return _current_strategy;
}

double MetaLearning::computeStrategyScore(const LearningStrategy& strategy) const
{
    auto it = _strategy_performances.find(strategy);
    if (it == _strategy_performances.end()) return 0.0;
    const LearningPerformance& perf = it->second;
    if (perf.episode_count == 0) return 0.0;

    // Combined score: 60% success rate, 30% avg reward, 10% learning speed
    return 0.6 * perf.success_rate
         + 0.3 * std::max(0.0, perf.avg_reward)
         + 0.1 * std::max(0.0, perf.learning_speed);
}

// ---------------------------------------------------------------------------
// Experience integration
// ---------------------------------------------------------------------------

Handle MetaLearning::processExperience(ExperienceType type,
                                        const Handle& context,
                                        const Handle& task,
                                        const Handle& outcome,
                                        double reward,
                                        double importance)
{
    // Adjust importance based on reward signal
    double adjusted_importance = std::max(0.1, std::min(1.0,
        importance * (1.0 + std::abs(reward) * 0.5)));

    Handle exp_handle = _experience_manager->recordExperience(
        type, context, task, outcome, adjusted_importance);

    // Update strategy performance tracking
    bool success = (type == ExperienceType::SUCCESS)
                || (reward > 0.0 && type == ExperienceType::ACTION_OUTCOME);

    recordStrategyPerformance(_current_strategy, success, reward);

    // Trigger the meta-learning update cycle
    update();

    return exp_handle;
}

// ---------------------------------------------------------------------------
// Strategy performance
// ---------------------------------------------------------------------------

void MetaLearning::recordStrategyPerformance(LearningStrategy strategy,
                                              bool success,
                                              double reward)
{
    auto& perf = _strategy_performances[strategy];
    ++perf.episode_count;

    // Exponential moving average
    double alpha = _config.learning_rate;
    double success_signal = success ? 1.0 : 0.0;

    if (perf.episode_count == 1) {
        perf.success_rate    = success_signal;
        perf.avg_reward      = reward;
        perf.learning_speed  = 0.0;
    } else {
        double old_reward    = perf.avg_reward;
        perf.success_rate    = (1.0 - alpha) * perf.success_rate + alpha * success_signal;
        perf.avg_reward      = (1.0 - alpha) * perf.avg_reward   + alpha * reward;
        perf.learning_speed  = (1.0 - alpha) * perf.learning_speed
                               + alpha * (perf.avg_reward - old_reward);
    }
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void MetaLearning::triggerSkillUpdate()
{
    logger().info() << "[MetaLearning] Triggering skill update from recent experiences";

    auto recent = _experience_manager->getRecentExperiences(
        std::chrono::hours(_config.adaptation_window * 24));

    if (!recent.empty()) {
        auto new_skills = _skill_acquisition->learnFromExperiences(recent);
        logger().info() << "[MetaLearning] Skill update: "
                        << new_skills.size() << " new skill(s) acquired";
    }
}

void MetaLearning::triggerPolicyUpdate()
{
    logger().info() << "[MetaLearning] Triggering policy optimisation";

    auto experiences = _experience_manager->getRecentExperiences(
        std::chrono::hours(_config.adaptation_window * 48));

    if (!experiences.empty()) {
        Handle current_best = _policy_optimizer->getBestPolicy();
        if (current_best == Handle::UNDEFINED) {
            // Bootstrap: create a placeholder policy anchored to PolicyOptimizer's space
            current_best = _atomspace->add_node(CONCEPT_NODE, "InitialPolicy");
            Handle policy_base = _policy_optimizer->getPolicyBase();
            if (policy_base != Handle::UNDEFINED) {
                _atomspace->add_link(MEMBER_LINK, current_best, policy_base);
            }
        }

        Handle improved = _policy_optimizer->optimizePolicy(current_best, experiences);
        if (improved != Handle::UNDEFINED && improved != current_best) {
            logger().info() << "[MetaLearning] Policy updated via optimiser";
        }
    }
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

std::map<std::string, double> MetaLearning::getMetaLearningStatistics() const
{
    std::map<std::string, double> stats;

    stats["current_strategy"]           = static_cast<double>(_current_strategy);
    stats["total_experiences"]          =
        static_cast<double>(_experience_manager->getExperienceCount());
    stats["total_skills"]               =
        static_cast<double>(_skill_acquisition->getSkillCount());
    stats["episodes_since_adaptation"]  =
        static_cast<double>(_episodes_since_last_adaptation);

    for (const auto& [strategy, perf] : _strategy_performances) {
        std::string prefix = strategyToString(strategy) + "_";
        stats[prefix + "success_rate"]   = perf.success_rate;
        stats[prefix + "avg_reward"]     = perf.avg_reward;
        stats[prefix + "learning_speed"] = perf.learning_speed;
        stats[prefix + "episode_count"]  = static_cast<double>(perf.episode_count);
    }

    return stats;
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

std::string MetaLearning::strategyToString(LearningStrategy s)
{
    switch (s) {
        case LearningStrategy::SUPERVISED:    return "SUPERVISED";
        case LearningStrategy::UNSUPERVISED:  return "UNSUPERVISED";
        case LearningStrategy::REINFORCEMENT: return "REINFORCEMENT";
        case LearningStrategy::IMITATION:     return "IMITATION";
        case LearningStrategy::EXPLORATION:   return "EXPLORATION";
        case LearningStrategy::META_ADAPTIVE: return "META_ADAPTIVE";
        default:                              return "UNKNOWN";
    }
}
