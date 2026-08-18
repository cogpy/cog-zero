#include "test_runner.h"

#include <opencog/agentzero/WorkingMemory.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(WorkingMemory_Construct)
{
    auto as = make_as();
    WorkingMemory wm(as, 50, 0.2, std::chrono::seconds(300));
    ASSERT_EQ(wm.getMaxCapacity(), static_cast<size_t>(50));
    ASSERT_NEAR(wm.getImportanceThreshold(), 0.2, 1e-9);
    ASSERT_EQ(wm.getCurrentSize(), static_cast<size_t>(0));
}

TEST(WorkingMemory_AddGetRemove)
{
    auto as = make_as();
    WorkingMemory wm(as, 100, 0.1, std::chrono::seconds(3600));

    Handle robot = as->add_node(CONCEPT_NODE, "Robot");
    Handle goal = as->add_node(CONCEPT_NODE, "Goal");

    ASSERT_TRUE(wm.addItem(robot, 0.9, "agent_self"));
    ASSERT_TRUE(wm.addItem(goal, 0.8, "goals"));
    ASSERT_TRUE(wm.hasItem(robot));
    ASSERT_EQ(wm.getCurrentSize(), static_cast<size_t>(2));

    auto item = wm.getItem(robot);
    ASSERT_TRUE(item != nullptr);
    ASSERT_NEAR(item->importance, 0.9, 1e-9);

    auto accessed = wm.accessItem(robot);
    ASSERT_TRUE(accessed != nullptr);
    ASSERT_GE(accessed->access_count, static_cast<size_t>(1));

    ASSERT_TRUE(wm.updateImportance(goal, 0.95));
    ASSERT_TRUE(wm.removeItem(goal));
    ASSERT_FALSE(wm.hasItem(goal));
}

TEST(WorkingMemory_ContextAndImportance)
{
    auto as = make_as();
    WorkingMemory wm(as, 100, 0.1, std::chrono::seconds(3600));

    Handle a = as->add_node(CONCEPT_NODE, "A");
    Handle b = as->add_node(CONCEPT_NODE, "B");
    Handle c = as->add_node(CONCEPT_NODE, "C");

    wm.addItem(a, 0.9, "goals");
    wm.addItem(b, 0.4, "perception");
    wm.setActiveContext("reasoning");
    wm.addItem(c, 0.7);  // uses active context

    ASSERT_EQ(wm.getActiveContext(), std::string("reasoning"));
    ASSERT_EQ(wm.getItemsByContext("goals").size(), static_cast<size_t>(1));
    ASSERT_EQ(wm.getItemsByContext("reasoning").size(), static_cast<size_t>(1));

    auto important = wm.getImportantItems(0.7);
    ASSERT_EQ(important.size(), static_cast<size_t>(2));

    auto top = wm.getMostImportantItems(1);
    ASSERT_EQ(top.size(), static_cast<size_t>(1));
    ASSERT_NEAR(top[0]->importance, 0.9, 1e-9);
}

TEST(WorkingMemory_CapacityAndCleanup)
{
    auto as = make_as();
    WorkingMemory wm(as, /*capacity=*/5, 0.3, std::chrono::seconds(3600));

    for (int i = 0; i < 8; ++i) {
        Handle h = as->add_node(CONCEPT_NODE, "T" + std::to_string(i));
        wm.addItem(h, 0.05 * (i + 1), "tmp");
    }
    ASSERT_LE(wm.getCurrentSize(), static_cast<size_t>(5));

    size_t removed = wm.runCleanup(true);
    ASSERT_GE(removed, static_cast<size_t>(0));
    ASSERT_TRUE(wm.validateMemoryConsistency());
}

TEST(WorkingMemory_Stats)
{
    auto as = make_as();
    WorkingMemory wm(as);
    Handle h = as->add_node(CONCEPT_NODE, "S");
    wm.addItem(h, 0.5);
    wm.getItem(h);
    wm.getItem(as->add_node(CONCEPT_NODE, "missing_probe")); // miss path via new atom not added
    // miss: get item that was never added
    Handle m = as->add_node(CONCEPT_NODE, "Miss");
    ASSERT_TRUE(wm.getItem(m) == nullptr);

    auto stats = wm.getPerformanceStats();
    ASSERT_TRUE(stats.count("current_size") > 0);
    ASSERT_GE(wm.getHitRate(), 0.0);
}
