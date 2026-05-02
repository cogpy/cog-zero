# Agent-Zero Benchmarking Guide

Guide for performance testing and benchmarking Agent-Zero.

## Table of Contents

1. [Benchmarking Philosophy](#benchmarking-philosophy)
2. [Performance Metrics](#performance-metrics)
3. [Benchmark Suite](#benchmark-suite)
4. [Running Benchmarks](#running-benchmarks)
5. [Analysis and Interpretation](#analysis-and-interpretation)
6. [Optimization](#optimization)

## Benchmarking Philosophy

### Goals

- **Measure Performance**: Quantify system performance
- **Identify Bottlenecks**: Find areas needing optimization
- **Track Progress**: Monitor performance over time
- **Validate Optimizations**: Confirm improvements work
- **Set Baselines**: Establish performance expectations

### Principles

1. **Repeatable**: Benchmarks must be reproducible
2. **Realistic**: Use realistic workloads
3. **Isolated**: Minimize external interference
4. **Comprehensive**: Cover all critical paths
5. **Automated**: Integrate with CI/CD

## Performance Metrics

### Latency Metrics

| Metric | Target | Maximum | Critical |
|--------|--------|---------|----------|
| Cognitive Step | < 100ms | 200ms | Yes |
| Goal Addition | < 1ms | 10ms | No |
| Pattern Match (simple) | < 10ms | 50ms | Yes |
| PLN Inference (10 steps) | < 50ms | 200ms | Yes |
| Perception Processing | < 20ms | 100ms | Yes |
| Action Execution | < 5ms | 20ms | Yes |

### Throughput Metrics

| Operation | Target | Minimum |
|-----------|--------|---------|
| Cognitive Steps/sec | 10+ | 5 |
| Goals Added/sec | 1000+ | 100 |
| Atoms Created/sec | 10000+ | 1000 |
| Inferences/sec | 100+ | 10 |

### Resource Metrics

| Resource | Target | Maximum |
|----------|--------|---------|
| Memory per 1K atoms | < 10 MB | 50 MB |
| CPU per cognitive step | < 10% | 50% |
| AtomSpace size (working) | < 100K atoms | 1M atoms |

## Benchmark Suite

### Microbenchmarks

Test individual operations:

```cpp
#include <benchmark/benchmark.h>
#include <agentzero/AgentZeroCore.h>

// Benchmark goal addition
static void BM_AddGoal(benchmark::State& state) {
    AtomSpace atomspace;
    AgentZeroCore agent(atomspace);
    agent.initialize();
    
    for (auto _ : state) {
        Handle goal = agent.addGoal("Test goal", 0.5f);
        benchmark::DoNotOptimize(goal);
    }
}
BENCHMARK(BM_AddGoal);

// Benchmark cognitive step
static void BM_CognitiveStep(benchmark::State& state) {
    AtomSpace atomspace;
    AgentZeroCore agent(atomspace);
    agent.initialize();
    
    for (auto _ : state) {
        agent.cognitiveStep();
    }
}
BENCHMARK(BM_CognitiveStep);

// Benchmark pattern matching
static void BM_PatternMatch(benchmark::State& state) {
    AtomSpace atomspace;
    setupTestData(atomspace, state.range(0));
    
    Handle pattern = createTestPattern(atomspace);
    
    for (auto _ : state) {
        HandleSeq results = satisfying_set(&atomspace, pattern);
        benchmark::DoNotOptimize(results);
    }
}
BENCHMARK(BM_PatternMatch)->Range(100, 10000);

BENCHMARK_MAIN();
```

### Component Benchmarks

Test full components:

```cpp
static void BM_PerceptionPipeline(benchmark::State& state) {
    AtomSpace atomspace;
    PerceptualProcessor processor(atomspace);
    
    SensoryData testData = createTestData(state.range(0));
    
    for (auto _ : state) {
        processor.process(testData);
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_PerceptionPipeline)->Range(10, 1000);

static void BM_ReasoningEngine(benchmark::State& state) {
    AtomSpace atomspace;
    ReasoningEngine engine(atomspace);
    
    setupKnowledgeBase(atomspace, 1000);
    Handle premise = createTestPremise(atomspace);
    
    for (auto _ : state) {
        Handle result = engine.infer(premise, state.range(0));
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_ReasoningEngine)->Range(10, 100);
```

### System Benchmarks

End-to-end scenarios:

```cpp
static void BM_FullCognitiveLoop(benchmark::State& state) {
    AtomSpace atomspace;
    AgentZeroCore agent(atomspace);
    agent.initialize();
    
    // Setup scenario
    agent.addGoal("Complete task", 0.9f);
    setupEnvironment(atomspace);
    
    for (auto _ : state) {
        // Full cognitive cycle
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 10; i++) {
            agent.cognitiveStep();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();
        
        state.SetIterationTime(elapsed / 1000.0);
    }
}
BENCHMARK(BM_FullCognitiveLoop)->UseManualTime();
```

### Stress Tests

Test under load:

```cpp
static void BM_AtomSpaceStress(benchmark::State& state) {
    AtomSpace atomspace;
    
    // Pre-populate
    for (int i = 0; i < state.range(0); i++) {
        atomspace.add_node(CONCEPT_NODE, "node_" + std::to_string(i));
    }
    
    for (auto _ : state) {
        // Random operations
        int op = rand() % 3;
        if (op == 0) {
            // Add node
            atomspace.add_node(CONCEPT_NODE, "temp");
        } else if (op == 1) {
            // Query
            atomspace.get_atoms_by_type(CONCEPT_NODE);
        } else {
            // Pattern match
            HandleSeq results = randomPatternMatch(atomspace);
            benchmark::DoNotOptimize(results);
        }
    }
    
    state.counters["atoms"] = atomspace.get_size();
}
BENCHMARK(BM_AtomSpaceStress)->Range(1000, 1000000);
```

## Running Benchmarks

### Build with Benchmarks

```bash
# Install Google Benchmark
sudo apt-get install -y libbenchmark-dev

# Build with benchmarks enabled
cd agents/cpp
mkdir build-benchmark && cd build-benchmark
cmake -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_BENCHMARKS=ON \
      ..
make -j$(nproc)
```

### Running Benchmarks

```bash
# Run all benchmarks
./benchmarks/agentzero_benchmarks

# Run specific benchmark
./benchmarks/agentzero_benchmarks --benchmark_filter=BM_AddGoal

# Run with different configurations
./benchmarks/agentzero_benchmarks --benchmark_repetitions=10

# Output to file
./benchmarks/agentzero_benchmarks --benchmark_out=results.json \
                                  --benchmark_out_format=json
```

### Benchmark Options

```bash
# Common options
--benchmark_filter=<regex>          # Filter benchmarks
--benchmark_repetitions=<n>         # Repeat n times
--benchmark_min_time=<min_time>     # Min time per test
--benchmark_counters_tabular=true   # Tabular output
--benchmark_out=<file>              # Output file
--benchmark_out_format=<format>     # json|csv|console
```

## Analysis and Interpretation

### Reading Results

Example output:
```
--------------------------------------------------------------------
Benchmark                          Time             CPU   Iterations
--------------------------------------------------------------------
BM_AddGoal                      2.45 ms         2.44 ms          287
BM_CognitiveStep                 156 ms          155 ms            4
BM_PatternMatch/100             4.23 ms         4.22 ms          165
BM_PatternMatch/1000            12.8 ms         12.7 ms           54
BM_PatternMatch/10000            145 ms          144 ms            5
```

Interpretation:
- **Time**: Wall clock time per iteration
- **CPU**: CPU time per iteration  
- **Iterations**: Number of times test ran

### Comparing Results

```bash
# Compare two runs
compare.py baseline.json current.json

# Example output:
# Benchmark                     Time         CPU    Time Old    Time New
# BM_AddGoal                  +0.05      +0.04          2.45        2.57
# BM_CognitiveStep            -0.12      -0.11           156         137
```

### Statistical Analysis

```python
import json
import numpy as np

def analyze_benchmark(filename):
    with open(filename) as f:
        data = json.load(f)
    
    for benchmark in data['benchmarks']:
        times = benchmark['times']
        
        print(f"{benchmark['name']}:")
        print(f"  Mean: {np.mean(times):.2f} ms")
        print(f"  Median: {np.median(times):.2f} ms")
        print(f"  Std Dev: {np.std(times):.2f} ms")
        print(f"  95th percentile: {np.percentile(times, 95):.2f} ms")
```

### Identifying Bottlenecks

Use profiling tools:

```bash
# CPU profiling with perf
perf record -g ./benchmarks/agentzero_benchmarks
perf report

# Call graph profiling
valgrind --tool=callgrind ./benchmarks/agentzero_benchmarks
kcachegrind callgrind.out.*

# Memory profiling
valgrind --tool=massif ./benchmarks/agentzero_benchmarks
ms_print massif.out.*
```

## Optimization

### Optimization Process

1. **Measure**: Run benchmarks to establish baseline
2. **Profile**: Identify hotspots with profiling tools
3. **Optimize**: Make targeted improvements
4. **Verify**: Re-run benchmarks to confirm improvement
5. **Repeat**: Continue until targets met

### Common Optimizations

#### 1. Reduce Allocations

```cpp
// ❌ Before: Frequent allocations
std::vector<Handle> getResults() {
    std::vector<Handle> results;
    for (const auto& item : items) {
        results.push_back(process(item));
    }
    return results;
}

// ✅ After: Reserve capacity
std::vector<Handle> getResults() {
    std::vector<Handle> results;
    results.reserve(items.size());  // Pre-allocate
    for (const auto& item : items) {
        results.push_back(process(item));
    }
    return results;
}
```

#### 2. Cache Expensive Operations

```cpp
// ❌ Before: Repeated expensive computation
float getSimilarity(Handle a, Handle b) {
    return computeExpensiveSimilarity(a, b);
}

// ✅ After: Cache results
class SimilarityCache {
    std::map<std::pair<Handle, Handle>, float> cache_;
    
public:
    float getSimilarity(Handle a, Handle b) {
        auto key = std::make_pair(a, b);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }
        float sim = computeExpensiveSimilarity(a, b);
        cache_[key] = sim;
        return sim;
    }
};
```

#### 3. Optimize Data Structures

```cpp
// ❌ Before: Linear search
std::vector<Handle> atoms_;
bool contains(Handle h) {
    return std::find(atoms_.begin(), atoms_.end(), h) != atoms_.end();
}

// ✅ After: Use appropriate structure
std::unordered_set<Handle> atoms_;
bool contains(Handle h) {
    return atoms_.find(h) != atoms_.end();
}
```

#### 4. Reduce AtomSpace Queries

```cpp
// ❌ Before: Query every iteration
for (int i = 0; i < 1000; i++) {
    auto goals = atomspace.get_atoms_by_type(EVALUATION_LINK);
    // ... use goals
}

// ✅ After: Query once
auto goals = atomspace.get_atoms_by_type(EVALUATION_LINK);
for (int i = 0; i < 1000; i++) {
    // ... use cached goals
}
```

### Performance Testing in CI

```yaml
# .github/workflows/benchmark.yml
name: Performance Benchmarks

on:
  push:
    branches: [main, develop]
  pull_request:

jobs:
  benchmark:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Build with benchmarks
      run: |
        mkdir build && cd build
        cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON ..
        make -j$(nproc)
    
    - name: Run benchmarks
      run: |
        cd build
        ./benchmarks/agentzero_benchmarks \
          --benchmark_out=results.json \
          --benchmark_out_format=json
    
    - name: Compare with baseline
      run: |
        python tools/compare_benchmarks.py \
          baseline.json \
          build/results.json
    
    - name: Upload results
      uses: actions/upload-artifact@v2
      with:
        name: benchmark-results
        path: build/results.json
```

## Best Practices

1. ✅ **Baseline First**: Always establish baseline before optimizing
2. ✅ **Profile Before Optimizing**: Don't guess where the bottleneck is
3. ✅ **Measure After Changes**: Verify optimizations actually help
4. ✅ **Use Release Builds**: Always benchmark optimized code
5. ✅ **Minimize Noise**: Run on idle system
6. ✅ **Multiple Runs**: Use --benchmark_repetitions=10
7. ✅ **Realistic Workloads**: Benchmark actual use cases
8. ✅ **Track Over Time**: Monitor performance regressions
9. ✅ **Document Results**: Keep records of benchmarks
10. ✅ **Automate**: Integrate with CI/CD

---

*Part of the AGENT-ZERO-GENESIS documentation - Phase 9: Integration & Testing (AZ-DOC-001)*
