/*
 * tests/framework/TestFramework.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Agent-Zero Unit Test Framework
 * Comprehensive testing infrastructure for Agent-Zero modules
 * Part of the AGENT-ZERO-GENESIS project - AZ-TEST-001
 */

#ifndef AGENTZERO_TEST_FRAMEWORK_H
#define AGENTZERO_TEST_FRAMEWORK_H

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <functional>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/cogserver/server/CogServer.h>
#include <opencog/util/Logger.h>

#include <cxxtest/TestSuite.h>

namespace opencog {
namespace agentzero {
namespace test {

/**
 * Test execution statistics and metrics
 */
struct TestMetrics {
    std::chrono::milliseconds execution_time{0};
    size_t memory_usage_bytes{0};
    size_t atoms_created{0};
    size_t operations_count{0};
    bool performance_within_limits{true};
    std::string error_message;
    
    void reset() {
        execution_time = std::chrono::milliseconds{0};
        memory_usage_bytes = 0;
        atoms_created = 0;
        operations_count = 0;
        performance_within_limits = true;
        error_message.clear();
    }
};

/**
 * Performance benchmarking configuration
 */
struct PerformanceTargets {
    std::chrono::milliseconds max_execution_time{100};
    size_t max_memory_usage{1024 * 1024}; // 1MB default
    size_t max_atoms_threshold{1000};
    
    PerformanceTargets() = default;
    PerformanceTargets(std::chrono::milliseconds time, size_t memory, size_t atoms)
        : max_execution_time(time), max_memory_usage(memory), max_atoms_threshold(atoms) {}
};

/**
 * Base test framework class providing common testing infrastructure
 * Supports unit tests, integration tests, performance benchmarks, and regression tests
 */
class AgentZeroTestFramework : public CxxTest::TestSuite {
protected:
    // Core OpenCog infrastructure
    std::unique_ptr<CogServer> _cogserver;
    std::shared_ptr<AtomSpace> _atomspace;
    
    // Test metrics and performance tracking
    TestMetrics _current_metrics;
    PerformanceTargets _performance_targets;
    std::unordered_map<std::string, TestMetrics> _test_history;
    
    // Test execution state
    std::string _current_test_name;
    bool _verbose_output{false};
    bool _performance_testing_enabled{true};
    
    // Timing utilities
    std::chrono::steady_clock::time_point _test_start_time;
    
public:
    AgentZeroTestFramework();
    virtual ~AgentZeroTestFramework();
    
    // Test lifecycle management
    virtual void setUp() override;
    virtual void tearDown() override;
    
    // Test configuration
    void enableVerboseOutput(bool enabled = true) { _verbose_output = enabled; }
    void enablePerformanceTesting(bool enabled = true) { _performance_testing_enabled = enabled; }
    void setPerformanceTargets(const PerformanceTargets& targets) { _performance_targets = targets; }
    
    // Test execution utilities
    void startTestTimer(const std::string& test_name);
    void stopTestTimer();
    bool validatePerformance() const;
    
    // AtomSpace utilities for testing
    Handle createTestAtom(Type type, const std::string& name);
    void clearAtomSpace();
    size_t getAtomSpaceSize() const;
    
    // Memory monitoring
    size_t getCurrentMemoryUsage() const;
    void trackMemoryUsage();
    
    // Test assertions with performance tracking
    void assertPerformanceWithinLimits();
    void assertAtomSpaceIntegrity();
    void assertMemoryUsageWithinLimits();
    
    // Test reporting
    void logTestMetrics(const std::string& test_name) const;
    void generateTestReport() const;
    TestMetrics getTestMetrics(const std::string& test_name) const;
    
