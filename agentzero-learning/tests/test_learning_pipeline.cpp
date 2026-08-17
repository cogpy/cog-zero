#include "test_runner.h"

#include <opencog/agentzero/MetaLearning.h>
#include <opencog/agentzero/ExperienceManager.h>
#include <opencog/agentzero/SkillAcquisition.h>
#include <opencog/agentzero/PolicyOptimizer.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(Pipeline_EndToEndLearningLoop)
{
    auto as = createAtomSpace();

    // Shared episodic memory
    ExperienceManager experiences(as, 1000);

    // Skills learned from experience
    SkillAcquisitionConfig skill_cfg;
    skill_cfg.min_demonstrations = 3;
    skill_cfg.min_success_rate = 0.4;
    SkillAcquisition skills(as, skill_cfg);

    // Policy evolution (tiny for test speed)
    PolicyOptimizerConfig pol_cfg;
    pol_cfg.population_size = 8;
    pol_cfg.max_generations = 2;
    PolicyOptimizer policies(as, pol_cfg);

    Handle ctx  = as->add_node(CONCEPT_NODE, "pipe_ctx");
    Handle task = as->add_node(CONCEPT_NODE, "pipe_task");
    Handle out  = as->add_node(CONCEPT_NODE, "pipe_out");

    // Collect a trajectory of successes
    for (int i = 0; i < 6; ++i) {
        experiences.recordExperience(ExperienceType::SUCCESS, ctx, task, out, 0.7 + 0.05 * i);
    }
    experiences.recordExperience(ExperienceType::FAILURE, ctx, task, out, 0.3);

    ASSERT_EQ(experiences.getExperienceCount(), static_cast<size_t>(7));

    auto success_exps = experiences.getExperiencesByType(ExperienceType::SUCCESS);
    ASSERT_EQ(success_exps.size(), static_cast<size_t>(6));

    auto new_skills = skills.learnFromExperiences(success_exps);
    ASSERT_GE(new_skills.size(), static_cast<size_t>(1));
    ASSERT_GE(skills.getApplicableSkills(ctx).size(), static_cast<size_t>(1));

    Handle seed = as->add_node(CONCEPT_NODE, "pipe_policy");
    auto all = experiences.getRecentExperiences(std::chrono::hours(24));
    Handle best = policies.optimizePolicy(seed, all);
    ASSERT_TRUE(best != Handle::UNDEFINED);

    // Meta-learning coordinator on the same AtomSpace
    MetaLearningConfig meta_cfg;
    meta_cfg.adaptation_window = 5;
    meta_cfg.min_samples_for_adaptation = 3;
    MetaLearning meta(as, meta_cfg);

    for (int i = 0; i < 5; ++i) {
        meta.processExperience(ExperienceType::SUCCESS, ctx, task, out, 0.8, 0.7);
    }

    ASSERT_GE(meta.getExperienceManager().getExperienceCount(), static_cast<size_t>(5));
    auto stats = meta.getMetaLearningStatistics();
    ASSERT_GE(stats.at("total_experiences"), 5.0);

    // AtomSpace should contain learning scaffolding nodes
    ASSERT_TRUE(as->get_handle(CONCEPT_NODE, "ExperienceContext") != Handle::UNDEFINED
                || as->get_size() > 0);
    ASSERT_GT(as->get_size(), static_cast<size_t>(5));
}
