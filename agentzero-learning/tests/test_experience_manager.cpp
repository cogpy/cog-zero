#include "test_runner.h"

#include <opencog/agentzero/ExperienceManager.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

#include <chrono>

using namespace opencog;
using namespace opencog::agentzero;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(ExperienceManager_Initialize)
{
    auto as = make_as();
    ExperienceManager mgr(as, /*max=*/100);
    ASSERT_TRUE(mgr.isInitialized());
    ASSERT_EQ(mgr.getExperienceCount(), static_cast<size_t>(0));
}

TEST(ExperienceManager_NullAtomSpaceThrows)
{
    bool threw = false;
    try {
        ExperienceManager mgr(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(ExperienceManager_RecordAndQueryByType)
{
    auto as = make_as();
    ExperienceManager mgr(as, 100);

    Handle ctx  = as->add_node(CONCEPT_NODE, "ctx");
    Handle task = as->add_node(CONCEPT_NODE, "task");
    Handle out  = as->add_node(CONCEPT_NODE, "out");

    Handle h1 = mgr.recordExperience(ExperienceType::SUCCESS, ctx, task, out, 0.9);
    Handle h2 = mgr.recordExperience(ExperienceType::FAILURE, ctx, task, out, 0.2);
    Handle h3 = mgr.recordExperience(ExperienceType::OBSERVATION, ctx, task, out, 0.5);

    ASSERT_TRUE(h1 != Handle::UNDEFINED);
    ASSERT_TRUE(h2 != Handle::UNDEFINED);
    ASSERT_TRUE(h3 != Handle::UNDEFINED);
    ASSERT_EQ(mgr.getExperienceCount(), static_cast<size_t>(3));

    auto successes = mgr.getExperiencesByType(ExperienceType::SUCCESS);
    ASSERT_EQ(successes.size(), static_cast<size_t>(1));
    ASSERT_EQ(successes[0].type, ExperienceType::SUCCESS);

    auto failures = mgr.getExperiencesByType(ExperienceType::FAILURE);
    ASSERT_EQ(failures.size(), static_cast<size_t>(1));
}

TEST(ExperienceManager_ContextAndRecent)
{
    auto as = make_as();
    ExperienceManager mgr(as, 100);

    Handle ctx_a = as->add_node(CONCEPT_NODE, "A");
    Handle ctx_b = as->add_node(CONCEPT_NODE, "B");
    Handle task  = as->add_node(CONCEPT_NODE, "t");
    Handle out   = as->add_node(CONCEPT_NODE, "o");

    mgr.recordExperience(ExperienceType::ACTION_OUTCOME, ctx_a, task, out, 0.7);
    mgr.recordExperience(ExperienceType::ACTION_OUTCOME, ctx_a, task, out, 0.6);
    mgr.recordExperience(ExperienceType::ACTION_OUTCOME, ctx_b, task, out, 0.5);

    auto by_a = mgr.getExperiencesByContext(ctx_a);
    ASSERT_EQ(by_a.size(), static_cast<size_t>(2));

    auto recent = mgr.getRecentExperiences(std::chrono::hours(1));
    ASSERT_EQ(recent.size(), static_cast<size_t>(3));
}

TEST(ExperienceManager_SimilarityAndImportance)
{
    auto as = make_as();
    ExperienceManager mgr(as, 100);

    Handle ctx  = as->add_node(CONCEPT_NODE, "sim_ctx");
    Handle task = as->add_node(CONCEPT_NODE, "sim_task");
    Handle out  = as->add_node(CONCEPT_NODE, "sim_out");

    Handle h = mgr.recordExperience(ExperienceType::SUCCESS, ctx, task, out, 0.5);
    mgr.recordExperience(ExperienceType::SUCCESS, ctx, task, out, 0.8);
    mgr.recordExperience(ExperienceType::DISCOVERY, ctx, task, out, 0.4);

    Experience target;
    target.type = ExperienceType::SUCCESS;
    target.context = ctx;
    target.task = task;
    auto similar = mgr.findSimilarExperiences(target, 5);
    ASSERT_GE(similar.size(), static_cast<size_t>(1));

    ASSERT_TRUE(mgr.updateExperienceImportance(h, 0.95));
    auto stats = mgr.getExperienceStatistics();
    ASSERT_EQ(stats.at("total_experiences"), 3.0);
    ASSERT_EQ(stats.at("success_count"), 2.0);
}

TEST(ExperienceManager_ConsolidationAndClear)
{
    auto as = make_as();
    ExperienceManager mgr(as, /*max=*/2, /*importance_threshold=*/0.5);

    Handle ctx  = as->add_node(CONCEPT_NODE, "c");
    Handle task = as->add_node(CONCEPT_NODE, "t");
    Handle out  = as->add_node(CONCEPT_NODE, "o");

    mgr.recordExperience(ExperienceType::SUCCESS, ctx, task, out, 0.9);
    mgr.recordExperience(ExperienceType::SUCCESS, ctx, task, out, 0.8);
    // Third record should trigger consolidation when over max
    mgr.recordExperience(ExperienceType::FAILURE, ctx, task, out, 0.1);

    ASSERT_LE(mgr.getExperienceCount(), static_cast<size_t>(2));

    mgr.clearAllExperiences();
    ASSERT_EQ(mgr.getExperienceCount(), static_cast<size_t>(0));
}

TEST(ExperienceManager_TypeStringRoundTrip)
{
    ASSERT_STREQ(ExperienceManager::experienceTypeToString(ExperienceType::SUCCESS).c_str(),
                 "SUCCESS");
    ASSERT_EQ(ExperienceManager::stringToExperienceType("FAILURE"), ExperienceType::FAILURE);
    ASSERT_EQ(ExperienceManager::stringToExperienceType("nope"), ExperienceType::OBSERVATION);
}
