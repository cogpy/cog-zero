/*
 * AgentZeroBenchmark.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _OPENCOG_AGENTZERO_BENCHMARK_H
#define _OPENCOG_AGENTZERO_BENCHMARK_H

#include <string>
#include <vector>
#include <chrono>
#include <fstream>
#include <numeric>
#include <cmath>
#include <memory>
#include <functional>

#include <opencog/util/Logger.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

namespace opencog
{

/**
 * Benchmark result structure for storing performance metrics
 */
struct BenchmarkResult
{
    std::string benchmark_name;
    size_t iterations;
    std::chrono::nanoseconds total_time;
    std::chrono::nanoseconds min_time;
    std::chrono::nanoseconds max_time;
    std::chrono::nanoseconds mean_time;
    std::chrono::nanoseconds std_dev;
    size_t memory_used_bytes;
    size_t atomspace_size_before;
    size_t atomspace_size_after;
    
    // Calculate operations per second
    double ops_per_second() const {
        if (total_time.count() == 0) return 0.0;
        return static_cast<double>(iterations) / 
               (static_cast<double>(total_time.count()) / 1e9);
    }
    
    // Calculate mean time in milliseconds
    double mean_time_ms() const {
        return static_cast<double>(mean_time.count()) / 1e6;
    }
};

/**
 * AgentZeroBenchmark - Core benchmarking framework for Agent-Zero components
 * 
 * Provides comprehensive performance testing capabilities including:
 * - High-resolution timing measurements
 * - Statistical analysis of benchmark runs
 * - Memory usage tracking
 * - AtomSpace size monitoring
 * - CSV export for data analysis
 */
class AgentZeroBenchmark
{
private:
    AtomSpacePtr _atomspace;
    std::string _output_directory;
    bool _verbose;
    std::vector<BenchmarkResult> _results;
    
    // Timing utilities
    std::chrono::high_resolution_clock::time_point _start_time;
    std::vector<std::chrono::nanoseconds> _timings;
    
    // Memory tracking
    size_t _initial_memory;
    size_t get_current_memory_usage();
    size_t get_atomspace_size();
    
    // Statistics calculation
    std::chrono::nanoseconds calculate_mean(const std::vector<std::chrono::nanoseconds>& times);
    std::chrono::nanoseconds calculate_std_dev(const std::vector<std::chrono::nanoseconds>& times,
                                                std::chrono::nanoseconds mean);
    
public:
    AgentZeroBenchmark(AtomSpacePtr asp = nullptr, 
                       const std::string& output_dir = "./benchmark_results",
                       bool verbose = false);
    ~AgentZeroBenchmark();
    
    /**
     * Run a benchmark function multiple times and collect statistics
     * 
     * @param name Name of the benchmark
     * @param iterations Number of times to run the function
     * @param benchmark_func The function to benchmark
     * @return BenchmarkResult containing performance metrics
     */
    BenchmarkResult run_benchmark(const std::string& name,
                                  size_t iterations,
                                  std::function<void()> benchmark_func);
    
    /**
     * Run a parameterized benchmark with varying input sizes
     * 
     * @param name Base name of the benchmark
     * @param sizes Vector of input sizes to test
     * @param setup_func Function to set up data for each size
     * @param benchmark_func Function to benchmark
     * @return Vector of BenchmarkResults for each size
     */
    std::vector<BenchmarkResult> run_scaling_benchmark(
        const std::string& name,
        const std::vector<size_t>& sizes,
        std::function<void(size_t)> setup_func,
        std::function<void()> benchmark_func);
    
    /**
     * Export all benchmark results to CSV file
     * 
     * @param filename Output filename (without extension)
     */
    void export_results_to_csv(const std::string& filename);
    
    /**
     * Print summary of all benchmark results
     */
    void print_summary();
    
    /**
     * Clear all stored results
     */
    void clear_results();
    
    /**
     * Get all benchmark results
     */
    const std::vector<BenchmarkResult>& get_results() const { return _results; }
    
    /**
     * Set verbosity level
     */
    void set_verbose(bool verbose) { _verbose = verbose; }
    
    /**
     * Get/Set AtomSpace for benchmarking
     */
    AtomSpacePtr get_atomspace() const { return _atomspace; }
    void set_atomspace(AtomSpacePtr asp) { _atomspace = asp; }
};

} // namespace opencog

#endif // _OPENCOG_AGENTZERO_BENCHMARK_H
