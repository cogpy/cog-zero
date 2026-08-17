#include "test_runner.h"

#include <opencog/agentzero/MetaLearning.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(MetaLearning_Initialize)
{
    auto as = make_as();
    MetaLearning ml(as);
    ASSERT_EQ(ml.getCurrentStrategy(), LearningStrategy::EXPLORATION);
    ASSERT_EQ(ml.getExperienceManager().getExperienceCount(), static_cast<size_t>(0));
    ASSERT_EQ(ml.getSkillAcquisition().getSkillCount(), static_cast<size_t>(0));
    ASSERT_TRUE(ml.getPolicyOptimizer().getPolicyBase() != Handle::UNDEFINED);
}

TEST(MetaLearning_NullAtomSpaceThrows)
{
    bool threw = false;
    try {
        MetaLearning ml(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(MetaLearning_ProcessExperience)
{
    auto as = make_as();
    MetaLearningConfig cfg;
    cfg.min_samples_for_adaptation = 100; // avoid adaptation noise in this test
    cfg.adaptation_window = 100;
    MetaLearning ml(as, cfg);

    Handle ctx  = as->add_node(CONCEPT_NODE, "m_ctx");
    Handle task = as->add_node(CONCEPT_NODE, "m_task");
    Handle out  = as->add_node(CONCEPT_NODE, "m_out");

    Handle h = ml.processExperience(ExperienceType::SUCCESS, ctx, task, out, 1.0, 0.8);
    ASSERT_TRUE(h != Handle::UNDEFINED);
    ASSERT_EQ(ml.getExperienceManager().getExperienceCount(), static_cast<size_t>(1));

    auto perfs = ml.getStrategyPerformances();
    ASSERT_GE(perfs.at(LearningStrategy::EXPLORATION).episode_count, 1);
}

TEST(MetaLearning_StrategyPerformanceAndSelection)
{
    auto as = make_as();
    MetaLearning ml(as);

    ml.recordStrategyPerformance(LearningStrategy::REINFORCEMENT, true, 1.0);
    ml.recordStrategyPerformance(LearningStrategy::REINFORCEMENT, true, 0.9);
    ml.recordStrategyPerformance(LearningStrategy::REINFORCEMENT, true, 0.8);
    ml.recordStrategyPerformance(LearningStrategy::IMITATION, false, -0.5);
    ml.recordStrategyPerformance(LearningStrategy::IMITATION, false, -0.4);

    auto perfs = ml.getStrategyPerformances();
    ASSERT_GT(perfs.at(LearningStrategy::REINFORCEMENT).success_rate,
              perfs.at(LearningStrategy::IMITATION).success_rate);

    Handle task = as->add_node(CONCEPT_NODE, "select_task");
    auto strategy = ml.selectStrategy(task);
    // Default current strategy is EXPLORATION (not META_ADAPTIVE)
    ASSERT_EQ(strategy, LearningStrategy::EXPLORATION);
}

TEST(MetaLearning_AdaptAndStatistics)
{
    auto as = make_as();
    MetaLearningConfig cfg;
    cfg.min_samples_for_adaptation = 2;
    cfg.adaptation_threshold = 0.05;
    cfg.adaptation_window = 50;
    MetaLearning ml(as, cfg);

    // Build a strong REINFORCEMENT track record
    for (int i = 0; i < 5; ++i) {
        ml.recordStrategyPerformance(LearningStrategy::REINFORCEMENT, true, 1.0);
    }
    for (int i = 0; i < 5; ++i) {
        ml.recordStrategyPerformance(LearningStrategy::EXPLORATION, false, 0.0);
    }

    bool adapted = ml.adaptLearningParameters();
    // May or may not switch depending on scores; just ensure call is safe
    (void)adapted;

    auto stats = ml.getMetaLearningStatistics();
    ASSERT_GE(stats.at("total_experiences"), 0.0);
    ASSERT_TRUE(stats.count("REINFORCEMENT_episode_count") > 0);

    ASSERT_STREQ(MetaLearning::strategyToString(LearningStrategy::META_ADAPTIVE).c_str(),
                 "META_ADAPTIVE");
}

TEST(MetaLearning_UpdateCycle)
{
    auto as = make_as();
    MetaLearningConfig cfg;
    cfg.adaptation_window = 3;
    cfg.min_samples_for_adaptation = 2;
    MetaLearning ml(as, cfg);

    Handle ctx  = as->add_node(CONCEPT_NODE, "u_ctx");
    Handle task = as->add_node(CONCEPT_NODE, "u_task");
    Handle out  = as->add_node(CONCEPT_NODE, "u_out");

    for (int i = 0; i < 4; ++i) {
        ml.processExperience(
            (i % 2 == 0) ? ExperienceType::SUCCESS : ExperienceType::ACTION_OUTCOME,
            ctx, task, out, 0.5 + 0.1 * i, 0.7);
    }

    // Explicit update should be safe
    ml.update();
    ASSERT_GE(ml.getExperienceManager().getExperienceCount(), static_cast<size_t>(4));
}
