/**
 * DistributedCoordinatorTest.cpp
 * 
 * Unit tests for DistributedCoordinator
 * Part of the AGENT-ZERO-GENESIS project (AZ-SCALE-001)
 */

#include <opencog/agentzero/distributed/DistributedCoordinator.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>

#include <gtest/gtest.h>
#include <memory>

using namespace opencog;
using namespace opencog::agentzero;

class DistributedCoordinatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        atomspace = std::make_shared<AtomSpace>();
        coordinator = std::make_unique<DistributedCoordinator>(atomspace, "test-coord");
    }
    
    void TearDown() override {
        coordinator.reset();
        atomspace.reset();
    }
    
    AtomSpacePtr atomspace;
    std::unique_ptr<DistributedCoordinator> coordinator;
};

TEST_F(DistributedCoordinatorTest, ConstructorTest) {
    EXPECT_NE(coordinator, nullptr);
    EXPECT_EQ(coordinator->getCoordinatorId(), "test-coord");
}

TEST_F(DistributedCoordinatorTest, RegisterNodeTest) {
    bool result = coordinator->registerNode("node1", "localhost", 8080);
    EXPECT_TRUE(result);
    
    // Verify node is registered
    auto nodes = coordinator->getRegisteredNodes();
    EXPECT_EQ(nodes.size(), 1);
    EXPECT_EQ(nodes[0].id, "node1");
    EXPECT_EQ(nodes[0].hostname, "localhost");
    EXPECT_EQ(nodes[0].port, 8080);
}

TEST_F(DistributedCoordinatorTest, RegisterDuplicateNodeTest) {
    coordinator->registerNode("node1", "localhost", 8080);
    bool result = coordinator->registerNode("node1", "localhost", 8080);
    EXPECT_FALSE(result);  // Should fail on duplicate
}

TEST_F(DistributedCoordinatorTest, UnregisterNodeTest) {
    coordinator->registerNode("node1", "localhost", 8080);
    
    bool result = coordinator->unregisterNode("node1");
    EXPECT_TRUE(result);
    
    auto nodes = coordinator->getRegisteredNodes();
    EXPECT_EQ(nodes.size(), 0);
}

TEST_F(DistributedCoordinatorTest, UnregisterNonexistentNodeTest) {
    bool result = coordinator->unregisterNode("nonexistent");
    EXPECT_FALSE(result);
}

TEST_F(DistributedCoordinatorTest, SubmitTaskTest) {
    coordinator->registerNode("node1", "localhost", 8080);
    
    Handle task_atom = atomspace->add_node(CONCEPT_NODE, "TestTask");
    std::string task_id = coordinator->submitTask("reasoning", task_atom);
    
    EXPECT_FALSE(task_id.empty());
    EXPECT_TRUE(task_id.find("test-coord-task-") != std::string::npos);
}

TEST_F(DistributedCoordinatorTest, SubmitTaskWithoutNodesTest) {
    Handle task_atom = atomspace->add_node(CONCEPT_NODE, "TestTask");
    std::string task_id = coordinator->submitTask("reasoning", task_atom);
    
    EXPECT_TRUE(task_id.empty());  // Should fail without nodes
}

TEST_F(DistributedCoordinatorTest, TaskCompletionTest) {
    coordinator->registerNode("node1", "localhost", 8080);
    
    Handle task_atom = atomspace->add_node(CONCEPT_NODE, "TestTask");
    std::string task_id = coordinator->submitTask("reasoning", task_atom);
    
    // Initially not completed
    EXPECT_FALSE(coordinator->isTaskCompleted(task_id));
    
    // Task result should be undefined until completed
    Handle result = coordinator->getTaskResult(task_id);
    EXPECT_EQ(result, Handle::UNDEFINED);
}

TEST_F(DistributedCoordinatorTest, ClusterStatsTest) {
    coordinator->registerNode("node1", "localhost", 8080);
    coordinator->registerNode("node2", "localhost", 8081);
    
    auto stats = coordinator->getClusterStats();
    
    EXPECT_EQ(stats["total_nodes"], 2);
    EXPECT_EQ(stats["active_nodes"], 2);
    EXPECT_GT(stats["total_capacity"], 0);
}

TEST_F(DistributedCoordinatorTest, HealthCheckTest) {
    coordinator->registerNode("node1", "localhost", 8080);
    coordinator->registerNode("node2", "localhost", 8081);
    
    // Should not throw
    EXPECT_NO_THROW(coordinator->healthCheck());
}

TEST_F(DistributedCoordinatorTest, MultipleTasksTest) {
    coordinator->registerNode("node1", "localhost", 8080);
    coordinator->registerNode("node2", "localhost", 8081);
    
    Handle task1 = atomspace->add_node(CONCEPT_NODE, "Task1");
    Handle task2 = atomspace->add_node(CONCEPT_NODE, "Task2");
    Handle task3 = atomspace->add_node(CONCEPT_NODE, "Task3");
    
    std::string id1 = coordinator->submitTask("reasoning", task1);
    std::string id2 = coordinator->submitTask("learning", task2);
    std::string id3 = coordinator->submitTask("planning", task3);
    
    EXPECT_FALSE(id1.empty());
    EXPECT_FALSE(id2.empty());
    EXPECT_FALSE(id3.empty());
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
}

TEST_F(DistributedCoordinatorTest, TaskCompletionCallbackTest) {
    bool callback_called = false;
    std::string callback_task_id;
    
    coordinator->setTaskCompletionCallback([&](const std::string& task_id) {
        callback_called = true;
        callback_task_id = task_id;
    });
    
    coordinator->registerNode("node1", "localhost", 8080);
    Handle task_atom = atomspace->add_node(CONCEPT_NODE, "TestTask");
    std::string task_id = coordinator->submitTask("reasoning", task_atom);
    
    // Callback setup should succeed
    EXPECT_FALSE(task_id.empty());
}

TEST_F(DistributedCoordinatorTest, ShutdownTest) {
    coordinator->registerNode("node1", "localhost", 8080);
    coordinator->registerNode("node2", "localhost", 8081);
    
    coordinator->shutdown();
    
    auto nodes = coordinator->getRegisteredNodes();
    EXPECT_EQ(nodes.size(), 0);
}

// Main function for running tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
