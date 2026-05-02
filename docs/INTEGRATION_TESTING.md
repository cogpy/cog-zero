# Integration Testing Guide for AZ-SCALE-001

## Overview

This document provides integration testing procedures for the Agent-Zero Distributed Computing module (AZ-SCALE-001) to verify OpenCog compatibility and proper operation.

## Prerequisites

Before running integration tests, ensure the following OpenCog components are installed:

1. **cogutil** - OpenCog utilities library
2. **atomspace** - AtomSpace knowledge representation
3. **GTest** (optional) - For running unit tests

### Installation

```bash
# From repository root
mkdir -p /tmp/opencog-build && cd /tmp/opencog-build
cmake /path/to/pycog0
make cogutil
cd cogutil-build && sudo make install && sudo ldconfig

cd /tmp/opencog-build
make atomspace
cd atomspace-build && sudo make install && sudo ldconfig
```

## Building the Distributed Module

### Option 1: Standalone Build

```bash
cd agents/cpp
mkdir -p build && cd build

# Set PKG_CONFIG_PATH if needed
export PKG_CONFIG_PATH=/usr/local/share/opencog/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH

cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
make agentzero-distributed
```

### Option 2: Integrated Build

```bash
# From repository root
cd agents/cpp
mkdir -p build && cd build
cmake ..
make
```

## Unit Tests

### Running All Tests

```bash
cd agents/cpp/build
make test
```

### Running Individual Tests

```bash
# Distributed Coordinator Tests
./agentzero-distributed/tests/DistributedCoordinatorTest

# Cluster Manager Tests  
./agentzero-distributed/tests/ClusterManagerTest

# Load Balancer Tests
./agentzero-distributed/tests/LoadBalancerTest
```

### Expected Test Output

All tests should pass with output similar to:
```
[==========] Running 15 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 15 tests from DistributedCoordinatorTest
[ RUN      ] DistributedCoordinatorTest.ConstructorTest
[       OK ] DistributedCoordinatorTest.ConstructorTest (0 ms)
...
[==========] 15 tests from 1 test suite ran. (X ms total)
[  PASSED  ] 15 tests.
```

## Integration Tests

### Test 1: Basic Coordinator Operation

**Objective**: Verify distributed coordinator can manage nodes and tasks

**Steps**:
1. Create AtomSpace instance
2. Create DistributedCoordinator
3. Register multiple nodes
4. Submit tasks
5. Verify task distribution
6. Check cluster statistics

**Expected Results**:
- All nodes register successfully
- Tasks are assigned to different nodes
- Statistics reflect correct state

**Test Script**:
```cpp
#include <opencog/agentzero/distributed/DistributedCoordinator.h>
#include <opencog/atomspace/AtomSpace.h>

using namespace opencog;
using namespace opencog::agentzero;

int main() {
    // Create AtomSpace
    AtomSpacePtr as = std::make_shared<AtomSpace>();
    
    // Create coordinator
    DistributedCoordinator coord(as, "test-coord");
    
    // Register nodes
    assert(coord.registerNode("node1", "localhost", 8080));
    assert(coord.registerNode("node2", "localhost", 8081));
    assert(coord.registerNode("node3", "localhost", 8082));
    
    // Verify registration
    auto nodes = coord.getRegisteredNodes();
    assert(nodes.size() == 3);
    
    // Submit tasks
    Handle task1 = as->add_node(CONCEPT_NODE, "Task1");
    Handle task2 = as->add_node(CONCEPT_NODE, "Task2");
    Handle task3 = as->add_node(CONCEPT_NODE, "Task3");
    
    std::string id1 = coord.submitTask("reasoning", task1);
    std::string id2 = coord.submitTask("learning", task2);
    std::string id3 = coord.submitTask("planning", task3);
    
    assert(!id1.empty() && !id2.empty() && !id3.empty());
    
    // Check statistics
    auto stats = coord.getClusterStats();
    assert(stats["total_nodes"] == 3);
    assert(stats["active_nodes"] == 3);
    assert(stats["total_load"] == 3);
    
    std::cout << "✓ Basic coordinator operation test passed" << std::endl;
    return 0;
}
```

### Test 2: Cluster Manager with Health Monitoring

**Objective**: Verify cluster manager tracks node health and capabilities

**Steps**:
1. Initialize ClusterManager
2. Add nodes with different capabilities
3. Update node health status
4. Query healthy nodes
5. Query nodes by capability