    // Mock object access
    std::shared_ptr<AtomSpace> getTestAtomSpace() const { return _atomspace; }
    CogServer* getTestCogServer() const { return _cogserver.get(); }
    
protected:
    // Internal utilities
    void initializeTestEnvironment();
    void cleanupTestEnvironment();
    void recordTestMetrics(const std::string& test_name);
    size_t calculateMemoryFootprint() const;
};

/**
 * Specialized test framework for unit tests
 * Provides lightweight testing with minimal setup
 */
class UnitTestFramework : public AgentZeroTestFramework {
public:
    UnitTestFramework();
    
    void setUp() override;
    void tearDown() override;
    
    // Simplified setup for unit testing
    void setUpMinimalEnvironment();
};

/**
 * Specialized test framework for integration tests
 * Provides full OpenCog environment setup
 */
class IntegrationTestFramework : public AgentZeroTestFramework {
public:
    IntegrationTestFramework();
    
    void setUp() override;
    void tearDown() override;
    
    // Full environment setup for integration testing
    void setUpFullEnvironment();
    void loadDefaultModules();
    void configureTestScenarios();
};

/**
 * Specialized test framework for performance benchmarks
 * Provides detailed performance monitoring and reporting
 */
class PerformanceTestFramework : public AgentZeroTestFramework {
private:
    std::vector<std::string> _benchmark_names;
    std::unordered_map<std::string, std::vector<TestMetrics>> _benchmark_history;
    
public:
    PerformanceTestFramework();
    
    void setUp() override;
    void tearDown() override;
    
    // Benchmark management
    void startBenchmark(const std::string& benchmark_name);
    void endBenchmark(const std::string& benchmark_name);
    void recordBenchmarkResult(const std::string& benchmark_name, const TestMetrics& metrics);
    
    // Statistical analysis
    TestMetrics calculateBenchmarkStatistics(const std::string& benchmark_name) const;
    void generateBenchmarkReport() const;
    bool hasPerformanceRegression(const std::string& benchmark_name, double threshold = 0.1) const;
    
    // Stress testing utilities
    void runStressTest(const std::string& test_name, 
                      std::function<void()> test_function, 
                      int iterations = 1000);
    void runMemoryStressTest(const std::string& test_name,
                           std::function<void()> test_function,
                           size_t max_memory_mb = 100);
};

/**
 * Test fixture for regression testing
 * Ensures that changes don't break existing functionality
 */
class RegressionTestFramework : public AgentZeroTestFramework {
private:
    std::unordered_map<std::string, std::string> _expected_outputs;
    std::string _baseline_file_path;
    
public:
    RegressionTestFramework();
    
    void setUp() override;
    void tearDown() override;
    
    // Baseline management
    void loadBaseline(const std::string& baseline_file);
    void saveBaseline(const std::string& baseline_file);
    void updateBaseline(const std::string& test_name, const std::string& output);
    
    // Regression validation
    bool validateOutput(const std::string& test_name, const std::string& actual_output);
    void assertNoRegression(const std::string& test_name, const std::string& actual_output);
    
    // Output comparison utilities
    double calculateSimilarity(const std::string& expected, const std::string& actual) const;
    std::string getDifferences(const std::string& expected, const std::string& actual) const;
};

// Utility macros for enhanced assertions
#define TS_ASSERT_PERFORMANCE_OK(framework) \
    do { \
        (framework).assertPerformanceWithinLimits(); \
    } while(0)

#define TS_ASSERT_ATOMSPACE_INTEGRITY(framework) \
    do { \
        (framework).assertAtomSpaceIntegrity(); \
    } while(0)

#define TS_ASSERT_MEMORY_USAGE_OK(framework) \
    do { \
        (framework).assertMemoryUsageWithinLimits(); \
    } while(0)

#define TS_BENCHMARK(framework, name, code) \
    do { \
        (framework).startTestTimer(name); \
        code; \
        (framework).stopTestTimer(); \
        (framework).logTestMetrics(name); \
    } while(0)

} // namespace test
} // namespace agentzero  
} // namespace opencog

#endif // AGENTZERO_TEST_FRAMEWORK_H