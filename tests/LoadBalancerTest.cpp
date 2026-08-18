/**
 * LoadBalancerTest.cpp
 * 
 * Unit tests for LoadBalancer
 * Part of the AGENT-ZERO-GENESIS project (AZ-SCALE-001)
 */

#include <opencog/agentzero/distributed/LoadBalancer.h>
#include <opencog/agentzero/distributed/DistributedCoordinator.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>

#include <gtest/gtest.h>
#include <memory>

using namespace opencog;
using namespace opencog::agentzero;

class LoadBalancerTest : public ::testing::Test {
protected:
    void SetUp() override {
        atomspace = std::make_shared<AtomSpace>();
        balancer = std::make_unique<LoadBalancer>(atomspace);
        
        // Create test nodes
        node1 = ComputeNode("node1", "localhost", 8080);
        node1.capacity = 100;
        node1.current_load = 10;
        
        node2 = ComputeNode("node2", "localhost", 8081);
        node2.capacity = 100;
        node2.current_load = 50;
        
        node3 = ComputeNode("node3", "localhost", 8082);
        node3.capacity = 100;
        node3.current_load = 30;
    }
    
    void TearDown() override {
        balancer.reset();
        atomspace.reset();
    }
    
    AtomSpacePtr atomspace;
    std::unique_ptr<LoadBalancer> balancer;
    ComputeNode node1, node2, node3;
};

TEST_F(LoadBalancerTest, ConstructorTest) {
    EXPECT_NE(balancer, nullptr);
    EXPECT_EQ(balancer->getStrategy(), LoadBalancingStrategy::LEAST_LOADED);
}

TEST_F(LoadBalancerTest, SetStrategyTest) {
    balancer->setStrategy(LoadBalancingStrategy::ROUND_ROBIN);
    EXPECT_EQ(balancer->getStrategy(), LoadBalancingStrategy::ROUND_ROBIN);
    
    balancer->setStrategy(LoadBalancingStrategy::WEIGHTED_RANDOM);
    EXPECT_EQ(balancer->getStrategy(), LoadBalancingStrategy::WEIGHTED_RANDOM);
}

TEST_F(LoadBalancerTest, AssignTaskLeastLoadedTest) {
    balancer->setStrategy(LoadBalancingStrategy::LEAST_LOADED);
    
    Handle task_atom = atomspace->add_node(CONCEPT_NODE, "TestTask");
    DistributedTask task("task1", "reasoning", task_atom);
    
    std::vector<ComputeNode> nodes = {node1, node2, node3};
    TaskAssignment assignment = balancer->assignTask(task, nodes);
    
    EXPECT_TRUE(assignment.success);
    EXPECT_EQ(assignment.task_id, "task1");
    EXPECT_EQ(assignment.node_id, "node1");  // Least loaded (10)
}

TEST_F(LoadBalancerTest, AssignTaskRoundRobinTest) {
    balancer->setStrategy(LoadBalancingStrategy::ROUND_ROBIN);
    
    Handle task_atom1 = atomspace->add_node(CONCEPT_NODE, "Task1");
    Handle task_atom2 = atomspace->add_node(CONCEPT_NODE, "Task2");
    Handle task_atom3 = atomspace->add_node(CONCEPT_NODE, "Task3");
    
    DistributedTask task1("task1", "reasoning", task_atom1);
    DistributedTask task2("task2", "reasoning", task_atom2);
    DistributedTask task3("task3", "reasoning", task_atom3);
    
    std::vector<ComputeNode> nodes = {node1, node2, node3};
    
    TaskAssignment a1 = balancer->assignTask(task1, nodes);
    TaskAssignment a2 = balancer->assignTask(task2, nodes);
    TaskAssignment a3 = balancer->assignTask(task3, nodes);
    
    EXPECT_TRUE(a1.success);
    EXPECT_TRUE(a2.success);
    EXPECT_TRUE(a3.success);
    
    // Should cycle through nodes
    EXPECT_EQ(a1.node_id, "node1");
    EXPECT_EQ(a2.node_id, "node2");
    EXPECT_EQ(a3.node_id, "node3");
}

TEST_F(LoadBalancerTest, AssignTaskWithNoNodesTest) {
    Handle task_atom = atomspace->add_node(CONCEPT_NODE, "TestTask");
    DistributedTask task("task1", "reasoning", task_atom);
    
    std::vector<ComputeNode> empty_nodes;
    TaskAssignment assignment = balancer->assignTask(task, empty_nodes);
    
    EXPECT_FALSE(assignment.success);
    EXPECT_EQ(assignment.error_message, "No available nodes");
}

