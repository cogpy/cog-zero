#include "test_runner.h"

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/agentzero/ActionExecutor.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(ActionExecutor_SyncAndQueue)
{
    auto as = createAtomSpace();
    ActionExecutor exec(nullptr, as);

    Handle action = as->add_node(CONCEPT_NODE, "TestAction");
    auto result = exec.executeActionSync(action, 1000);
    ASSERT_EQ(result.status, ActionExecutor::ActionStatus::COMPLETED);
    ASSERT_GT(result.duration.count(), 0);
    ASSERT_GT(result.success_probability, 0.0);

    Handle a1 = as->add_node(CONCEPT_NODE, "A1");
    Handle a2 = as->add_node(CONCEPT_NODE, "A2");
    ASSERT_TRUE(exec.executeAction(a1, ActionExecutor::Priority::LOW));
    ASSERT_TRUE(exec.executeAction(a2, ActionExecutor::Priority::HIGH));
    ASSERT_EQ(exec.getPendingActions().size(), 2u);

    ASSERT_TRUE(exec.cancelAction(a1));
    ASSERT_EQ(exec.getActionStatus(a1), ActionExecutor::ActionStatus::CANCELLED);

    int processed = exec.processActionQueue();
    ASSERT_GE(processed, 0);

    auto status = exec.getStatusInfo();
    ASSERT_TRUE(status.find("action_queue_size") != std::string::npos);
}
