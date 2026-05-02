/**
 * SkillAcquisition.h - Hierarchical Skill Learning for Agent-Zero
 * 
 * Part of AZ-LEARN-003: MOSES Policy Optimization Integration
 * Implements skill acquisition framework building on policy optimization
 * 
 * Copyright (C) 2024 OpenCog Foundation
 */

#ifndef AGENTZERO_SKILL_ACQUISITION_H
#define AGENTZERO_SKILL_ACQUISITION_H

#include <memory>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <mutex>

#include "LearningTypes.h"

namespace opencog {
namespace agentzero {
namespace learning {

// Forward declarations
class PolicyOptimizer;
class ExperienceManager;

/**
 * SkillAcquisition - Hierarchical skill learning framework
 * 
 * Builds on policy optimization to create and manage hierarchical skills:
 * - Automatic skill discovery from successful policy sequences
 * - Skill composition and decomposition
 * - Hierarchical skill organization
 * - Skill transfer and reuse
 */
class SkillAcquisition {
public:
    explicit SkillAcquisition(AtomSpacePtr atomspace,
                             std::shared_ptr<PolicyOptimizer> policy_optimizer,
                             std::shared_ptr<ExperienceManager> experience_manager,
                             const LearningConfig& config = LearningConfig{});
    
    ~SkillAcquisition();
    
    // Core skill acquisition methods
    std::shared_ptr<Skill> acquireSkill(const std::string& skill_name,
                                      const std::vector<PolicyId>& component_policies);
    
    std::vector<std::shared_ptr<Skill>> discoverSkillsFromExperience();
    
    bool composeSkill(const SkillId& new_skill_id,
                     const std::vector<SkillId>& component_skills);
    
    std::shared_ptr<Skill> getSkill(const SkillId& skill_id);
    std::vector<std::shared_ptr<Skill>> getAllSkills();
    
    // Skill management
    Handle storeSkillInAtomSpace(const Skill& skill);
    std::shared_ptr<Skill> retrieveSkillFromAtomSpace(const SkillId& skill_id);
    
private:
    AtomSpacePtr atomspace_;
    std::shared_ptr<PolicyOptimizer> policy_optimizer_;
    std::shared_ptr<ExperienceManager> experience_manager_;
    LearningConfig config_;
    
    std::map<SkillId, std::shared_ptr<Skill>> skill_cache_;
    mutable std::mutex skill_cache_mutex_;
};

} // namespace learning
} // namespace agentzero
} // namespace opencog

#endif // AGENTZERO_SKILL_ACQUISITION_H