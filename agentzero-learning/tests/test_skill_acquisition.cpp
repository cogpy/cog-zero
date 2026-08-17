#include "test_runner.h"

#include <opencog/agentzero/SkillAcquisition.h>
#include <opencog/agentzero/ExperienceManager.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;

static AtomSpacePtr make_as() { return createAtomSpace(); }

static std::vector<Experience> make_success_batch(AtomSpacePtr as, int n)
{
    Handle ctx  = as->add_node(CONCEPT_NODE, "skill_ctx");
    Handle task = as->add_node(CONCEPT_NODE, "skill_task");
    Handle out  = as->add_node(CONCEPT_NODE, "skill_out");

    std::vector<Experience> exps;
    for (int i = 0; i < n; ++i) {
        Experience e;
        e.type = ExperienceType::SUCCESS;
        e.context = ctx;
        e.task = task;
        e.outcome = out;
        e.importance = 0.8;
        e.timestamp = std::chrono::system_clock::now();
        exps.push_back(e);
    }
    return exps;
}

TEST(SkillAcquisition_Initialize)
{
    auto as = make_as();
    SkillAcquisition sa(as);
    ASSERT_EQ(sa.getSkillCount(), static_cast<size_t>(0));
}

TEST(SkillAcquisition_NullAtomSpaceThrows)
{
    bool threw = false;
    try {
        SkillAcquisition sa(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(SkillAcquisition_LearnFromExperiences)
{
    auto as = make_as();
    SkillAcquisitionConfig cfg;
    cfg.min_demonstrations = 3;
    cfg.min_success_rate = 0.5;
    SkillAcquisition sa(as, cfg);

    auto exps = make_success_batch(as, 5);
    auto handles = sa.learnFromExperiences(exps);
    ASSERT_GE(handles.size(), static_cast<size_t>(1));
    ASSERT_GE(sa.getSkillCount(), static_cast<size_t>(1));

    auto all = sa.getAllSkills();
    ASSERT_FALSE(all.empty());
    ASSERT_GT(all[0].success_rate, 0.0);

    Skill fetched = sa.getSkill(handles[0]);
    ASSERT_TRUE(fetched.id != Handle::UNDEFINED);
}

TEST(SkillAcquisition_RefineAndTransfer)
{
    auto as = make_as();
    SkillAcquisitionConfig cfg;
    cfg.min_demonstrations = 3;
    cfg.enable_skill_transfer = true;
    SkillAcquisition sa(as, cfg);

    auto handles = sa.learnFromExperiences(make_success_batch(as, 4));
    ASSERT_FALSE(handles.empty());

    Handle ctx2 = as->add_node(CONCEPT_NODE, "new_ctx");
    sa.refineSkill(handles[0], true, ctx2);
    Skill refined = sa.getSkill(handles[0]);
    ASSERT_EQ(refined.application_count, 1);

    Handle transferred = sa.transferSkill(handles[0], ctx2);
    ASSERT_TRUE(transferred != Handle::UNDEFINED);
    ASSERT_GE(sa.getSkillCount(), static_cast<size_t>(2));
}

TEST(SkillAcquisition_ApplicableSkills)
{
    auto as = make_as();
    SkillAcquisitionConfig cfg;
    cfg.min_demonstrations = 3;
    SkillAcquisition sa(as, cfg);

    auto exps = make_success_batch(as, 4);
    sa.learnFromExperiences(exps);

    Handle ctx = exps[0].context;
    auto applicable = sa.getApplicableSkills(ctx, 10);
    ASSERT_GE(applicable.size(), static_cast<size_t>(1));

    auto stats = sa.getSkillStatistics();
    ASSERT_GE(stats.at("total_skills"), 1.0);
}

TEST(SkillAcquisition_TooFewDemonstrations)
{
    auto as = make_as();
    SkillAcquisitionConfig cfg;
    cfg.min_demonstrations = 5;
    SkillAcquisition sa(as, cfg);

    auto handles = sa.learnFromExperiences(make_success_batch(as, 2));
    ASSERT_EQ(handles.size(), static_cast<size_t>(0));
    ASSERT_EQ(sa.getSkillCount(), static_cast<size_t>(0));
}
