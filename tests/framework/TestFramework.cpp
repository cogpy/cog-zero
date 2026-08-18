/*
 * tests/framework/TestFramework.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Agent-Zero Unit Test Framework Implementation
 * Comprehensive testing infrastructure for Agent-Zero modules
 * Part of the AGENT-ZERO-GENESIS project - AZ-TEST-001
 */

#include "TestFramework.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/resource.h>
#include <unistd.h>

using namespace opencog;
using namespace opencog::agentzero::test;

// ===================================================================
// AgentZeroTestFramework Implementation
// ===================================================================

AgentZeroTestFramework::AgentZeroTestFramework() {
    // Set up logging for tests
    logger().set_level(Logger::INFO);
    logger().set_timestamp_flag(false);
    logger().set_print_level_flag(false);
    logger().set_print_to_stdout_flag(true);
}

AgentZeroTestFramework::~AgentZeroTestFramework() {
    cleanupTestEnvironment();
}

void AgentZeroTestFramework::setUp() {
    initializeTestEnvironment();
    _current_metrics.reset();
}

void AgentZeroTestFramework::tearDown() {
    // Record metrics for the current test if any
    if (!_current_test_name.empty()) {
        recordTestMetrics(_current_test_name);
    }
    
    cleanupTestEnvironment();
}

void AgentZeroTestFramework::initializeTestEnvironment() {
    // Create fresh AtomSpace for each test
    _atomspace = std::make_shared<AtomSpace>();
    
    // Create CogServer if needed (minimal setup)
    if (!_cogserver) {
        _cogserver = std::make_unique<CogServer>();
    }
    
    if (_verbose_output) {
        std::cout << "[TEST] Test environment initialized" << std::endl;
    }
}

void AgentZeroTestFramework::cleanupTestEnvironment() {
    // Clear AtomSpace
    if (_atomspace) {
        _atomspace->clear();
    }
    
    // Reset metrics
    _current_metrics.reset();
    _current_test_name.clear();
    
    if (_verbose_output) {
        std::cout << "[TEST] Test environment cleaned up" << std::endl;
    }
}

void AgentZeroTestFramework::startTestTimer(const std::string& test_name) {
    _current_test_name = test_name;
    _test_start_time = std::chrono::steady_clock::now();
    _current_metrics.reset();
    
    if (_performance_testing_enabled) {
        trackMemoryUsage();
        _current_metrics.atoms_created = getAtomSpaceSize();
    }
    
    if (_verbose_output) {
        std::cout << "[TEST] Starting test: " << test_name << std::endl;
    }
}

void AgentZeroTestFramework::stopTestTimer() {
    auto end_time = std::chrono::steady_clock::now();
    _current_metrics.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - _test_start_time);
    
    if (_performance_testing_enabled) {
        trackMemoryUsage();
        _current_metrics.atoms_created = getAtomSpaceSize() - _current_metrics.atoms_created;
        _current_metrics.performance_within_limits = validatePerformance();
    }
    
    if (_verbose_output) {
        std::cout << "[TEST] Finished test: " << _current_test_name 
                  << " (Time: " << _current_metrics.execution_time.count() << "ms)" << std::endl;
    }
}

bool AgentZeroTestFramework::validatePerformance() const {
    if (!_performance_testing_enabled) return true;
    
    bool within_limits = true;
    
    if (_current_metrics.execution_time > _performance_targets.max_execution_time) {
        within_limits = false;
        if (_verbose_output) {
            std::cout << "[PERF] Execution time exceeded: " 
                      << _current_metrics.execution_time.count() << "ms > "
                      << _performance_targets.max_execution_time.count() << "ms" << std::endl;
        }
    }
    
    if (_current_metrics.memory_usage_bytes > _performance_targets.max_memory_usage) {
        within_limits = false;
        if (_verbose_output) {
            std::cout << "[PERF] Memory usage exceeded: " 
                      << _current_metrics.memory_usage_bytes << " bytes > "
                      << _performance_targets.max_memory_usage << " bytes" << std::endl;
        }
    }
    
    if (_current_metrics.atoms_created > _performance_targets.max_atoms_threshold) {
        within_limits = false;
        if (_verbose_output) {
            std::cout << "[PERF] Atom creation exceeded: " 
                      << _current_metrics.atoms_created << " atoms > "
                      << _performance_targets.max_atoms_threshold << " atoms" << std::endl;
        }
    }
    
    return within_limits;
}

