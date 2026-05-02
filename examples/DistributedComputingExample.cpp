/**
 * DistributedComputingExample.cpp
 * 
 * Example demonstrating distributed computing integration for Agent-Zero
 * Part of the AGENT-ZERO-GENESIS project (AZ-SCALE-001)
 */

#include <opencog/agentzero/distributed/DistributedCoordinator.h>
#include <opencog/agentzero/distributed/ClusterManager.h>
#include <opencog/agentzero/distributed/LoadBalancer.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace opencog;
using namespace opencog::agentzero;

int main() {
    std::cout << "Agent-Zero Distributed Computing Example" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    // Create shared AtomSpace
    AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
    
    // 1. Create Cluster Manager
    std::cout << "\n1. Initializing Cluster Manager..." << std::endl;
    ClusterManager cluster(atomspace, "example-cluster");
    cluster.initialize();
    
    // 2. Add compute nodes to cluster
    std::cout << "\n2. Adding compute nodes to cluster..." << std::endl;
    
    NodeCapabilities caps1;
    caps1.cpu_cores = 8;
    caps1.memory_mb = 16384;
    caps1.has_gpu = true;
    caps1.supported_tasks = {"reasoning", "learning", "planning"};
    cluster.addNode("node1", "192.168.1.10", 8080, caps1);
    std::cout << "   Added node1 (8 cores, 16GB RAM, GPU)" << std::endl;
    
    NodeCapabilities caps2;
    caps2.cpu_cores = 4;
    caps2.memory_mb = 8192;
    caps2.has_gpu = false;
    caps2.supported_tasks = {"reasoning", "planning"};
    cluster.addNode("node2", "192.168.1.11", 8080, caps2);
    std::cout << "   Added node2 (4 cores, 8GB RAM)" << std::endl;
    
    NodeCapabilities caps3;
    caps3.cpu_cores = 16;
    caps3.memory_mb = 32768;
    caps3.has_gpu = true;
    caps3.supported_tasks = {"learning", "planning"};
    cluster.addNode("node3", "192.168.1.12", 8080, caps3);
    std::cout << "   Added node3 (16 cores, 32GB RAM, GPU)" << std::endl;
    
    // 3. Display cluster capacity
    std::cout << "\n3. Cluster Capacity:" << std::endl;
    auto capacity = cluster.getClusterCapacity();
    std::cout << "   Total CPU cores: " << capacity["total_cpu_cores"] << std::endl;
    std::cout << "   Total memory: " << capacity["total_memory_mb"] << " MB" << std::endl;
    std::cout << "   GPU nodes: " << capacity["gpu_nodes"] << std::endl;
    std::cout << "   Total nodes: " << capacity["total_nodes"] << std::endl;
    
    // 4. Create Distributed Coordinator
    std::cout << "\n4. Creating Distributed Coordinator..." << std::endl;
    DistributedCoordinator coordinator(atomspace, "example-coordinator");
    
    // Register nodes with coordinator
    coordinator.registerNode("node1", "192.168.1.10", 8080);
    coordinator.registerNode("node2", "192.168.1.11", 8080);
    coordinator.registerNode("node3", "192.168.1.12", 8080);
    std::cout << "   Registered 3 nodes with coordinator" << std::endl;
    
    // 5. Create Load Balancer
    std::cout << "\n5. Creating Load Balancer..." << std::endl;
    LoadBalancer balancer(atomspace, LoadBalancingStrategy::LEAST_LOADED);
    std::cout << "   Using LEAST_LOADED strategy" << std::endl;
    
    // 6. Submit distributed tasks
    std::cout << "\n6. Submitting distributed tasks..." << std::endl;
    
    Handle reasoning_task = atomspace->add_node(CONCEPT_NODE, "ReasoningTask1");
    std::string task1_id = coordinator.submitTask("reasoning", reasoning_task);
    std::cout << "   Submitted reasoning task: " << task1_id << std::endl;
    
    Handle learning_task = atomspace->add_node(CONCEPT_NODE, "LearningTask1");
    std::string task2_id = coordinator.submitTask("learning", learning_task);
    std::cout << "   Submitted learning task: " << task2_id << std::endl;
    
    Handle planning_task = atomspace->add_node(CONCEPT_NODE, "PlanningTask1");
    std::string task3_id = coordinator.submitTask("planning", planning_task);
    std::cout << "   Submitted planning task: " << task3_id << std::endl;
    
    // 7. Check cluster statistics
    std::cout << "\n7. Cluster Statistics:" << std::endl;
    auto stats = coordinator.getClusterStats();
    std::cout << "   Total nodes: " << stats["total_nodes"] << std::endl;
    std::cout << "   Active nodes: " << stats["active_nodes"] << std::endl;
    std::cout << "   Total capacity: " << stats["total_capacity"] << std::endl;
    std::cout << "   Current load: " << stats["total_load"] << std::endl;
    std::cout << "   Available capacity: " << stats["available_capacity"] << std::endl;
    
    // 8. Perform health check
    std::cout << "\n8. Performing health check..." << std::endl;
    coordinator.healthCheck();
    int responsive = cluster.performHealthCheck();
    std::cout << "   Responsive nodes: " << responsive << std::endl;
    
    // 9. Check load balancer statistics
    std::cout << "\n9. Load Balancer Statistics:" << std::endl;
    balancer.updateNodeLoad("node1", 30);
    balancer.updateNodeLoad("node2", 20);
    balancer.updateNodeLoad("node3", 50);
    
    auto load_stats = balancer.getLoadStats();
    std::cout << "   Average load: " << load_stats["average_load"] << "%" << std::endl;
    std::cout << "   Max load: " << load_stats["max_load"] << "%" << std::endl;
    std::cout << "   Min load: " << load_stats["min_load"] << "%" << std::endl;
    std::cout << "   Load variance: " << load_stats["load_variance"] << "%" << std::endl;
    
    // 10. Test different load balancing strategies
    std::cout << "\n10. Testing different load balancing strategies..." << std::endl;
    
    balancer.setStrategy(LoadBalancingStrategy::ROUND_ROBIN);
    std::cout << "   Switched to ROUND_ROBIN strategy" << std::endl;
    
    balancer.setStrategy(LoadBalancingStrategy::WEIGHTED_RANDOM);
    std::cout << "   Switched to WEIGHTED_RANDOM strategy" << std::endl;
    
    balancer.setStrategy(LoadBalancingStrategy::TASK_AFFINITY);
    std::cout << "   Switched to TASK_AFFINITY strategy" << std::endl;
    
    // 11. Enable adaptive mode
    std::cout << "\n11. Enabling adaptive load balancing..." << std::endl;
    balancer.setAdaptiveMode(true);
    std::cout << "   Adaptive mode enabled - strategy will adjust automatically" << std::endl;
    
    // 12. Display nodes with specific capabilities
    std::cout << "\n12. Querying nodes by capability..." << std::endl;
    auto gpu_nodes = cluster.getNodesWithCapability("learning");
    std::cout << "   Nodes with learning capability: " << gpu_nodes.size() << std::endl;
    for (const auto& node_id : gpu_nodes) {
        std::cout << "     - " << node_id << std::endl;
    }
    
    // 13. Shutdown
    std::cout << "\n13. Shutting down..." << std::endl;
    coordinator.shutdown();
    cluster.shutdown();
    std::cout << "   Clean shutdown complete" << std::endl;
    
    std::cout << "\n=========================================" << std::endl;
    std::cout << "Example completed successfully!" << std::endl;
    
    return 0;
}
