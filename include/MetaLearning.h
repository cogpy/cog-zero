/*
 * opencog/agentzero/MetaLearning.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * MetaLearning — learning-to-learn improvements
 * Part of Phase 5: Continuous Learning & Adaptation
 * Part of the AGENT-ZERO-GENESIS project
 */

#ifndef _OPENCOG_AGENTZERO_META_LEARNING_H
#define _OPENCOG_AGENTZERO_META_LEARNING_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/ExperienceManager.h"
#include "opencog/agentzero/SkillAcquisition.h"
#include "opencog/agentzero/PolicyOptimizer.h"

namespace opencog {
namespace agentzero {

/**
 * Strategy used for a learning episode
 */
enum class LearningStrategy {
    SUPERVISED,       ///< Learn from labelled examples
    UNSUPERVISED,     ///< Find structure without labels
    REINFORCEMENT,    ///< Learn from reward signals
    IMITATION,        ///< Learn by copying demonstrations
    EXPLORATION,      ///< Explore the environment to gather data
    META_ADAPTIVE     ///< Dynamically select the best strategy
};

/**
 * Configuration for the meta-learning system
 */
struct MetaLearningConfig {
    double  adaptation_threshold;        ///< Minimum improvement to trigger adaptation
    int     min_samples_for_adaptation;  ///< Episodes needed before adapting strategy
    bool    enable_strategy_transfer;    ///< Allow strategy transfer across tasks
    double  learning_rate;               ///< Base learning rate for parameter updates
    int     adaptation_window;           ///< Number of recent episodes to consider

    MetaLearningConfig()
        : adaptation_threshold(0.1)
        , min_samples_for_adaptation(5)
        , enable_strategy_transfer(true)
        , learning_rate(0.01)
        , adaptation_window(20)
    {}
};

/**
 * Performance snapshot used for strategy selection
 */
struct LearningPerformance {
    LearningStrategy strategy;  ///< Strategy that produced this performance
    double success_rate;        ///< Fraction of successful episodes
    double avg_reward;          ///< Average reward per episode
    double learning_speed;      ///< Improvement rate (reward delta per episode)
    int    episode_count;       ///< Number of episodes this strategy was used

    LearningPerformance()
        : strategy(LearningStrategy::EXPLORATION)
        , success_rate(0.0)
        , avg_reward(0.0)
        , learning_speed(0.0)
        , episode_count(0)
    {}
};

/**
 * MetaLearning — coordinates the agent's learning sub-systems and
 * improves their effectiveness over time (learning-to-learn)
 *
 * Maintains and selects among multiple learning strategies, adapts
 * hyper-parameters based on observed performance, and integrates the
 * ExperienceManager, SkillAcquisition, and PolicyOptimizer components.
 *
 * Key features:
 * - Dynamic learning-strategy selection and switching
 * - Hyper-parameter adaptation based on recent performance
 * - Strategy transfer across similar tasks
 * - Coordination of all Phase 5 learning components
 * - AtomSpace-backed persistent meta-learning state
 */
class MetaLearning
{
public:
    /**
     * Construct a MetaLearning coordinator.
     * @param atomspace  Shared AtomSpace for all sub-components.
     * @param config     Meta-learning configuration (optional).
     */
    explicit MetaLearning(AtomSpacePtr atomspace,
                          const MetaLearningConfig& config = MetaLearningConfig());

    virtual ~MetaLearning();

    // ----------------------------------------------------------------
    // Core meta-learning cycle
    // ----------------------------------------------------------------

    /**
     * Run one meta-learning update cycle.
     * Analyses recent experience, selects/adapts strategies, updates
     * SkillAcquisition and PolicyOptimizer as needed.
     * @return True if any adaptation was performed.
     */
    bool update();

    /**
     * Adapt learning hyper-parameters based on the performance window.
     * @return True if any parameter was changed.
     */
    bool adaptLearningParameters();

    /**
     * Select the best strategy for the current task context.
     * @param task_context  Atom representing the current task.
     * @return Selected LearningStrategy.
     */
    LearningStrategy selectStrategy(const Handle& task_context) const;

    // ----------------------------------------------------------------
    // Experience integration
    // ----------------------------------------------------------------

    /**
     * Process a new experience through all learning sub-systems.
     * Calls ExperienceManager::recordExperience, then triggers
     * SkillAcquisition and PolicyOptimizer updates when thresholds are met.
     * @param type        Category of the experience.
     * @param context     Context atom.
     * @param task        Task atom.
     * @param outcome     Outcome atom.
     * @param reward      Scalar reward signal.
     * @param importance  Storage importance in [0,1].
     * @return Handle of the stored experience.
     */
    Handle processExperience(ExperienceType type,
                             const Handle& context,
                             const Handle& task,
                             const Handle& outcome,
                             double reward,
                             double importance = 0.5);

    // ----------------------------------------------------------------
    // Strategy management
    // ----------------------------------------------------------------

    /**
     * Record the performance of a strategy after an episode.
     * @param strategy    Strategy that was used.
     * @param success     Whether the episode succeeded.
     * @param reward      Reward obtained.
     */
    void recordStrategyPerformance(LearningStrategy strategy, bool success, double reward);

    /**
     * Return performance statistics for every tracked strategy.
     */
    std::map<LearningStrategy, LearningPerformance> getStrategyPerformances() const
    { return _strategy_performances; }

    /**
     * Return the strategy currently in use.
     */
    LearningStrategy getCurrentStrategy() const { return _current_strategy; }

    // ----------------------------------------------------------------
    // Sub-component accessors
    // ----------------------------------------------------------------
    ExperienceManager& getExperienceManager() { return *_experience_manager; }
    SkillAcquisition&  getSkillAcquisition()  { return *_skill_acquisition; }
    PolicyOptimizer&   getPolicyOptimizer()   { return *_policy_optimizer; }

    const ExperienceManager& getExperienceManager() const { return *_experience_manager; }
    const SkillAcquisition&  getSkillAcquisition()  const { return *_skill_acquisition; }
    const PolicyOptimizer&   getPolicyOptimizer()   const { return *_policy_optimizer; }

    // ----------------------------------------------------------------
    // Statistics and diagnostics
    // ----------------------------------------------------------------

    /**
     * Aggregate statistics about the meta-learning system.
     * @return Key/value map of statistic names to values.
     */
    std::map<std::string, double> getMetaLearningStatistics() const;

    // ----------------------------------------------------------------
    // Configuration
    // ----------------------------------------------------------------
    void setConfig(const MetaLearningConfig& config) { _config = config; }
    const MetaLearningConfig& getConfig() const { return _config; }

    // Utility
    static std::string strategyToString(LearningStrategy s);

private:
    AtomSpacePtr      _atomspace;
    MetaLearningConfig _config;

    // Sub-components
    std::unique_ptr<ExperienceManager> _experience_manager;
    std::unique_ptr<SkillAcquisition>  _skill_acquisition;
    std::unique_ptr<PolicyOptimizer>   _policy_optimizer;

    // Strategy tracking
    LearningStrategy _current_strategy;
    std::map<LearningStrategy, LearningPerformance> _strategy_performances;

    // AtomSpace housekeeping
    Handle _meta_learning_base;

    // Internal counters for triggering adaptation
    int _episodes_since_last_adaptation;
    int _episodes_since_skill_update;
    int _episodes_since_policy_update;

    // Internal helpers
    void   initializeMetaLearningBase();
    void   initializeSubComponents();
    double computeStrategyScore(const LearningStrategy& strategy) const;
    void   triggerSkillUpdate();
    void   triggerPolicyUpdate();
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_META_LEARNING_H