Handle AgentZeroTestFramework::createTestAtom(Type type, const std::string& name) {
    if (!_atomspace) {
        throw std::runtime_error("AtomSpace not initialized for testing");
    }
    
    Handle h = _atomspace->add_node(type, name);
    _current_metrics.operations_count++;
    return h;
}

void AgentZeroTestFramework::clearAtomSpace() {
    if (_atomspace) {
        _atomspace->clear();
    }
}

size_t AgentZeroTestFramework::getAtomSpaceSize() const {
    return _atomspace ? _atomspace->get_size() : 0;
}

size_t AgentZeroTestFramework::getCurrentMemoryUsage() const {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    // Convert from kilobytes to bytes
    return usage.ru_maxrss * 1024;
}

void AgentZeroTestFramework::trackMemoryUsage() {
    _current_metrics.memory_usage_bytes = getCurrentMemoryUsage();
}

void AgentZeroTestFramework::assertPerformanceWithinLimits() {
    if (_performance_testing_enabled && !validatePerformance()) {
        TS_FAIL("Performance targets exceeded");
    }
}

void AgentZeroTestFramework::assertAtomSpaceIntegrity() {
    if (!_atomspace) {
        TS_FAIL("AtomSpace is null");
        return;
    }
    
    // Check basic integrity
    size_t atom_count = _atomspace->get_size();
    if (_verbose_output) {
        std::cout << "[TEST] AtomSpace integrity check: " << atom_count << " atoms" << std::endl;
    }
    
    // Additional integrity checks could be added here
    // e.g., checking for orphaned atoms, invalid references, etc.
}

void AgentZeroTestFramework::assertMemoryUsageWithinLimits() {
    trackMemoryUsage();
    if (_current_metrics.memory_usage_bytes > _performance_targets.max_memory_usage) {
        TS_FAIL("Memory usage exceeds limits");
    }
}

void AgentZeroTestFramework::logTestMetrics(const std::string& test_name) const {
    if (_verbose_output) {
        std::cout << "[METRICS] Test: " << test_name 
                  << ", Time: " << _current_metrics.execution_time.count() << "ms"
                  << ", Memory: " << _current_metrics.memory_usage_bytes << " bytes"
                  << ", Atoms: " << _current_metrics.atoms_created
                  << ", Performance: " << (_current_metrics.performance_within_limits ? "PASS" : "FAIL")
                  << std::endl;
    }
}

void AgentZeroTestFramework::generateTestReport() const {
    std::cout << "\n=== Agent-Zero Test Framework Report ===" << std::endl;
    std::cout << "Total tests recorded: " << _test_history.size() << std::endl;
    
    std::chrono::milliseconds total_time{0};
    size_t total_memory = 0;
    size_t performance_failures = 0;
    
    for (const auto& [test_name, metrics] : _test_history) {
        total_time += metrics.execution_time;
        total_memory += metrics.memory_usage_bytes;
        if (!metrics.performance_within_limits) {
            performance_failures++;
        }
        
        std::cout << "  " << test_name 
                  << ": " << metrics.execution_time.count() << "ms"
                  << ", " << metrics.memory_usage_bytes << " bytes"
                  << ", " << metrics.atoms_created << " atoms"
                  << " [" << (metrics.performance_within_limits ? "PASS" : "FAIL") << "]"
                  << std::endl;
    }
    
    std::cout << "Summary:"
              << "\n  Total execution time: " << total_time.count() << "ms"
              << "\n  Average memory usage: " << (total_memory / std::max(size_t(1), _test_history.size())) << " bytes"
              << "\n  Performance failures: " << performance_failures << "/" << _test_history.size()
              << std::endl;
}

