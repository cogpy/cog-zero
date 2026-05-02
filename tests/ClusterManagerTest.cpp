/**
 * ClusterManagerTest.cpp
 * 
 * Unit tests for ClusterManager
 * Part of the AGENT-ZERO-GENESIS project (AZ-SCALE-001)
 */

#include <opencog/agentzero/distributed/ClusterManager.h>
#include <opencog/atomspace/AtomSpace.h>

#include <gtest/gtest.h>
#include <memory>

using namespace opencog;
using namespace opencog::agentzero;

class ClusterManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        atomspace = std::make_shared<AtomSpace>();
        cluster = std::make_unique<ClusterManager>(atomspace, "test-cluster");
        cluster->initialize();
    }
    
    void TearDown() override {
        cluster.reset();
        atomspace.reset();
    }
    
    AtomSpacePtr atomspace;
    std::unique_ptr<ClusterManager> cluster;
};

TEST_F(ClusterManagerTest, InitializeTest) {
    EXPECT_NE(cluster, nullptr);
    EXPECT_EQ(cluster->getClusterId(), "test-cluster");
}

TEST_F(ClusterManagerTest, AddNodeTest) {
    NodeCapabilities caps;
    caps.cpu_cores = 4;
    caps.memory_mb = 8192;
    caps.has_gpu = false;
    
    bool result = cluster->addNode("node1", "192.168.1.10", 8080, caps);
    EXPECT_TRUE(result);
    EXPECT_EQ(cluster->getNodeCount(), 1);
}

TEST_F(ClusterManagerTest, AddDuplicateNodeTest) {
    NodeCapabilities caps;
    caps.cpu_cores = 4;
    caps.memory_mb = 8192;
    
    cluster->addNode("node1", "192.168.1.10", 8080, caps);
    bool result = cluster->addNode("node1", "192.168.1.10", 8080, caps);
    
    EXPECT_FALSE(result);
}

TEST_F(ClusterManagerTest, RemoveNodeTest) {
    NodeCapabilities caps;
    caps.cpu_cores = 4;
    caps.memory_mb = 8192;
    
    cluster->addNode("node1", "192.168.1.10", 8080, caps);
    bool result = cluster->removeNode("node1");
    
    EXPECT_TRUE(result);
    EXPECT_EQ(cluster->getNodeCount(), 0);
}

TEST_F(ClusterManagerTest, GetNodeCapabilitiesTest) {
    NodeCapabilities caps;
    caps.cpu_cores = 8;
    caps.memory_mb = 16384;
    caps.has_gpu = true;
    caps.supported_tasks = {"reasoning", "learning"};
    
    cluster->addNode("node1", "192.168.1.10", 8080, caps);
    
    NodeCapabilities retrieved = cluster->getNodeCapabilities("node1");
    EXPECT_EQ(retrieved.cpu_cores, 8);
    EXPECT_EQ(retrieved.memory_mb, 16384);
    EXPECT_TRUE(retrieved.has_gpu);
    EXPECT_EQ(retrieved.supported_tasks.size(), 2);
}

TEST_F(ClusterManagerTest, UpdateNodeHealthTest) {
    NodeCapabilities caps;
    caps.cpu_cores = 4;
    caps.memory_mb = 8192;
    
    cluster->addNode("node1", "192.168.1.10", 8080, caps);
    
    NodeHealth health;
    health.is_responsive = true;
    health.cpu_usage = 0.5;
    health.memory_usage = 0.6;
    health.active_tasks = 3;
    
    cluster->updateNodeHealth("node1", health);
    
    NodeHealth retrieved = cluster->getNodeHealth("node1");
    EXPECT_TRUE(retrieved.is_responsive);
    EXPECT_DOUBLE_EQ(retrieved.cpu_usage, 0.5);
    EXPECT_DOUBLE_EQ(retrieved.memory_usage, 0.6);
    EXPECT_EQ(retrieved.active_tasks, 3);
}

