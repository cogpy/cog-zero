#include "test_runner.h"

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/agentzero/MetaPlanner.h>
#include <opencog/agentzero/TaskManager.h>
#include <opencog/agentzero/ActionScheduler.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(MetaPlanner_BasicLifecycle)
{
    auto as = createAtomSpace();
    MetaPlanner mp(nullptr, as);
    ASSERT_TRUE(mp.isInitialized());

    mp.setOptimizationObjective(MetaPlanner::OptimizationObjective::MINIMIZE_TIME);
    ASSERT_EQ(mp.getOptimizationObjective(), MetaPlanner::OptimizationObjective::MINIMIZE_TIME);

    Handle ctx = as->add_node(CONCEPT_NODE, "PlanContext");
    Handle analysis = mp.analyzePlanningEffectiveness(ctx);
    ASSERT_NE(analysis, Handle::UNDEFINED);

    auto strategy = mp.optimizePlanningStrategy(ctx);
    (void)strategy;

    mp.recordPlanningEpisode(as->add_node(CONCEPT_NODE, "Episode1"), true,
                             std::chrono::milliseconds(25));
    Handle reflection = mp.triggerReflection();
    ASSERT_NE(reflection, Handle::UNDEFINED);

    auto tm = std::make_shared<TaskManager>(nullptr, as);
    auto sched = std::make_shared<ActionScheduler>(nullptr, as);
    mp.setComponentReferences(tm, sched);
}