TestMetrics AgentZeroTestFramework::getTestMetrics(const std::string& test_name) const {
    auto it = _test_history.find(test_name);
    return (it != _test_history.end()) ? it->second : TestMetrics{};
}

void AgentZeroTestFramework::recordTestMetrics(const std::string& test_name) {
    _test_history[test_name] = _current_metrics;
}

size_t AgentZeroTestFramework::calculateMemoryFootprint() const {
    return getCurrentMemoryUsage();
}

// ===================================================================
// UnitTestFramework Implementation
// ===================================================================

UnitTestFramework::UnitTestFramework() : AgentZeroTestFramework() {
    // Configure for lightweight unit testing
    _performance_targets.max_execution_time = std::chrono::milliseconds{50};
    _performance_targets.max_memory_usage = 512 * 1024; // 512KB
    _performance_targets.max_atoms_threshold = 100;
}

void UnitTestFramework::setUp() {
    AgentZeroTestFramework::setUp();
    setUpMinimalEnvironment();
}

void UnitTestFramework::tearDown() {
    AgentZeroTestFramework::tearDown();
}

void UnitTestFramework::setUpMinimalEnvironment() {
    // Minimal setup for unit tests - just AtomSpace
    if (_verbose_output) {
        std::cout << "[UNIT] Minimal test environment set up" << std::endl;
    }
}

// ===================================================================
// IntegrationTestFramework Implementation  
// ===================================================================

IntegrationTestFramework::IntegrationTestFramework() : AgentZeroTestFramework() {
    // Configure for comprehensive integration testing
    _performance_targets.max_execution_time = std::chrono::milliseconds{1000};
    _performance_targets.max_memory_usage = 10 * 1024 * 1024; // 10MB
    _performance_targets.max_atoms_threshold = 10000;
}

void IntegrationTestFramework::setUp() {
    AgentZeroTestFramework::setUp();
    setUpFullEnvironment();
}

void IntegrationTestFramework::tearDown() {
    AgentZeroTestFramework::tearDown();
}

void IntegrationTestFramework::setUpFullEnvironment() {
    loadDefaultModules();
    configureTestScenarios();
    
    if (_verbose_output) {
        std::cout << "[INTEGRATION] Full test environment set up" << std::endl;
    }
}

void IntegrationTestFramework::loadDefaultModules() {
    // Load CogServer modules for integration testing
    if (_cogserver) {
        // This would normally load modules, but we'll keep it simple for now
        if (_verbose_output) {
            std::cout << "[INTEGRATION] Default modules loaded" << std::endl;
        }
    }
}

void IntegrationTestFramework::configureTestScenarios() {
    // Set up test scenarios for integration testing
    if (_verbose_output) {
        std::cout << "[INTEGRATION] Test scenarios configured" << std::endl;
    }
}

// ===================================================================
// PerformanceTestFramework Implementation
// ===================================================================

PerformanceTestFramework::PerformanceTestFramework() : AgentZeroTestFramework() {
    // Configure for performance testing
    _performance_targets.max_execution_time = std::chrono::milliseconds{5000};
    _performance_targets.max_memory_usage = 100 * 1024 * 1024; // 100MB
    _performance_targets.max_atoms_threshold = 100000;
}

void PerformanceTestFramework::setUp() {
    AgentZeroTestFramework::setUp();
}

void PerformanceTestFramework::tearDown() {
    AgentZeroTestFramework::tearDown();
}

void PerformanceTestFramework::startBenchmark(const std::string& benchmark_name) {
    startTestTimer(benchmark_name);
    _benchmark_names.push_back(benchmark_name);
}

void PerformanceTestFramework::endBenchmark(const std::string& benchmark_name) {
    stopTestTimer();
    recordBenchmarkResult(benchmark_name, _current_metrics);
}

void PerformanceTestFramework::recordBenchmarkResult(const std::string& benchmark_name, 
                                                    const TestMetrics& metrics) {
    _benchmark_history[benchmark_name].push_back(metrics);
}

