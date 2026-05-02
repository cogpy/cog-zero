# Agent-Zero Testing Guide

Comprehensive guide for testing Agent-Zero components.

## Table of Contents

1. [Testing Philosophy](#testing-philosophy)
2. [Test Structure](#test-structure)
3. [Unit Testing](#unit-testing)
4. [Integration Testing](#integration-testing)
5. [Performance Testing](#performance-testing)
6. [Test Coverage](#test-coverage)
7. [Continuous Integration](#continuous-integration)

## Testing Philosophy

Agent-Zero follows test-driven development principles:

- **Comprehensive Coverage**: Aim for >80% code coverage
- **Fast Feedback**: Unit tests run in seconds
- **Isolated Tests**: Each test is independent
- **Clear Assertions**: Tests clearly show expected behavior
- **Maintainable**: Tests are easy to understand and modify

## Test Structure

```
agents/cpp/
├── agentzero-core/
│   └── tests/
│       ├── AgentZeroCoreTest.cpp
│       ├── CognitiveLoopTest.cpp
│       └── TaskManagerTest.cpp
│
├── agentzero-perception/
│   └── tests/
│       ├── PerceptualProcessorTest.cpp
│       └── MultiModalSensorTest.cpp
│
└── tests/                    # Integration tests
    ├── IntegrationTest.cpp
    └── PerformanceTest.cpp
```

## Unit Testing

### Test Framework

Agent-Zero uses CxxTest for unit testing.

#### Basic Test Structure

```cpp
#include <cxxtest/TestSuite.h>
#include <agentzero/AgentZeroCore.h>
#include <opencog/atomspace/AtomSpace.h>

class AgentZeroCoreTest : public CxxTest::TestSuite {
private:
    std::unique_ptr<opencog::AtomSpace> atomspace;
    std::unique_ptr<agentzero::AgentZeroCore> agent;
    
public:
    void setUp() override {
        // Run before each test
        atomspace = std::make_unique<opencog::AtomSpace>();
        agent = std::make_unique<agentzero::AgentZeroCore>(*atomspace);
        agent->initialize();
    }
    
    void tearDown() override {
        // Run after each test
        agent.reset();
        atomspace.reset();
    }
    
    void testInitialization() {
        TS_ASSERT(agent->isInitialized());
    }
    
    void testAddGoal() {
        opencog::Handle goal = agent->addGoal("Test goal", 0.8f);
        TS_ASSERT(goal != opencog::Handle::UNDEFINED);
        TS_ASSERT_EQUALS(agent->getGoalPriority(goal), 0.8f);
    }
    
    void testCognitiveStep() {
        TS_ASSERT_THROWS_NOTHING(agent->cognitiveStep());
    }
};
```

### Running Unit Tests

```bash
# Build with tests enabled
cd agents/cpp/build
cmake -DBUILD_TESTING=ON ..
make -j$(nproc)

# Run all tests
make test

# Run with verbose output
ctest -V

# Run specific test suite
ctest -R AgentZeroCoreTest

# Run specific test
ctest -R AgentZeroCoreTest.testAddGoal
```

### Testing Best Practices

#### 1. Test One Thing

```cpp
// ✅ Good: Tests single functionality
void testGoalPriorityRetrieval() {
    Handle goal = agent->addGoal("Test", 0.7f);
    TS_ASSERT_EQUALS(agent->getGoalPriority(goal), 0.7f);
}

// ❌ Bad: Tests multiple things
void testGoalManagement() {
    Handle goal = agent->addGoal("Test", 0.7f);
    TS_ASSERT_EQUALS(agent->getGoalPriority(goal), 0.7f);
    agent->removeGoal(goal);
    TS_ASSERT_THROWS(agent->getGoalPriority(goal), ...);
    // ... more assertions
}
```

#### 2. Clear Test Names

```cpp
// ✅ Good: Descriptive name
void testAddGoalWithInvalidPriorityThrowsException()

// ❌ Bad: Vague name
void testGoals()
```

#### 3. Arrange-Act-Assert Pattern

```cpp
void testGoalDecomposition() {
    // Arrange: Set up test data
    Handle parentGoal = agent->addGoal("Parent", 0.9f);
    
    // Act: Execute the operation
    Handle childGoal = agent->addSubGoal(parentGoal, "Child");
    
    // Assert: Verify results
    TS_ASSERT(childGoal != Handle::UNDEFINED);
    auto children = agent->getSubGoals(parentGoal);
    TS_ASSERT_EQUALS(children.size(), 1);
    TS_ASSERT_EQUALS(children[0], childGoal);
}
```

#### 4. Test Edge Cases

```cpp
void testEmptyGoalDescription() {
    TS_ASSERT_THROWS(agent->addGoal(""), InvalidArgumentException);
}

void testGoalPriorityBounds() {
    // Test lower bound
    TS_ASSERT_THROWS(agent->addGoal("Test", -0.1f), 
                    InvalidArgumentException);
    
    // Test upper bound
    TS_ASSERT_THROWS(agent->addGoal("Test", 1.1f),
                    InvalidArgumentException);
    
    // Test valid boundaries
    TS_ASSERT_THROWS_NOTHING(agent->addGoal("Test", 0.0f));
    TS_ASSERT_THROWS_NOTHING(agent->addGoal("Test", 1.0f));
}
```

### Mocking Dependencies

Use test doubles for external dependencies:

```cpp
class MockPerceptualProcessor : public PerceptualProcessor {
public:
    MockPerceptualProcessor(AtomSpace& as) : PerceptualProcessor(as) {}
    
    void process(const SensoryData& input) override {
        processCallCount++;
        lastInput = input;
    }
    
    int processCallCount = 0;
    SensoryData lastInput;
};

class PerceptionIntegrationTest : public CxxTest::TestSuite {
    void testPerceptionProcessing() {
        AtomSpace as;
        MockPerceptualProcessor processor(as);
        
        SensoryData data = createTestData();
        processor.process(data);
        
        TS_ASSERT_EQUALS(processor.processCallCount, 1);
        TS_ASSERT_EQUALS(processor.lastInput, data);
    }
};
```

## Integration Testing

### Testing Component Interactions

```cpp
#include <cxxtest/TestSuite.h>
#include <agentzero/AgentZeroCore.h>
#include <agentzero/PerceptualProcessor.h>
#include <agentzero/ReasoningEngine.h>

class CognitiveLoopIntegrationTest : public CxxTest::TestSuite {
public:
    void testPerceptionToReasoning() {
        // Setup
        AtomSpace atomspace;
        AgentZeroCore agent(atomspace);
        agent.initialize();
        
        // Add test percept
        Handle percept = atomspace.add_node(CONCEPT_NODE, "TestPercept");
        
        // Run cognitive step
        agent.cognitiveStep();
        
        // Verify perception was processed
        auto reasoning = agent.getComponent<ReasoningEngine>("reasoning");
        auto inferences = reasoning->getRecentInferences();
        
        TS_ASSERT(!inferences.empty());
    }
    
    void testEndToEndGoalAchievement() {
        AtomSpace atomspace;
        AgentZeroCore agent(atomspace);
        agent.initialize();
        
        // Set simple goal
        Handle goal = agent.addGoal("Reach target state");
        
        // Run multiple cognitive steps
        for (int i = 0; i < 100; i++) {
            agent.cognitiveStep();
            
            if (agent.getGoalStatus(goal) == GoalStatus::Achieved) {
                break;
            }
        }
        
        // Verify goal achievement
        TS_ASSERT_EQUALS(agent.getGoalStatus(goal), GoalStatus::Achieved);
    }
};
```

### Testing OpenCog Integration

```cpp
class OpenCogIntegrationTest : public CxxTest::TestSuite {
public:
    void testAtomSpaceIntegration() {
        AtomSpace as;
        AgentZeroCore agent(as);
        
        // Add goal - should create atoms in AtomSpace
        Handle goal = agent.addGoal("Test", 0.5f);
        
        // Verify atoms were created
        HandleSeq goals = as.get_atoms_by_type(EVALUATION_LINK);
        TS_ASSERT(!goals.empty());
    }
    
    void testPLNIntegration() {
        AtomSpace as;
        AgentZeroCore agent(as);
        auto reasoning = agent.getComponent<ReasoningEngine>("reasoning");
        
        // Add premise
        Handle premise = as.add_link(INHERITANCE_LINK,
            as.add_node(CONCEPT_NODE, "A"),
            as.add_node(CONCEPT_NODE, "B"));
        
        // Run inference
        Handle result = reasoning->infer(premise, 10);
        
        // Verify inference occurred
        TS_ASSERT(result != Handle::UNDEFINED);
    }
};
```

## Performance Testing

### Benchmarking Framework

```cpp
#include <chrono>
#include <cxxtest/TestSuite.h>

class PerformanceTest : public CxxTest::TestSuite {
private:
    template<typename Func>
    double measureTime(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        
        std::chrono::duration<double, std::milli> duration = end - start;
        return duration.count();
    }
    
public:
    void testCognitiveStepPerformance() {
        AtomSpace as;
        AgentZeroCore agent(as);
        agent.initialize();
        
        // Warmup
        for (int i = 0; i < 10; i++) {
            agent.cognitiveStep();
        }
        
        // Benchmark
        double totalTime = measureTime([&agent]() {
            for (int i = 0; i < 100; i++) {
                agent.cognitiveStep();
            }
        });
        
        double avgTime = totalTime / 100.0;
        
        // Assert performance target: < 100ms per step
        TS_ASSERT_LESS_THAN(avgTime, 100.0);
        
        std::cout << "Average cognitive step time: " 
                  << avgTime << " ms" << std::endl;
    }
    
    void testMemoryUsage() {
        AtomSpace as;
        AgentZeroCore agent(as);
        
        size_t initialMemory = getCurrentMemoryUsage();
        
        // Add many goals
        for (int i = 0; i < 10000; i++) {
            agent.addGoal("Goal " + std::to_string(i), 0.5f);
        }
        
        size_t finalMemory = getCurrentMemoryUsage();
        size_t memoryIncrease = finalMemory - initialMemory;
        
        // Assert reasonable memory usage (< 100 MB for 10k goals)
        TS_ASSERT_LESS_THAN(memoryIncrease, 100 * 1024 * 1024);
        
        std::cout << "Memory increase for 10k goals: " 
                  << (memoryIncrease / 1024 / 1024) << " MB" << std::endl;
    }
};
```

### Performance Targets

| Operation | Target | Maximum |
|-----------|--------|---------|
| Cognitive Step | < 100ms | 200ms |
| Goal Addition | < 1ms | 10ms |
| Pattern Matching (simple) | < 10ms | 50ms |
| PLN Inference (10 steps) | < 50ms | 200ms |
| Memory per 1K atoms | < 10MB | 50MB |

### Running Performance Tests

```bash
# Build with optimizations
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Run performance tests
ctest -R PerformanceTest -V

# With profiling
perf record ctest -R PerformanceTest
perf report
```

## Test Coverage

### Measuring Coverage

```bash
# Install coverage tools
sudo apt-get install -y lcov

# Build with coverage flags
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="--coverage" \
      ..
make -j$(nproc)

# Run tests
make test

# Generate coverage report
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
lcov --list coverage.info

# Generate HTML report
genhtml coverage.info --output-directory coverage_html
```

### Coverage Targets

- **Overall**: > 80% line coverage
- **Core Components**: > 90% line coverage
- **Public APIs**: 100% line coverage
- **Critical Paths**: 100% line coverage

### Coverage Report Interpretation

```
File 'AgentZeroCore.cpp'
Lines executed: 87.5% of 240
Branches executed: 82.3% of 156
```

Focus on:
1. Uncovered lines in critical paths
2. Uncovered error handling code
3. Uncovered edge cases

## Continuous Integration

### GitHub Actions Workflow

```yaml
name: Agent-Zero Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Install Dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y libboost-all-dev cxxtest
    
    - name: Build OpenCog Dependencies
      run: |
        mkdir build && cd build
        cmake ..
        make cogutil atomspace cogserver
    
    - name: Build Agent-Zero
      run: |
        cd agents/cpp
        mkdir build && cd build
        cmake -DBUILD_TESTING=ON ..
        make -j$(nproc)
    
    - name: Run Tests
      run: |
        cd agents/cpp/build
        ctest --output-on-failure
    
    - name: Generate Coverage
      if: matrix.build_type == 'Debug'
      run: |
        lcov --capture --directory . --output-file coverage.info
        bash <(curl -s https://codecov.io/bash)
```

### Pre-commit Hooks

```bash
# Install pre-commit hook
cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash

# Run tests before commit
cd agents/cpp/build
make test

if [ $? -ne 0 ]; then
    echo "Tests failed! Commit aborted."
    exit 1
fi

exit 0
EOF

chmod +x .git/hooks/pre-commit
```

## Test Organization

### Naming Conventions

- Test files: `{Component}Test.cpp`
- Test classes: `{Component}Test`
- Test methods: `test{Functionality}`

### Test Categories

Use test labels for organization:

```cpp
// Unit tests
void testBasicFunctionality() { /* ... */ }

// Integration tests  
void testComponentIntegration() { /* ... */ }

// Performance tests
void testPerformance() { /* ... */ }

// Regression tests
void testBugFix_Issue123() { /* ... */ }
```

## Debugging Failed Tests

### Common Issues

**Test Segfaults**
```bash
# Run with GDB
gdb --args ./test_executable --verbose
```

**Memory Leaks**
```bash
# Run with Valgrind
valgrind --leak-check=full ./test_executable
```

**Test Timeouts**
```bash
# Increase timeout
ctest --timeout 300
```

### Test-Specific Logging

```cpp
void testComplexOperation() {
    // Enable detailed logging for test
    logger().set_level(Logger::DEBUG);
    
    // Run test
    agent->complexOperation();
    
    // Restore normal logging
    logger().set_level(Logger::INFO);
}
```

## Best Practices Summary

1. ✅ **Write tests first** (TDD)
2. ✅ **Keep tests fast** (< 1 second per unit test)
3. ✅ **Make tests independent** (no shared state)
4. ✅ **Test edge cases** (boundaries, errors)
5. ✅ **Use descriptive names** (test intent clear)
6. ✅ **Mock external dependencies** (isolate components)
7. ✅ **Measure coverage** (aim for > 80%)
8. ✅ **Run tests frequently** (every commit)
9. ✅ **Fix broken tests immediately** (don't accumulate)
10. ✅ **Review test code** (same standards as production)

---

*Part of the AGENT-ZERO-GENESIS documentation - Phase 9: Integration & Testing (AZ-DOC-001)*
