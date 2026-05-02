# Agent-Zero Comprehensive Integration Tests

## Overview

This directory contains comprehensive integration tests for the Agent-Zero cognitive architecture, validating end-to-end functionality and deep integration with OpenCog components (cogutil and atomspace).

**Task ID**: AZ-INT-001 - Create comprehensive integration tests  
**Phase**: 9 - Integration & Testing  
**Status**: Complete

## Test Suites

### 1. ComponentIntegrationTest.cxxtest

Tests cross-component interactions and data flow between Agent-Zero modules.

**Coverage**: 10 comprehensive tests
- Agent Core and Cognitive Loop Integration
- Task Manager and Knowledge Integrator Coordination  
- Cognitive Loop Multi-Phase Integration
- Goal Hierarchy and Task Decomposition
- Knowledge Integration with Reasoning
- Complete Agent Lifecycle Integration
- Multi-Agent Coordination
- Error Recovery and Resilience
- Performance Under Load
- State Persistence and Recovery

**Key Features**:
- Validates seamless communication between components
- Tests goal decomposition and hierarchical task management
- Verifies multi-agent coordination through shared AtomSpace
- Ensures robust error handling and recovery mechanisms

### 2. AtomSpaceIntegrationTest.cxxtest

Tests deep integration between Agent-Zero and OpenCog's AtomSpace and cogutil libraries.

**Coverage**: 15 comprehensive tests
- Basic AtomSpace Operations
- TruthValue Integration
- Value Attachments  
- Atom Type Hierarchy
- Pattern Matching Integration
- Knowledge Integration Operations
- AtomSpace Bulk Operations
- Attention Allocation Integration
- AtomSpace Filtering and Queries
- AtomSpace Atomic Operations
- Logger Integration (cogutil)
- AtomSpace Memory Management
- Complex Graph Structures
- Concurrent Access Safety
- Agent-Zero Specific Types

**Key Features**:
- Validates all core AtomSpace operations
- Tests TruthValue propagation and uncertainty handling
- Verifies pattern matching and knowledge queries
- Ensures proper memory management and cleanup
- Tests attention allocation (STI values)
- Validates cogutil logger integration

### 3. CogServerIntegrationTest.cxxtest

Tests Agent-Zero integration with OpenCog's CogServer for network access and module management.

**Coverage**: 15 comprehensive tests
- CogServer Basic Initialization
- Agent Registration with CogServer
- Command Execution Through CogServer
- Network Access Through CogServer
- Shared AtomSpace Through CogServer
- CogServer Request/Response Pattern
- Module Lifecycle Management
- CogServer Shell Interface
- Multi-Client Connections
- CogServer Event Broadcasting
- Agent Status Monitoring
- Dynamic Configuration Updates
- Error Handling and Recovery
- Performance Monitoring Integration
- Integration with External Tools

**Key Features**:
- Tests agent registration as CogServer module
- Validates network command execution
- Ensures multi-client connection handling
- Tests event broadcasting to connected clients
- Verifies dynamic configuration updates
- Tests shell interface commands

### 4. EndToEndIntegrationTest.cxxtest

Tests complete Agent-Zero workflows from perception to action.

**Coverage**: 10 comprehensive end-to-end tests
- Complete Perception-Action Loop
- Goal Hierarchy Resolution
- Knowledge Acquisition and Application
- Adaptive Behavior Under Changing Conditions
- Multi-Agent Collaboration Scenario
- Long-Running Agent Lifecycle
- Error Recovery and Resilience
- Performance Under Stress
- State Persistence and Restoration
- Complete Agent-Zero Demonstration

**Key Features**:
- Validates full cognitive cycle: perception → attention → reasoning → planning → action → learning
- Tests hierarchical goal decomposition and achievement
- Verifies knowledge acquisition from experience and application to new situations
- Tests agent adaptation to environmental changes
- Validates multi-agent collaboration on shared goals
- Tests extended operation stability (100+ cognitive cycles)
- Ensures graceful error recovery
- Stress tests with high load (200+ cycles, 100+ goals)
- Tests state persistence across agent restart

## Test Execution

### Building Tests

```bash
cd /tmp/opencog-build
cmake /home/runner/work/pycog0/pycog0
make agentzero-integration-tests
```

### Running All Integration Tests

```bash
make run-integration-tests
```

Or using ctest directly:

```bash
ctest -R ".*IntegrationTest" --output-on-failure
```

### Running Individual Test Suites

```bash
# Component integration tests
ctest -R "ComponentIntegrationTest" --output-on-failure

# AtomSpace integration tests  
ctest -R "AtomSpaceIntegrationTest" --output-on-failure

# CogServer integration tests
ctest -R "CogServerIntegrationTest" --output-on-failure

# End-to-end integration tests
ctest -R "EndToEndIntegrationTest" --output-on-failure
```

## Test Framework

All integration tests inherit from `IntegrationTestFramework`, which provides:

### Core Features
- **AtomSpace Management**: Automatic setup and teardown of test AtomSpace
- **CogServer Integration**: Mock CogServer for testing server-based functionality
- **Performance Tracking**: Execution time, memory usage, and atom creation metrics
- **Integrity Validation**: Automatic AtomSpace consistency checking
- **Test Utilities**: Helper methods for common test operations

### Performance Targets

Integration tests have relaxed performance targets compared to unit tests:

- **Max Execution Time**: 1000ms per test
- **Max Memory Usage**: 10MB per test
- **Max Atoms**: 10,000 atoms per test

### Enhanced Assertions

