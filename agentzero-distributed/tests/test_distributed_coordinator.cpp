#include "test_runner.h"

#include <opencog/agentzero/distributed/DistributedCoordinator.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(DistributedCoordinator_RegisterAndUnregister)
{
    auto as = make_as();
    DistributedCoordinator coord(as, "test-coord");

    ASSERT_EQ(coord.getCoordinatorId(), std::string("test-coord"));
    ASSERT_TRUE(coord.registerNode("node1", "localhost", 8080));
    ASSERT_FALSE(coord.registerNode("node1", "localhost", 8080));

    auto nodes = coord.getRegisteredNodes();
    ASSERT_EQ(nodes.size(), static_cast<size_t>(1));
    ASSERT_EQ(nodes[0].id, std::string("node1"));
    ASSERT_EQ(nodes[0].port, 8080);

    ASSERT_TRUE(coord.unregisterNode("node1"));
    ASSERT_FALSE(coord.unregisterNode("missing"));
    ASSERT_EQ(coord.getRegisteredNodes().size(), static_cast<size_t>(0));
}

TEST(DistributedCoordinator_SubmitTaskAndStats)
{
    auto as = make_as();
    DistributedCoordinator coord(as, "coord-a");
    coord.registerNode("n1", "h1", 1);
    coord.registerNode("n2", "h2", 2);

    Handle task_atom = as->add_node(CONCEPT_NODE, "ReasonTask");
    std::string tid = coord.submitTask("reasoning", task_atom);
    ASSERT_FALSE(tid.empty());
    ASSERT_TRUE(tid.find("coord-a-task-") != std::string::npos);
    ASSERT_FALSE(coord.isTaskCompleted(tid));
    ASSERT_TRUE(coord.getTaskResult(tid) == Handle::UNDEFINED);

    auto stats = coord.getClusterStats();
    ASSERT_EQ(stats["total_nodes"], 2);
    ASSERT_EQ(stats["active_nodes"], 2);
    ASSERT_GT(stats["total_capacity"], 0);
    ASSERT_GT(stats["total_load"], 0);

    // No nodes → empty task id
    DistributedCoordinator empty(as, "lonely");
    ASSERT_TRUE(empty.submitTask("planning", task_atom).empty());

    coord.healthCheck();
    coord.shutdown();
    ASSERT_EQ(coord.getRegisteredNodes().size(), static_cast<size_t>(0));
}

TEST(DistributedCoordinator_MultipleTasksAndCallback)
{
    auto as = make_as();
    DistributedCoordinator coord(as, "coord-b");
    coord.registerNode("worker", "local", 7000);

    bool called = false;
    coord.setTaskCompletionCallback([&](const std::string&) { called = true; });

    Handle a1 = as->add_node(CONCEPT_NODE, "T1");
    Handle a2 = as->add_node(CONCEPT_NODE, "T2");
    Handle a3 = as->add_node(CONCEPT_NODE, "T3");
    auto id1 = coord.submitTask("reasoning", a1);
    auto id2 = coord.submitTask("learning", a2);
    auto id3 = coord.submitTask("planning", a3);
    ASSERT_FALSE(id1.empty());
    ASSERT_FALSE(id2.empty());
    ASSERT_FALSE(id3.empty());
    ASSERT_NE(id1, id2);
    ASSERT_NE(id2, id3);
    // Callback fires only on completion path (not wired in submit); ensure setter is safe
    ASSERT_FALSE(called);
}
