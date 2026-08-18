/*
 * AgentZeroBenchmark.cpp
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

#include "AgentZeroBenchmark.h"
#include <sys/resource.h>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

using namespace opencog;

AgentZeroBenchmark::AgentZeroBenchmark(AtomSpacePtr asp,
                                       const std::string& output_dir,
                                       bool verbose)
    : _atomspace(asp), _output_directory(output_dir), _verbose(verbose)
{
    if (!_atomspace) {
        _atomspace = createAtomSpace();
    }
    _initial_memory = get_current_memory_usage();
}

AgentZeroBenchmark::~AgentZeroBenchmark()
{
}

size_t AgentZeroBenchmark::get_current_memory_usage()
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss * 1024; // Convert KB to bytes on Linux
}

size_t AgentZeroBenchmark::get_atomspace_size()
{
    if (!_atomspace) return 0;
    return _atomspace->get_size();
}

std::chrono::nanoseconds AgentZeroBenchmark::calculate_mean(
    const std::vector<std::chrono::nanoseconds>& times)
{
    if (times.empty()) return std::chrono::nanoseconds(0);
    
    long long total = 0;
    for (const auto& t : times) {
        total += t.count();
    }
    return std::chrono::nanoseconds(total / times.size());
}

std::chrono::nanoseconds AgentZeroBenchmark::calculate_std_dev(
    const std::vector<std::chrono::nanoseconds>& times,
    std::chrono::nanoseconds mean)
{
    if (times.size() < 2) return std::chrono::nanoseconds(0);
    
    long long sum_squared_diff = 0;
    for (const auto& t : times) {
        long long diff = t.count() - mean.count();
        sum_squared_diff += diff * diff;
    }
    
    double variance = static_cast<double>(sum_squared_diff) / (times.size() - 1);
    return std::chrono::nanoseconds(static_cast<long long>(std::sqrt(variance)));
}

BenchmarkResult AgentZeroBenchmark::run_benchmark(
    const std::string& name,
    size_t iterations,
    std::function<void()> benchmark_func)
{
    if (_verbose) {
        std::cout << "Running benchmark: " << name 
                  << " (" << iterations << " iterations)" << std::endl;
    }
    
    BenchmarkResult result;
    result.benchmark_name = name;
    result.iterations = iterations;
    result.atomspace_size_before = get_atomspace_size();
    
    std::vector<std::chrono::nanoseconds> timings;
    timings.reserve(iterations);
    
    // Warm-up run
    benchmark_func();
    
    // Actual benchmark runs
    auto total_start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        benchmark_func();
        auto end = std::chrono::high_resolution_clock::now();
        
        timings.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
    }
    
    auto total_end = std::chrono::high_resolution_clock::now();
    result.total_time = std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start);
    
    // Calculate statistics
    result.min_time = *std::min_element(timings.begin(), timings.end());
    result.max_time = *std::max_element(timings.begin(), timings.end());
    result.mean_time = calculate_mean(timings);
    result.std_dev = calculate_std_dev(timings, result.mean_time);
    
    result.atomspace_size_after = get_atomspace_size();
    result.memory_used_bytes = get_current_memory_usage() - _initial_memory;
    
    _results.push_back(result);
    
    if (_verbose) {
        std::cout << "  Operations per second: " << std::fixed << std::setprecision(2)
                  << result.ops_per_second() << std::endl;
        std::cout << "  Mean time: " << result.mean_time_ms() << " ms" << std::endl;
    }
    
    return result;
}

std::vector<BenchmarkResult> AgentZeroBenchmark::run_scaling_benchmark(
    const std::string& name,
    const std::vector<size_t>& sizes,
    std::function<void(size_t)> setup_func,
    std::function<void()> benchmark_func)
{
    std::vector<BenchmarkResult> scaling_results;
    
    for (size_t size : sizes) {
        if (_verbose) {
            std::cout << "\n=== Scaling benchmark with size: " << size << " ===" << std::endl;
        }
        
        // Setup with specific size
        setup_func(size);
        
        // Run benchmark
        std::string size_name = name + "_size_" + std::to_string(size);
        BenchmarkResult result = run_benchmark(size_name, 100, benchmark_func);
        scaling_results.push_back(result);
    }
    
    return scaling_results;
}

void AgentZeroBenchmark::export_results_to_csv(const std::string& filename)
{
    // Create output directory if it doesn't exist - safe method without system()
    struct stat st;
    if (stat(_output_directory.c_str(), &st) != 0) {
        // Directory doesn't exist, create it
        mode_t mode = 0755;
        if (mkdir(_output_directory.c_str(), mode) != 0 && errno != EEXIST) {
            logger().error("Failed to create output directory: " + _output_directory);
            return;
        }
    }
    
    std::string full_path = _output_directory + "/" + filename + ".csv";
    std::ofstream file(full_path);
    
    if (!file.is_open()) {
        logger().error("Failed to open file for writing: " + full_path);
        return;
    }
    
    // Write header
    file << "Benchmark,Iterations,TotalTime(ns),MinTime(ns),MaxTime(ns),"
         << "MeanTime(ns),StdDev(ns),OpsPerSec,MemoryUsed(bytes),"
         << "AtomSpaceSizeBefore,AtomSpaceSizeAfter\n";
    
    // Write data
    for (const auto& result : _results) {
        file << result.benchmark_name << ","
             << result.iterations << ","
             << result.total_time.count() << ","
             << result.min_time.count() << ","
             << result.max_time.count() << ","
             << result.mean_time.count() << ","
             << result.std_dev.count() << ","
             << std::fixed << std::setprecision(2) << result.ops_per_second() << ","
             << result.memory_used_bytes << ","
             << result.atomspace_size_before << ","
             << result.atomspace_size_after << "\n";
    }
    
    file.close();
    
    if (_verbose) {
        std::cout << "\nResults exported to: " << full_path << std::endl;
    }
}

void AgentZeroBenchmark::print_summary()
{
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "AGENT-ZERO BENCHMARK SUMMARY" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    for (const auto& result : _results) {
        std::cout << "\nBenchmark: " << result.benchmark_name << std::endl;
        std::cout << "  Iterations: " << result.iterations << std::endl;
        std::cout << "  Total time: " << std::fixed << std::setprecision(3)
                  << (result.total_time.count() / 1e6) << " ms" << std::endl;
        std::cout << "  Mean time: " << result.mean_time_ms() << " ms" << std::endl;
        std::cout << "  Min time: " << (result.min_time.count() / 1e6) << " ms" << std::endl;
        std::cout << "  Max time: " << (result.max_time.count() / 1e6) << " ms" << std::endl;
        std::cout << "  Std deviation: " << (result.std_dev.count() / 1e6) << " ms" << std::endl;
        std::cout << "  Operations/sec: " << std::fixed << std::setprecision(2)
                  << result.ops_per_second() << std::endl;
        std::cout << "  Memory used: " << (result.memory_used_bytes / 1024) << " KB" << std::endl;
        std::cout << "  AtomSpace growth: " << result.atomspace_size_before 
                  << " -> " << result.atomspace_size_after 
                  << " (+" << (result.atomspace_size_after - result.atomspace_size_before) << ")" << std::endl;
    }
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
}

void AgentZeroBenchmark::clear_results()
{
    _results.clear();
}
