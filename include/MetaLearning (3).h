/**
 * MetaLearning.h - Meta-Learning Capabilities for Agent-Zero
 * 
 * Part of AZ-LEARN-003: MOSES Policy Optimization Integration
 * Implements learning how to learn more effectively
 * 
 * Copyright (C) 2024 OpenCog Foundation
 */

#ifndef AGENTZERO_META_LEARNING_H
#define AGENTZERO_META_LEARNING_H

#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <mutex>

#include "LearningTypes.h"

namespace opencog {
namespace agentzero {
namespace learning {

// Forward declarations
class PolicyOptimizer;
class ExperienceManager;
class SkillAcquisition;

/**
 * MetaLearning - Learning how to learn more effectively
 * 
 * Implements meta-learning capabilities:
 * - Learning rate adaptation
 * - Hyperparameter optimization
 * - Transfer learning strategies
 * - Learning algorithm selection
 */
class MetaLearning {
public:
    explicit MetaLearning(AtomSpacePtr atomspace,
                         std::shared_ptr<PolicyOptimizer> policy_optimizer,
                         std::shared_ptr<ExperienceManager> experience_manager,
                         std::shared_ptr<SkillAcquisition> skill_acquisition,
                         const LearningConfig& config = LearningConfig{});
    
    ~MetaLearning();
    
    // Meta-learning methods
    void adaptLearningParameters();
    void optimizeHyperparameters();
    void updateLearningStrategy();
    
    std::map<std::string, double> getMetaLearningStats() const;

private:
    AtomSpacePtr atomspace_;
    std::shared_ptr<PolicyOptimizer> policy_optimizer_;
    std::shared_ptr<ExperienceManager> experience_manager_;
    std::shared_ptr<SkillAcquisition> skill_acquisition_;
    LearningConfig config_;
    
    mutable std::mutex stats_mutex_;
};

} // namespace learning
} // namespace agentzero
} // namespace opencog

#endif // AGENTZERO_META_LEARNING_H