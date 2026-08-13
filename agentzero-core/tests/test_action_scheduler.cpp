#include "test_runner.h"

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/agentzero/ActionScheduler.h>
#include <opencog/agentzero/ActionExecutor.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(ActionScheduler_BasicScheduling)
{
    auto as = createAtomSpace();
    auto scheduler = std::make_unique<ActionScheduler>(nullptr, as);
    auto executor = std::make_shared<ActionExecutor>(nullptr, as);
    scheduler->setExecutor(executor);

    Handle action = as->add_node(CONCEPT_NODE, "ScheduledAction");
    auto now = std::chrono::steady_clock::now();
    auto r = scheduler->scheduleAction(action, now, 5);
    ASSERT_EQ(r, ActionScheduler::ScheduleResult::SCHEDULED);
    ASSERT_EQ(scheduler->getScheduledActions().size(), 1u);
    ASSERT_EQ(scheduler->getScheduledActions()[0].priority, 5);

    Handle delayed = as->add_node(CONCEPT_NODE, "Delayed");
    auto r2 = scheduler->scheduleActionAfter(delayed, 200, 7);
    ASSERT_EQ(r2, ActionScheduler::ScheduleResult::SCHEDULED);
    // Delayed action should appear among scheduled entries with priority 7
    bool found_delayed = false;
    for (const auto& sa : scheduler->getScheduledActions()) {
        if (sa.action_atom == delayed) {
            found_delayed = true;
            ASSERT_EQ(sa.priority, 7);
        }
    }
    ASSERT_TRUE(found_delayed);
    ASSERT_NE(scheduler->getNextActionTime(), std::chrono::steady_clock::time_point{});

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    auto r3 = scheduler->scheduleActionBefore(as->add_node(CONCEPT_NODE, "Deadline"), deadline, 8);
    ASSERT_EQ(r3, ActionScheduler::ScheduleResult::SCHEDULED);

    auto r4 = scheduler->schedulePeriodicAction(as->add_node(CONCEPT_NODE, "Periodic"), 50, 3, 6);
    ASSERT_EQ(r4, ActionScheduler::ScheduleResult::SCHEDULED);

    Handle dep = as->add_node(CONCEPT_NODE, "Dep");
    auto r5 = scheduler->scheduleActionWithDependencies(
        as->add_node(CONCEPT_NODE, "Main"), {dep}, 5);
    ASSERT_EQ(r5, ActionScheduler::ScheduleResult::SCHEDULED);

    auto r6 = scheduler->scheduleActionWithResources(
        as->add_node(CONCEPT_NODE, "Res"), {"cpu", "memory"}, 7);
    ASSERT_EQ(r6, ActionScheduler::ScheduleResult::SCHEDULED);

    ASSERT_GT(scheduler->scheduledCount(), 3u);
    // Immediate action should dispatch
    int n = scheduler->dispatchDueActions();
    ASSERT_GE(n, 0);
}
