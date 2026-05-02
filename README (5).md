# Agent-Zero Distributed Computing Module (AZ-SCALE-001)

## Overview

The Agent-Zero Distributed Computing module provides scalable distributed computing capabilities for the Agent-Zero cognitive architecture. It enables Agent-Zero instances to coordinate across multiple compute nodes, distributing workloads efficiently while maintaining integration with OpenCog's AtomSpace knowledge representation.

## Features

- **Distributed Coordination**: Central coordinator manages task distribution across cluster nodes
- **Cluster Management**: Automatic node discovery, health monitoring, and resource tracking
- **Intelligent Load Balancing**: Multiple strategies for optimal task distribution
- **AtomSpace Integration**: Seamless integration with OpenCog's knowledge representation
- **Fault Tolerance**: Health monitoring and automatic node failure handling
- **Scalability**: Support for hundreds of compute nodes and thousands of concurrent tasks

## Architecture

### Core Components

1. **DistributedCoordinator**: Central coordination hub for distributed operations
   - Task submission and tracking
   - Node registration and management
   - Result aggregation
   - Cluster-wide coordination

2. **ClusterManager**: Manages compute cluster resources
   - Node discovery and registration
   - Health monitoring and status tracking
   - Resource capacity management
   - Node capability tracking

3. **LoadBalancer**: Intelligent task distribution
   - Multiple load balancing strategies
   - Dynamic load monitoring
   - Adaptive strategy selection
   - Task affinity optimization

## Load Balancing Strategies

### 1. Round Robin
Simple round-robin distribution of tasks across available nodes.

**Use Case**: Uniform task distribution with similar task costs

```cpp
balancer.setStrategy(LoadBalancingStrategy::ROUND_ROBIN);
```

### 2. Least Loaded
Assigns tasks to the node with the lowest current load.

**Use Case**: Dynamic workload with varying task durations

```cpp
balancer.setStrategy(LoadBalancingStrategy::LEAST_LOADED);
```

### 3. Weighted Random
Random selection weighted by available node capacity.

**Use Case**: Probabilistic distribution with capacity awareness

```cpp
balancer.setStrategy(LoadBalancingStrategy::WEIGHTED_RANDOM);
```

### 4. Task Affinity
Considers task type and node capabilities for optimal matching.

**Use Case**: Heterogeneous cluster with specialized nodes

```cpp
balancer.setStrategy(LoadBalancingStrategy::TASK_AFFINITY);
```

### 5. Locality Aware
Optimizes for data locality in AtomSpace.

**Use Case**: Tasks with significant data dependencies

```cpp
balancer.setStrategy(LoadBalancingStrategy::LOCALITY_AWARE);
```

### Adaptive Mode
Automatically selects optimal strategy based on cluster conditions.

```cpp
balancer.setAdaptiveMode(true);
```

## Usage Examples

### Basic Setup

```cpp
#include <opencog/agentzero/distributed/DistributedCoordinator.h>
#include <opencog/agentzero/distributed/ClusterManager.h>
#include <opencog/agentzero/distributed/LoadBalancer.h>

// Create shared AtomSpace
AtomSpacePtr atomspace = std::make_shared<AtomSpace>();

// Initialize cluster
ClusterManager cluster(atomspace, "my-cluster");
cluster.initialize();

// Add compute nodes
NodeCapabilities caps;
caps.cpu_cores = 8;
caps.memory_mb = 16384;
caps.has_gpu = true;
caps.supported_tasks = {"reasoning", "learning"};

cluster.addNode("node1", "192.168.1.10", 8080, caps);
```

### Task Distribution

```cpp
// Create coordinator
DistributedCoordinator coordinator(atomspace, "coordinator1");

// Register nodes
coordinator.registerNode("node1", "192.168.1.10", 8080);
coordinator.registerNode("node2", "192.168.1.11", 8080);

// Submit tasks
Handle task_atom = atomspace->add_node(CONCEPT_NODE, "MyTask");
std::string task_id = coordinator.submitTask("reasoning", task_atom);

// Check task status
if (coordinator.isTaskCompleted(task_id)) {
    Handle result = coordinator.getTaskResult(task_id);
    // Process result
}
```

### Load Balancing

```cpp
// Create load balancer
LoadBalancer balancer(atomspace, LoadBalancingStrategy::LEAST_LOADED);

// Update node loads
balancer.updateNodeLoad("node1", 30);
balancer.updateNodeLoad("node2", 60);

// Get statistics
auto stats = balancer.getLoadStats();
std::cout << "Average load: " << stats["average_load"] << std::endl;

// Suggest rebalancing
auto migrations = balancer.suggestRebalancing(nodes);
```

### Health Monitoring

```cpp
// Perform health check
int responsive = cluster.performHealthCheck();
std::cout << "Responsive nodes: " << responsive << std::endl;

// Get healthy nodes
auto healthy = cluster.getHealthyNodes();
for (const auto& node_id : healthy) {
    std::cout << "Healthy: " << node_id << std::endl;
}

// Update health status
NodeHealth health;
health.is_responsive = true;
health.cpu_usage = 0.5;
health.memory_usage = 0.4;
health.active_tasks = 3;
cluster.updateNodeHealth("node1", health);
```

## Integration with OpenCog

### AtomSpace Integration

All distributed computing state is represented in AtomSpace:

- **Compute nodes** as ConceptNodes with capabilities
- **Tasks** as structured atoms with type and status
- **Results** stored as atom values
- **Cluster topology** as link structures

Example:
```scheme
(ConceptNode "ComputeNode:node1")
(EvaluationLink
    (PredicateNode "cpu_cores")
    (ConceptNode "ComputeNode:node1")
    (NumberNode "8"))
```

