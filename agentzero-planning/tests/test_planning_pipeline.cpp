#include "test_runner.h"

#include <opencog/agentzero/planning/GoalHierarchy.h>
#include <opencog/agentzero/planning/PlanningEngine.h>
#include <opencog/agentzero/planning/TemporalReasoner.h>
#include <opencog/agentzero/planning/SpaceTimeIntegrator.h>
#include <opencog/agentzero/planning/MetaPlanner.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::planning;

/**
 * End-to-end pipeline: goals → HTN/STRIPS plan → temporal schedule →
 * spacetime window → meta quality feedback.
 */
TEST(Pipeline_FullPlanningCycle)
{
    auto as = createAtomSpace();

    auto goals = std::make_shared<GoalHierarchy>(as);
    goals->initialize();

    Handle mission = as->add_node(CONCEPT_NODE, "mission");
    Handle prep = as->add_node(CONCEPT_NODE, "prepare");
    Handle deliver = as->add_node(CONCEPT_NODE, "deliver-package");
    ASSERT_TRUE(goals->addGoal(mission, Handle::UNDEFINED,
                               GoalHierarchy::GoalPriority::HIGH));
    ASSERT_TRUE(goals->addGoal(prep, mission));
    ASSERT_TRUE(goals->addGoal(deliver, mission));
    ASSERT_TRUE(goals->addGoalDependency(deliver, prep));

    auto pe = std::make_shared<PlanningEngine>(as);
    pe->initialize();
    pe->setGoalHierarchy(goals);

    // Domain
    PlanningEngine::StripsOperator gather;
    gather.name = "gather-kit";
    gather.add_effects = {"prepare"};
    pe->registerOperator(gather);

    PlanningEngine::StripsOperator navigate;
    navigate.name = "navigate";
    navigate.preconditions = {"prepare"};
    navigate.add_effects = {"at-destination"};
    pe->registerOperator(navigate);

    PlanningEngine::StripsOperator drop;
    drop.name = "drop-package";
    drop.preconditions = {"at-destination"};
    drop.add_effects = {"deliver-package"};
    pe->registerOperator(drop);

    PlanningEngine::HtnMethod prep_m;
    prep_m.name = "prep-method";
    prep_m.compound_task = "prepare";
    prep_m.subtasks = {"gather-kit"};
    pe->registerMethod(prep_m);

    PlanningEngine::HtnMethod del_m;
    del_m.name = "deliver-method";
    del_m.compound_task = "deliver-package";
    del_m.subtasks = {"navigate", "drop-package"};
    pe->registerMethod(del_m);

    // Satisfy prep dependency first via its own plan
    ASSERT_TRUE(goals->activateGoal(prep));
    auto r1 = pe->createPlan(prep, PlanningEngine::PlanningStrategy::HIERARCHICAL);
    ASSERT_EQ(r1, PlanningEngine::PlanResult::SUCCESS);
    pe->startExecution(prep);
    while (pe->getPlan(prep)->status == PlanningEngine::ExecutionStatus::EXECUTING)
        pe->stepExecution(prep);
    ASSERT_EQ(goals->getGoalStatus(prep), GoalHierarchy::GoalStatus::SATISFIED);
    ASSERT_TRUE(goals->areGoalDependenciesSatisfied(deliver));

    // Temporal + spacetime
    auto tr = std::make_shared<TemporalReasoner>(as);
    tr->initialize();
    pe->setTemporalReasoner(tr);

    SpaceTimeIntegrator sti(as);
    sti.initialize();
    Handle robot = as->add_node(CONCEPT_NODE, "delivery-robot");
    auto t0 = sti.getCurrentTime();
    SpaceTimeIntegrator::Trajectory traj;
    ASSERT_TRUE(sti.planTrajectory(robot, Point3{0, 0, 0}, Point3{10, 0, 0},
                                   t0, t0 + std::chrono::seconds(30), traj));
    ASSERT_GE(traj.size(), static_cast<size_t>(2));

    // Deliver plan with temporal strategy
    ASSERT_TRUE(goals->activateGoal(deliver));
    auto r2 = pe->createTemporalPlan(deliver, std::chrono::steady_clock::now() +
                                     std::chrono::minutes(5));
    ASSERT_EQ(r2, PlanningEngine::PlanResult::SUCCESS);
    const auto* dplan = pe->getPlan(deliver);
    ASSERT_TRUE(dplan != nullptr);
    ASSERT_FALSE(dplan->action_sequence.empty());

    // Meta quality loop
    MetaPlanner meta(as, pe);
    meta.initialize();
    auto q = pe->computePlanQuality(*dplan);
    meta.recordPlanQuality(q, mission);
    ASSERT_GE(meta.getPlanQualitySamples(), 1);

    // Execute delivery
    pe->startExecution(deliver);
    while (pe->getPlan(deliver)->status == PlanningEngine::ExecutionStatus::EXECUTING)
        pe->stepExecution(deliver);
    ASSERT_TRUE(pe->getFluent("deliver-package"));

    float mission_ach = goals->calculateHierarchicalAchievement(mission);
    ASSERT_GT(mission_ach, 0.5f);
}
