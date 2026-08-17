#include "test_runner.h"

#include <opencog/agentzero/planning/MetaPlanner.h>
#include <opencog/agentzero/planning/PlanningEngine.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::planning;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(MetaPlanner_Initialize)
{
    auto as = make_as();
    MetaPlanner mp(as);
    ASSERT_TRUE(mp.initialize());
    ASSERT_TRUE(mp.isInitialized());
    ASSERT_EQ(mp.getOptimizationObjective(), MetaPlanner::OptimizationObjective::BALANCED);
}

TEST(MetaPlanner_RecordQualityAndAdapt)
{
    auto as = make_as();
    MetaPlanner mp(as);
    mp.initialize();
    mp.configure(0.5, 0.9, true); // high threshold → force adapt on low quality

    MetaPlanner::PlanQualityReport report;
    report.optimality_score = 0.1f;
    report.goal_coverage = 0.1f;
    report.temporal_satisfaction = 0.1f;
    report.resource_efficiency = 0.1f;
    report.robustness_score = 0.1f;
    report.action_count = 10;
    report.planning_time = std::chrono::milliseconds(50);

    Handle ctx = as->add_node(CONCEPT_NODE, "ctx");
    mp.recordPlanQuality(report, ctx);
    ASSERT_EQ(mp.getPlanQualitySamples(), 1);
    ASSERT_LT(mp.getAveragePlanQuality(), 0.5f);

    // good quality improves average
    report.optimality_score = report.goal_coverage = report.temporal_satisfaction = 1.0f;
    report.resource_efficiency = report.robustness_score = 1.0f;
    mp.recordPlanQuality(report, ctx);
    ASSERT_EQ(mp.getPlanQualitySamples(), 2);
    ASSERT_GT(mp.getAveragePlanQuality(), 0.1f);
}

TEST(MetaPlanner_PlanAndLearn)
{
    auto as = make_as();
    auto pe = std::make_shared<PlanningEngine>(as);
    pe->initialize();

    PlanningEngine::StripsOperator go;
    go.name = "do-it";
    go.add_effects = {"done"};
    pe->registerOperator(go);

    PlanningEngine::HtnMethod m;
    m.name = "finish-method";
    m.compound_task = "finish";
    m.subtasks = {"do-it"};
    pe->registerMethod(m);

    MetaPlanner mp(as, pe);
    mp.initialize();

    Handle goal = as->add_node(CONCEPT_NODE, "finish");
    auto result = mp.planAndLearn(goal);
    ASSERT_EQ(result, PlanningEngine::PlanResult::SUCCESS);
    ASSERT_GE(mp.getPlanQualitySamples(), 1);
    ASSERT_GT(mp.getAveragePlanQuality(), 0.0f);

    Handle reflection = mp.triggerReflection();
    ASSERT_NE(reflection, Handle::UNDEFINED);
}

TEST(MetaPlanner_StrategySelection)
{
    auto as = make_as();
    MetaPlanner mp(as);
    mp.initialize();
    mp.setOptimizationObjective(MetaPlanner::OptimizationObjective::MAXIMIZE_SUCCESS);

    Handle ctx = as->add_node(CONCEPT_NODE, "select_ctx");
    auto s = mp.optimizePlanningStrategy(ctx);
    // default prior prefers HYBRID
    ASSERT_EQ(s, PlanningEngine::PlanningStrategy::HYBRID);

    mp.recordPlanningEpisode(as->add_node(CONCEPT_NODE, "ep1"), true,
                             std::chrono::milliseconds(10));
    auto analysis = mp.analyzePlanningEffectiveness(ctx);
    ASSERT_NE(analysis, Handle::UNDEFINED);
}