TEST_F(ClusterManagerTest, GetHealthyNodesTest) {
    NodeCapabilities caps;
    caps.cpu_cores = 4;
    caps.memory_mb = 8192;
    
    cluster->addNode("node1", "192.168.1.10", 8080, caps);
    cluster->addNode("node2", "192.168.1.11", 8080, caps);
    
    // Both should be healthy initially
    auto healthy = cluster->getHealthyNodes();
    EXPECT_EQ(healthy.size(), 2);
    
    // Mark one as unhealthy
    NodeHealth health;
    health.is_responsive = false;
    cluster->updateNodeHealth("node1", health);
    
    healthy = cluster->getHealthyNodes();
    EXPECT_EQ(healthy.size(), 1);
    EXPECT_EQ(healthy[0], "node2");
}

TEST_F(ClusterManagerTest, GetNodesWithCapabilityTest) {
    NodeCapabilities caps1;
    caps1.cpu_cores = 4;
    caps1.memory_mb = 8192;
    caps1.supported_tasks = {"reasoning", "learning"};
    
    NodeCapabilities caps2;
    caps2.cpu_cores = 8;
    caps2.memory_mb = 16384;
    caps2.supported_tasks = {"planning"};
    
    cluster->addNode("node1", "192.168.1.10", 8080, caps1);
    cluster->addNode("node2", "192.168.1.11", 8080, caps2);
    
    auto nodes_with_reasoning = cluster->getNodesWithCapability("reasoning");
    EXPECT_EQ(nodes_with_reasoning.size(), 1);
    EXPECT_EQ(nodes_with_reasoning[0], "node1");
    
    auto nodes_with_planning = cluster->getNodesWithCapability("planning");
    EXPECT_EQ(nodes_with_planning.size(), 1);
    EXPECT_EQ(nodes_with_planning[0], "node2");
}

TEST_F(ClusterManagerTest, GetClusterCapacityTest) {
    NodeCapabilities caps1;
    caps1.cpu_cores = 4;
    caps1.memory_mb = 8192;
    caps1.has_gpu = false;
    
    NodeCapabilities caps2;
    caps2.cpu_cores = 8;
    caps2.memory_mb = 16384;
    caps2.has_gpu = true;
    
    cluster->addNode("node1", "192.168.1.10", 8080, caps1);
    cluster->addNode("node2", "192.168.1.11", 8080, caps2);
    
    auto capacity = cluster->getClusterCapacity();
    
    EXPECT_EQ(capacity["total_cpu_cores"], 12);
    EXPECT_EQ(capacity["total_memory_mb"], 24576);
    EXPECT_EQ(capacity["gpu_nodes"], 1);
    EXPECT_EQ(capacity["total_nodes"], 2);
}

TEST_F(ClusterManagerTest, GetAvailableResourcesTest) {
    NodeCapabilities caps;
    caps.cpu_cores = 8;
    caps.memory_mb = 16384;
    
    cluster->addNode("node1", "192.168.1.10", 8080, caps);
    
    NodeHealth health;
    health.is_responsive = true;
    health.cpu_usage = 0.5;  // 50% used
    health.memory_usage = 0.25;  // 25% used
    health.active_tasks = 2;
    
    cluster->updateNodeHealth("node1", health);
    
    auto available = cluster->getAvailableResources();
    
    EXPECT_EQ(available["available_cpu_cores"], 4);  // 50% of 8
    EXPECT_EQ(available["available_memory_mb"], 12288);  // 75% of 16384
}

TEST_F(ClusterManagerTest, PerformHealthCheckTest) {
    NodeCapabilities caps;
    caps.cpu_cores = 4;
    caps.memory_mb = 8192;
    
    cluster->addNode("node1", "192.168.1.10", 8080, caps);
    cluster->addNode("node2", "192.168.1.11", 8080, caps);
    
    int responsive = cluster->performHealthCheck();
    
    // Initially all should be responsive
    EXPECT_EQ(responsive, 2);
}

TEST_F(ClusterManagerTest, ShutdownTest) {
    NodeCapabilities caps;
    caps.cpu_cores = 4;
    caps.memory_mb = 8192;
    
    cluster->addNode("node1", "192.168.1.10", 8080, caps);
    cluster->addNode("node2", "192.168.1.11", 8080, caps);
    
    cluster->shutdown();
    
    EXPECT_EQ(cluster->getNodeCount(), 0);
}

// Main function for running tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