TestMetrics PerformanceTestFramework::calculateBenchmarkStatistics(const std::string& benchmark_name) const {
    auto it = _benchmark_history.find(benchmark_name);
    if (it == _benchmark_history.end() || it->second.empty()) {
        return TestMetrics{};
    }
    
    const auto& results = it->second;
    TestMetrics avg_metrics;
    
    std::chrono::milliseconds total_time{0};
    size_t total_memory = 0;
    size_t total_atoms = 0;
    
    for (const auto& result : results) {
        total_time += result.execution_time;
        total_memory += result.memory_usage_bytes;
        total_atoms += result.atoms_created;
    }
    
    avg_metrics.execution_time = total_time / results.size();
    avg_metrics.memory_usage_bytes = total_memory / results.size();
    avg_metrics.atoms_created = total_atoms / results.size();
    
    return avg_metrics;
}

void PerformanceTestFramework::generateBenchmarkReport() const {
    std::cout << "\n=== Performance Benchmark Report ===" << std::endl;
    
    for (const auto& [benchmark_name, results] : _benchmark_history) {
        if (results.empty()) continue;
        
        TestMetrics stats = calculateBenchmarkStatistics(benchmark_name);
        
        std::cout << "Benchmark: " << benchmark_name << std::endl;
        std::cout << "  Runs: " << results.size() << std::endl;
        std::cout << "  Average time: " << stats.execution_time.count() << "ms" << std::endl;
        std::cout << "  Average memory: " << stats.memory_usage_bytes << " bytes" << std::endl;
        std::cout << "  Average atoms: " << stats.atoms_created << std::endl;
        
        // Calculate min/max
        auto time_minmax = std::minmax_element(results.begin(), results.end(),
            [](const TestMetrics& a, const TestMetrics& b) {
                return a.execution_time < b.execution_time;
            });
        
        std::cout << "  Time range: " << time_minmax.first->execution_time.count() 
                  << "ms - " << time_minmax.second->execution_time.count() << "ms" << std::endl;
        std::cout << std::endl;
    }
}

bool PerformanceTestFramework::hasPerformanceRegression(const std::string& benchmark_name, 
                                                       double threshold) const {
    auto it = _benchmark_history.find(benchmark_name);
    if (it == _benchmark_history.end() || it->second.size() < 2) {
        return false; // Not enough data to determine regression
    }
    
    const auto& results = it->second;
    auto latest = results.back().execution_time;
    auto previous = results[results.size() - 2].execution_time;
    
    double change = static_cast<double>(latest.count()) / previous.count() - 1.0;
    return change > threshold;
}

void PerformanceTestFramework::runStressTest(const std::string& test_name,
                                           std::function<void()> test_function,
                                           int iterations) {
    std::cout << "[STRESS] Running stress test: " << test_name 
              << " (" << iterations << " iterations)" << std::endl;
    
    startBenchmark(test_name + "_stress");
    
    for (int i = 0; i < iterations; ++i) {
        test_function();
        
        if (i % 100 == 0 && _verbose_output) {
            std::cout << "[STRESS] Completed " << i << "/" << iterations << " iterations" << std::endl;
        }
    }
    
    endBenchmark(test_name + "_stress");
    
    std::cout << "[STRESS] Stress test completed: " << test_name << std::endl;
}

void PerformanceTestFramework::runMemoryStressTest(const std::string& test_name,
                                                  std::function<void()> test_function,
                                                  size_t max_memory_mb) {
    std::cout << "[MEMORY] Running memory stress test: " << test_name 
              << " (max " << max_memory_mb << "MB)" << std::endl;
    
    size_t max_memory_bytes = max_memory_mb * 1024 * 1024;
    startBenchmark(test_name + "_memory_stress");
    
    int iterations = 0;
    while (getCurrentMemoryUsage() < max_memory_bytes) {
        test_function();
        iterations++;
        
        if (iterations % 100 == 0) {
            trackMemoryUsage();
            if (_verbose_output) {
                std::cout << "[MEMORY] Memory usage: " 
                          << (_current_metrics.memory_usage_bytes / 1024 / 1024) << "MB" << std::endl;
            }
        }
    }
    
    endBenchmark(test_name + "_memory_stress");
    
    std::cout << "[MEMORY] Memory stress test completed: " << test_name 
              << " (" << iterations << " iterations)" << std::endl;
}