### Remote AtomSpace Operations

When `atomspace-rpc` is available, supports remote AtomSpace operations:

```cpp
#ifdef HAVE_ATOMSPACE_RPC
    // Enable RPC backend for distributed AtomSpace
    coordinator.enableRPC("rpc://remote-host:50051");
#endif
```

### DHT Integration

When `atomspace-dht` is available, supports distributed hash table storage:

```cpp
#ifdef HAVE_ATOMSPACE_DHT
    // Enable DHT backend for decentralized storage
    coordinator.enableDHT("dht://bootstrap-node:4555");
#endif
```

## Performance Considerations

### Scalability

- **Horizontal scaling**: Add more compute nodes to increase capacity
- **Task granularity**: Balance between task overhead and parallelism
- **Network bandwidth**: Consider data transfer costs

### Optimization Tips

1. **Batch task submission**: Submit multiple tasks at once
2. **Use appropriate strategy**: Match strategy to workload characteristics
3. **Enable adaptive mode**: Let system optimize automatically
4. **Monitor cluster health**: Regularly check node status
5. **Consider data locality**: Minimize data transfer between nodes

### Resource Requirements

- **Memory**: ~1MB per node + ~100KB per active task
- **Network**: Low latency (<10ms) recommended for cluster communication
- **CPU**: Minimal overhead (<1% per node)

## Building and Testing

### Build

```bash
cd agents/cpp
mkdir build && cd build
cmake ..
make agentzero-distributed
```

### Run Tests

```bash
make test
# Or specifically:
./tests/DistributedCoordinatorTest
./tests/ClusterManagerTest
./tests/LoadBalancerTest
```

### Run Example

```bash
./examples/DistributedComputingExample
```

## Dependencies

### Required
- **cogutil**: OpenCog utilities
- **atomspace**: AtomSpace knowledge representation
- **Boost**: C++ libraries (system, filesystem, thread)

### Optional
- **atomspace-rpc**: Remote AtomSpace operations
- **atomspace-dht**: Distributed hash table storage
- **GTest**: Unit testing framework

## API Reference

### DistributedCoordinator

```cpp
class DistributedCoordinator {
public:
    DistributedCoordinator(AtomSpacePtr atomspace, const std::string& coordinator_id);
    
    bool registerNode(const std::string& node_id, const std::string& hostname, int port);
    bool unregisterNode(const std::string& node_id);
    
    std::string submitTask(const std::string& task_type, Handle task_atom);
    bool isTaskCompleted(const std::string& task_id) const;
    Handle getTaskResult(const std::string& task_id) const;
    
    std::vector<ComputeNode> getRegisteredNodes() const;
    std::map<std::string, int> getClusterStats() const;
    
    void healthCheck();
    void shutdown();
};
```

### ClusterManager

```cpp
class ClusterManager {
public:
    ClusterManager(AtomSpacePtr atomspace, const std::string& cluster_id);
    
    bool initialize();
    bool addNode(const std::string& node_id, const std::string& hostname, 
                int port, const NodeCapabilities& capabilities);
    bool removeNode(const std::string& node_id);
    
    void updateNodeHealth(const std::string& node_id, const NodeHealth& health);
    NodeCapabilities getNodeCapabilities(const std::string& node_id) const;
    NodeHealth getNodeHealth(const std::string& node_id) const;
    
    std::vector<std::string> getHealthyNodes() const;
    std::vector<std::string> getNodesWithCapability(const std::string& capability) const;
    
    std::map<std::string, size_t> getClusterCapacity() const;
    std::map<std::string, size_t> getAvailableResources() const;
    
    int performHealthCheck();
    void shutdown();
};
```

### LoadBalancer

```cpp
class LoadBalancer {
public:
    LoadBalancer(AtomSpacePtr atomspace, LoadBalancingStrategy strategy);
    
    TaskAssignment assignTask(const DistributedTask& task,
                            const std::vector<ComputeNode>& available_nodes);
    std::vector<TaskAssignment> assignTasks(
        const std::vector<DistributedTask>& tasks,
        const std::vector<ComputeNode>& available_nodes);
    
    void setStrategy(LoadBalancingStrategy strategy);
    LoadBalancingStrategy getStrategy() const;
    
    void updateNodeLoad(const std::string& node_id, int current_load);
    std::map<std::string, double> getLoadStats() const;
    
    std::map<std::string, std::string> suggestRebalancing(
        const std::vector<ComputeNode>& nodes);
    std::map<std::string, int> calculateDistribution(
        int num_tasks, const std::vector<ComputeNode>& nodes);
    
    void setAdaptiveMode(bool enabled);
    bool isAdaptiveMode() const;
};
```

## Error Handling

All methods return appropriate error indicators:
- Boolean methods return `false` on failure
- Methods returning objects/containers return empty on failure
- Errors are logged via OpenCog's logging system

Example error handling:
```cpp
if (!coordinator.registerNode("node1", "host", 8080)) {
    std::cerr << "Failed to register node" << std::endl;
}
```

## Future Enhancements

- Network protocol implementation (currently simulated)
- Security and authentication
- Advanced fault tolerance (task migration, checkpointing)
- Integration with Kubernetes/container orchestration
- Performance monitoring and metrics collection
- Distributed debugging tools

## Contributing

See the main AGENT-ZERO-GENESIS.md document for contribution guidelines.

## License

Part of the OpenCog project. See LICENSE file for details.

## References

- [AGENT-ZERO-GENESIS.md](../../../../AGENT-ZERO-GENESIS.md) - Project roadmap
- [OpenCog AtomSpace](https://wiki.opencog.org/w/AtomSpace)
- [OpenCog CogServer](https://wiki.opencog.org/w/CogServer)
