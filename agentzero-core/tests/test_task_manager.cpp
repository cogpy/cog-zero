#include "test_runner.h"

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/agentzero/TaskManager.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(TaskManager_GoalsAndTasks)
{
    auto as = createAtomSpace();
    TaskManager tm(nullptr, as);

    Handle goal = tm.setGoal("learn", "learn opencog", 0.9);
    ASSERT_NE(goal, Handle::UNDEFINED);
    ASSERT_EQ(tm.getGoalCount(), 1u);
    ASSERT_EQ(tm.getCurrentGoalAtom(), goal);

    bool ran = false;
    Handle task = tm.createTask("read-docs", "read docs", TaskManager::Priority::HIGH,
                                [&]() { ran = true; return true; });
    ASSERT_NE(task, Handle::UNDEFINED);
    ASSERT_EQ(tm.getPendingTaskCount(), 1u);

    // Goal/task represented with EvaluationLink + StateLink
    ASSERT_GT(as->get_handles_by_type(EVALUATION_LINK).size(), 0u);
    ASSERT_GT(as->get_handles_by_type(STATE_LINK).size(), 0u);

    ASSERT_TRUE(tm.executeNext());
    ASSERT_TRUE(ran);
    ASSERT_EQ(tm.getPendingTaskCount(), 0u);

    ASSERT_TRUE(tm.achieveGoal(1));
    ASSERT_TRUE(tm.processTaskManagement());
}