// ===================================================================
// RegressionTestFramework Implementation
// ===================================================================

RegressionTestFramework::RegressionTestFramework() : AgentZeroTestFramework() {
    // Configure for regression testing
    _performance_targets.max_execution_time = std::chrono::milliseconds{500};
    _performance_targets.max_memory_usage = 5 * 1024 * 1024; // 5MB
    _performance_targets.max_atoms_threshold = 5000;
}

void RegressionTestFramework::setUp() {
    AgentZeroTestFramework::setUp();
}

void RegressionTestFramework::tearDown() {
    AgentZeroTestFramework::tearDown();
}

void RegressionTestFramework::loadBaseline(const std::string& baseline_file) {
    _baseline_file_path = baseline_file;
    std::ifstream file(baseline_file);
    if (!file.is_open()) {
        if (_verbose_output) {
            std::cout << "[REGRESSION] Baseline file not found: " << baseline_file << std::endl;
        }
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            _expected_outputs[key] = value;
        }
    }
    
    if (_verbose_output) {
        std::cout << "[REGRESSION] Loaded " << _expected_outputs.size() 
                  << " baseline entries from " << baseline_file << std::endl;
    }
}

void RegressionTestFramework::saveBaseline(const std::string& baseline_file) {
    std::ofstream file(baseline_file);
    if (!file.is_open()) {
        std::cerr << "[REGRESSION] Could not save baseline to: " << baseline_file << std::endl;
        return;
    }
    
    for (const auto& [key, value] : _expected_outputs) {
        file << key << "=" << value << std::endl;
    }
    
    if (_verbose_output) {
        std::cout << "[REGRESSION] Saved " << _expected_outputs.size() 
                  << " baseline entries to " << baseline_file << std::endl;
    }
}

void RegressionTestFramework::updateBaseline(const std::string& test_name, 
                                            const std::string& output) {
    _expected_outputs[test_name] = output;
}

bool RegressionTestFramework::validateOutput(const std::string& test_name, 
                                            const std::string& actual_output) {
    auto it = _expected_outputs.find(test_name);
    if (it == _expected_outputs.end()) {
        if (_verbose_output) {
            std::cout << "[REGRESSION] No baseline found for test: " << test_name << std::endl;
        }
        return false;
    }
    
    return it->second == actual_output;
}

void RegressionTestFramework::assertNoRegression(const std::string& test_name, 
                                                const std::string& actual_output) {
    if (!validateOutput(test_name, actual_output)) {
        auto it = _expected_outputs.find(test_name);
        if (it != _expected_outputs.end()) {
            std::string diff = getDifferences(it->second, actual_output);
            TS_FAIL(("Regression detected in " + test_name + ": " + diff).c_str());
        } else {
            TS_FAIL(("No baseline found for test: " + test_name).c_str());
        }
    }
}

double RegressionTestFramework::calculateSimilarity(const std::string& expected, 
                                                   const std::string& actual) const {
    if (expected.empty() && actual.empty()) return 1.0;
    if (expected.empty() || actual.empty()) return 0.0;
    
    // Simple similarity calculation - could be enhanced with more sophisticated algorithms
    size_t common_length = 0;
    size_t min_length = std::min(expected.length(), actual.length());
    
    for (size_t i = 0; i < min_length; ++i) {
        if (expected[i] == actual[i]) {
            common_length++;
        }
    }
    
    return static_cast<double>(common_length) / std::max(expected.length(), actual.length());
}

std::string RegressionTestFramework::getDifferences(const std::string& expected, 
                                                   const std::string& actual) const {
    std::ostringstream diff;
    diff << "Expected: '" << expected << "', Actual: '" << actual << "'";
    
    double similarity = calculateSimilarity(expected, actual);
    diff << " (Similarity: " << (similarity * 100.0) << "%)";
    
    return diff.str();
}