**Expected Results**:
- Nodes are tracked correctly
- Health updates are reflected
- Capability queries return correct nodes

### Test 3: Load Balancer Strategy Testing

**Objective**: Verify all load balancing strategies work correctly

**Steps**:
1. Create LoadBalancer with each strategy
2. Submit multiple tasks
3. Verify distribution follows strategy
4. Test adaptive mode
5. Verify rebalancing suggestions

**Expected Results**:
- Round-robin distributes evenly
- Least-loaded assigns to lowest load
- Weighted random respects capacity
- Adaptive mode switches strategies appropriately

### Test 4: AtomSpace Integration

**Objective**: Verify distributed state is properly represented in AtomSpace

**Steps**:
1. Create cluster with nodes and tasks
2. Query AtomSpace for node atoms
3. Verify link structures
4. Check task atoms

**Expected Results**:
- Compute nodes stored as ConceptNodes
- Capabilities stored as EvaluationLinks
- Tasks stored with correct type information
- Results accessible via AtomSpace queries

**Test Script**:
```cpp
// Create cluster
ClusterManager cluster(atomspace, "test-cluster");
cluster.initialize();

// Add node
NodeCapabilities caps;
caps.cpu_cores = 8;
caps.memory_mb = 16384;
cluster.addNode("node1", "192.168.1.10", 8080, caps);

// Query AtomSpace
Handle node_atom = atomspace->get_node(CONCEPT_NODE, "ClusterNode:node1");
assert(node_atom != Handle::UNDEFINED);

Handle cluster_atom = atomspace->get_node(CONCEPT_NODE, "Cluster:test-cluster");
assert(cluster_atom != Handle::UNDEFINED);

// Verify link exists
HandleSeq outgoing = {node_atom, cluster_atom};
Handle link = atomspace->get_link(MEMBER_LINK, outgoing);
assert(link != Handle::UNDEFINED);

std::cout << "✓ AtomSpace integration test passed" << std::endl;
```

### Test 5: Multi-Strategy Performance

**Objective**: Compare performance of different strategies under load

**Steps**:
1. Create 1000 tasks
2. Create 10 nodes
3. Test each strategy
4. Measure distribution variance
5. Measure assignment time

**Expected Results**:
- All strategies complete successfully
- Round-robin has lowest variance
- Least-loaded adapts to changing load
- Performance is acceptable (<1ms per assignment)

### Test 6: Failure Recovery

**Objective**: Verify system handles node failures gracefully

**Steps**:
1. Start cluster with 5 nodes
2. Submit tasks
3. Mark 2 nodes as unhealthy
4. Perform health check
5. Submit more tasks
6. Verify tasks assigned only to healthy nodes

**Expected Results**:
- Unhealthy nodes detected
- New tasks not assigned to unhealthy nodes
- Existing tasks on failed nodes identified
- Cluster statistics reflect current state

## Performance Benchmarks

### Benchmark 1: Node Registration Throughput

**Target**: > 1000 registrations/second

```bash
# Run benchmark
./agentzero-distributed/tests/NodeRegistrationBenchmark
```

### Benchmark 2: Task Assignment Latency

**Target**: < 1ms per task assignment

```bash
# Run benchmark
./agentzero-distributed/tests/TaskAssignmentBenchmark
```

### Benchmark 3: Health Check Overhead

**Target**: < 10ms for 100 nodes

```bash
# Run benchmark
./agentzero-distributed/tests/HealthCheckBenchmark
```

## Troubleshooting

### Common Issues

#### 1. CMake Cannot Find OpenCog Dependencies

**Symptom**: `Package 'cogutil' not found` or similar error

**Solution**:
```bash
# Set PKG_CONFIG_PATH
export PKG_CONFIG_PATH=/usr/local/share/opencog/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH

# Verify packages can be found
pkg-config --list-all | grep opencog
```

#### 2. Linking Errors

**Symptom**: Undefined reference to OpenCog symbols

**Solution**:
```bash
# Update library cache
sudo ldconfig

# Verify libraries are found
ldconfig -p | grep opencog
```

#### 3. Tests Fail to Run

**Symptom**: Tests crash or fail to start

**Solution**:
```bash
# Check library paths
ldd ./DistributedCoordinatorTest

# Ensure all dependencies are loaded
LD_LIBRARY_PATH=/usr/local/lib/opencog:$LD_LIBRARY_PATH ./DistributedCoordinatorTest
```

#### 4. GTest Not Found

**Symptom**: `GTest required but not found`