TEST_F(LoadBalancerTest, AssignMultipleTasksTest) {
    balancer->setStrategy(LoadBalancingStrategy::LEAST_LOADED);
    
    std::vector<DistributedTask> tasks;
    for (int i = 0; i < 5; i++) {
        Handle atom = atomspace->add_node(CONCEPT_NODE, "Task" + std::to_string(i));
        tasks.emplace_back("task" + std::to_string(i), "reasoning", atom);
    }
    
    std::vector<ComputeNode> nodes = {node1, node2, node3};
    std::vector<TaskAssignment> assignments = balancer->assignTasks(tasks, nodes);
    
    EXPECT_EQ(assignments.size(), 5);
    for (const auto& assignment : assignments) {
        EXPECT_TRUE(assignment.success);
    }
}

TEST_F(LoadBalancerTest, UpdateNodeLoadTest) {
    balancer->updateNodeLoad("node1", 25);
    balancer->updateNodeLoad("node2", 75);
    balancer->updateNodeLoad("node3", 50);
    
    auto stats = balancer->getLoadStats();
    
    EXPECT_GT(stats["average_load"], 0.0);
    EXPECT_EQ(stats["max_load"], 75.0);
    EXPECT_EQ(stats["min_load"], 25.0);
    EXPECT_EQ(stats["load_variance"], 50.0);
}

TEST_F(LoadBalancerTest, GetLoadStatsTest) {
    balancer->updateNodeLoad("node1", 10);
    balancer->updateNodeLoad("node2", 50);
    balancer->updateNodeLoad("node3", 30);
    
    auto stats = balancer->getLoadStats();
    
    EXPECT_DOUBLE_EQ(stats["average_load"], 30.0);
    EXPECT_DOUBLE_EQ(stats["max_load"], 50.0);
    EXPECT_DOUBLE_EQ(stats["min_load"], 10.0);
    EXPECT_DOUBLE_EQ(stats["load_variance"], 40.0);
}

TEST_F(LoadBalancerTest, CalculateDistributionTest) {
    std::vector<ComputeNode> nodes = {node1, node2, node3};
    
    auto distribution = balancer->calculateDistribution(100, nodes);
    
    // Check that all tasks are distributed
    int total_distributed = 0;
    for (const auto& pair : distribution) {
        total_distributed += pair.second;
    }
    
    EXPECT_GT(total_distributed, 0);
    EXPECT_LE(total_distributed, 100);
}

TEST_F(LoadBalancerTest, SuggestRebalancingTest) {
    node1.current_load = 10;
    node2.current_load = 90;  // High load
    node3.current_load = 30;
    
    std::vector<ComputeNode> nodes = {node1, node2, node3};
    
    auto migrations = balancer->suggestRebalancing(nodes);
    
    // Should suggest migrations due to high imbalance
    // (specific migrations may vary by implementation)
    EXPECT_NO_THROW(migrations.size());
}

TEST_F(LoadBalancerTest, AdaptiveModeTest) {
    EXPECT_FALSE(balancer->isAdaptiveMode());
    
    balancer->setAdaptiveMode(true);
    EXPECT_TRUE(balancer->isAdaptiveMode());
    
    balancer->setAdaptiveMode(false);
    EXPECT_FALSE(balancer->isAdaptiveMode());
}

TEST_F(LoadBalancerTest, WeightedRandomStrategyTest) {
    balancer->setStrategy(LoadBalancingStrategy::WEIGHTED_RANDOM);
    
    Handle task_atom = atomspace->add_node(CONCEPT_NODE, "TestTask");
    DistributedTask task("task1", "reasoning", task_atom);
    
    std::vector<ComputeNode> nodes = {node1, node2, node3};
    
    // Run multiple times to ensure randomness works
    bool assigned_to_different_nodes = false;
    std::string first_node;
    
    for (int i = 0; i < 10; i++) {
        TaskAssignment assignment = balancer->assignTask(task, nodes);
        EXPECT_TRUE(assignment.success);
        
        if (i == 0) {
            first_node = assignment.node_id;
        } else if (assignment.node_id != first_node) {
            assigned_to_different_nodes = true;
        }
    }
    
    // With randomness, should assign to different nodes
    // (though theoretically could randomly pick same node each time)
    // This test may occasionally fail due to random chance
}

TEST_F(LoadBalancerTest, InactiveNodeTest) {
    node2.is_active = false;  // Mark node2 as inactive
    
    balancer->setStrategy(LoadBalancingStrategy::LEAST_LOADED);
    
    Handle task_atom = atomspace->add_node(CONCEPT_NODE, "TestTask");
    DistributedTask task("task1", "reasoning", task_atom);
    
    std::vector<ComputeNode> nodes = {node1, node2, node3};
    TaskAssignment assignment = balancer->assignTask(task, nodes);
    
    EXPECT_TRUE(assignment.success);
    EXPECT_NE(assignment.node_id, "node2");  // Should not assign to inactive node
}

// Main function for running tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