```cpp
TS_ASSERT_PERFORMANCE_OK(*this);      // Validates performance within limits
TS_ASSERT_ATOMSPACE_INTEGRITY(*this); // Checks AtomSpace consistency
TS_ASSERT_MEMORY_USAGE_OK(*this);     // Validates memory usage
```

## Dependencies

These integration tests require:

### Required Components
- **cogutil**: Core OpenCog utilities (Logger, Config, etc.)
- **atomspace**: AtomSpace knowledge representation
- **CxxTest**: Testing framework

### Optional Components (for full functionality)
- **cogserver**: CogServer network interface
- **attention**: ECAN attention allocation
- **pln**: Probabilistic Logic Networks
- **ure**: Unified Rule Engine

## Test Coverage

### Component Coverage Matrix

| Component | Unit Tests | Integration Tests | End-to-End Tests |
|-----------|------------|-------------------|------------------|
| AgentZeroCore | ✓ | ✓ | ✓ |
| CognitiveLoop | ✓ | ✓ | ✓ |
| TaskManager | ✓ | ✓ | ✓ |
| KnowledgeIntegrator | ✓ | ✓ | ✓ |
| AtomSpace Integration | - | ✓ | ✓ |
| CogServer Integration | - | ✓ | ✓ |
| Multi-Agent | - | ✓ | ✓ |

### Scenario Coverage

- ✅ Perception-Action cycles
- ✅ Goal decomposition and achievement
- ✅ Knowledge acquisition and learning
- ✅ Attention allocation
- ✅ Pattern matching and reasoning
- ✅ Multi-agent collaboration
- ✅ Error recovery and resilience
- ✅ Long-running stability
- ✅ State persistence
- ✅ Performance under load

## Success Criteria (AZ-INT-001)

All acceptance criteria met:

- ✅ **Implementation follows OpenCog architectural patterns**
  - Uses standard AtomSpace operations and patterns
  - Follows OpenCog naming conventions and code style
  - Integrates with cogutil and atomspace libraries

- ✅ **Code is well-documented with clear interfaces**
  - Comprehensive documentation for each test suite
  - Clear test names and descriptions
  - Inline comments explaining complex test logic

- ✅ **Unit tests provide adequate coverage**
  - 50 total integration tests across 4 test suites
  - Tests all major components and interactions
  - Validates both success and error paths

- ✅ **Integration tests verify OpenCog compatibility**
  - Deep AtomSpace integration testing (15 tests)
  - CogServer module integration testing (15 tests)
  - Component interaction testing (10 tests)
  - End-to-end workflow testing (10 tests)

- ✅ **Performance meets specified targets**
  - All tests execute within performance targets
  - Stress tests validate high-load behavior
  - Long-running tests verify extended stability

- ✅ **Memory usage is optimized**
  - Proper cleanup and memory management
  - No memory leaks detected
  - AtomSpace integrity maintained

- ✅ **Error handling is robust**
  - Comprehensive error recovery tests
  - Graceful handling of invalid inputs
  - System stability under error conditions

## Continuous Integration

These tests are integrated into the OpenCog build system and can be run as part of CI/CD pipelines:

```yaml
# Example GitHub Actions integration
- name: Build Agent-Zero Integration Tests
  run: |
    mkdir -p build && cd build
    cmake ..
    make agentzero-integration-tests

- name: Run Integration Tests
  run: |
    cd build
    ctest -R ".*IntegrationTest" --output-on-failure
```

## Troubleshooting

### Common Issues

**Issue**: Tests fail with "CxxTest not found"
- **Solution**: Install CxxTest: `sudo apt-get install cxxtest`

**Issue**: Tests fail with "cogutil not found"
- **Solution**: Install cogutil first: `make cogutil && cd cogutil-build && sudo make install && sudo ldconfig`

**Issue**: Tests fail with "atomspace not found"  
- **Solution**: Install atomspace: `make atomspace && cd atomspace-build && sudo make install && sudo ldconfig`

**Issue**: Performance tests timeout
- **Solution**: Increase timeout in CMakeLists.txt or run with `ctest --timeout 600`

### Debugging Tests

Enable verbose output:

```bash
ctest -R "IntegrationTest" --verbose
```

Run specific test:

```bash
./agents/cpp/tests/integration-tests/ComponentIntegrationTest
```

Enable debug logging in test:

```cpp
logger().set_level(Logger::DEBUG);
logger().set_print_to_stdout_flag(true);
```

## Future Enhancements

Potential areas for expansion:

- **Performance Profiling**: Add detailed profiling data collection
- **Distributed Testing**: Test multi-node Agent-Zero configurations
- **Visualization**: Generate visual reports of test execution and coverage
- **Fuzz Testing**: Add randomized testing for robustness validation
- **Regression Tracking**: Automated performance regression detection

## References

- [Agent-Zero Genesis Roadmap](../../../AGENT-ZERO-GENESIS.md)
- [Testing Framework Guide](../TESTING_FRAMEWORK_GUIDE.md)
- [OpenCog AtomSpace Documentation](https://wiki.opencog.org/w/AtomSpace)
- [OpenCog CogServer Documentation](https://wiki.opencog.org/w/CogServer)

## Maintainers

For questions or issues related to these integration tests:

- Review the [Agent-Zero Genesis Roadmap](../../../AGENT-ZERO-GENESIS.md)
- Check existing GitHub issues
- Create a new issue with label `AZ-INT-001` or `integration-tests`

---

**Last Updated**: 2024-12-06  
**Task**: AZ-INT-001 - Create comprehensive integration tests  
**Status**: ✅ Complete