**Solution**:
```bash
# Install GTest
sudo apt-get install libgtest-dev googletest

# Or build without tests
cmake .. -DBUILD_TESTING=OFF
```

## Continuous Integration

### GitHub Actions Workflow

Add the following to `.github/workflows/test-distributed.yml`:

```yaml
name: Test Distributed Computing Module

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Install Dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y libboost-all-dev guile-3.0-dev \
          build-essential cmake libgtest-dev
    
    - name: Build cogutil
      run: |
        mkdir -p build && cd build
        cmake ..
        make cogutil
        cd cogutil-build && sudo make install && sudo ldconfig
    
    - name: Build atomspace
      run: |
        cd build
        make atomspace
        cd atomspace-build && sudo make install && sudo ldconfig
    
    - name: Build Distributed Module
      run: |
        cd agents/cpp
        mkdir -p build && cd build
        cmake .. -DBUILD_TESTING=ON
        make agentzero-distributed
    
    - name: Run Tests
      run: |
        cd agents/cpp/build
        make test
```

## Validation Checklist

Before considering AZ-SCALE-001 complete, verify:

- [ ] All unit tests pass
- [ ] Integration tests pass
- [ ] Performance benchmarks meet targets
- [ ] Code compiles without warnings
- [ ] Memory leaks checked (valgrind)
- [ ] Thread safety verified
- [ ] Documentation complete
- [ ] Examples run successfully
- [ ] CI/CD pipeline passes

## Acceptance Criteria Verification

### Implementation follows OpenCog architectural patterns
- [ ] Uses AtomSpace for knowledge representation
- [ ] Follows OpenCog naming conventions
- [ ] Integrates with existing OpenCog components

### Code is well-documented
- [ ] All public interfaces have documentation
- [ ] README explains usage
- [ ] Examples demonstrate key features

### Unit tests provide adequate coverage
- [ ] > 80% code coverage
- [ ] All public methods tested
- [ ] Edge cases covered

### Integration tests verify OpenCog compatibility
- [ ] AtomSpace integration works
- [ ] No conflicts with other components
- [ ] Can be used alongside other OpenCog modules

### Performance meets specified targets
- [ ] Task assignment < 1ms
- [ ] Node registration > 1000/sec
- [ ] Health check < 10ms for 100 nodes

### Memory usage is optimized
- [ ] No memory leaks
- [ ] Efficient data structures used
- [ ] Memory scales linearly with cluster size

### Error handling is robust
- [ ] All errors logged appropriately
- [ ] Graceful degradation on failures
- [ ] No unhandled exceptions

## Report Template

After running integration tests, use this template to report results:

```markdown
# AZ-SCALE-001 Integration Test Report

## Test Environment
- OS: Ubuntu 24.04
- Compiler: GCC 13.3.0
- Boost Version: 1.83.0
- cogutil Version: X.X.X
- atomspace Version: X.X.X

## Unit Test Results
- Total Tests: XX
- Passed: XX
- Failed: XX
- Duration: XX seconds

## Integration Test Results
| Test | Status | Duration | Notes |
|------|--------|----------|-------|
| Basic Coordinator | ✓ | 0.5s | All operations successful |
| Cluster Manager | ✓ | 0.3s | Health monitoring working |
| Load Balancer | ✓ | 1.2s | All strategies functional |
| AtomSpace Integration | ✓ | 0.4s | Proper storage verified |
| Multi-Strategy Performance | ✓ | 3.5s | Performance acceptable |
| Failure Recovery | ✓ | 0.8s | Graceful degradation |

## Performance Benchmarks
- Node Registration: XXXX/sec (Target: >1000/sec)
- Task Assignment: X.XXms (Target: <1ms)
- Health Check: XXms for 100 nodes (Target: <10ms)

## Issues Found
1. [List any issues discovered]
2. [...]

## Recommendations
1. [Any recommendations for improvements]
2. [...]

## Conclusion
[Overall assessment of integration testing results]
```

## Next Steps

After successful integration testing:

1. Address any issues found
2. Run performance profiling
3. Conduct security review
4. Update documentation with findings
5. Request code review
6. Merge to main branch
7. Tag release

## References

- [AGENT-ZERO-GENESIS.md](../../../AGENT-ZERO-GENESIS.md) - Project roadmap
- [README.md](../README.md) - Module documentation
- [OpenCog Testing Guide](https://wiki.opencog.org/w/Testing)
- [AtomSpace Documentation](https://wiki.opencog.org/w/AtomSpace)
