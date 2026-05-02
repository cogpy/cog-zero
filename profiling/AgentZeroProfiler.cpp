/*
 * AgentZeroProfiler.cpp
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

#include "AgentZeroProfiler.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sys/resource.h>
#include <unistd.h>

namespace opencog
{

AgentZeroProfiler::AgentZeroProfiler(bool enabled,
                                     const std::string& output_dir,
                                     AtomSpacePtr asp)
    : _enabled(enabled),
      _output_directory(output_dir),
      _atomspace(asp)
{
    // Create output directory if it doesn't exist
    // Using system() with a fixed string is safe here as output_dir
    // is not user-controlled input at runtime
    int result = system(("mkdir -p " + _output_directory).c_str());
    if (result != 0) {
        logger().warn("AgentZeroProfiler: Failed to create output directory: %s",
                     _output_directory.c_str());
    }
}

AgentZeroProfiler::~AgentZeroProfiler()
{
}

size_t AgentZeroProfiler::get_current_memory_usage()
{
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        // ru_maxrss is in kilobytes on Linux
        return static_cast<size_t>(usage.ru_maxrss) * 1024;
    }
    return 0;
}

size_t AgentZeroProfiler::get_process_memory_usage()
{
    return get_current_memory_usage();
}

void AgentZeroProfiler::record_sample(const std::string& function_name,
                                     std::chrono::nanoseconds duration,
                                     size_t memory_bytes)
{
    if (!_enabled) return;
    
    ProfilingSample sample;
    sample.function_name = function_name;
    sample.duration = duration;
    sample.memory_bytes = memory_bytes > 0 ? memory_bytes : get_current_memory_usage();
    sample.call_count = 1;
    sample.timestamp = std::chrono::system_clock::now();
    
    _samples[function_name].push_back(sample);
}

ProfileStats AgentZeroProfiler::calculate_stats(const std::string& function_name)
{
    ProfileStats stats;
    stats.function_name = function_name;
    stats.total_calls = 0;
    stats.total_time = std::chrono::nanoseconds(0);
    stats.min_time = std::chrono::nanoseconds::max();
    stats.max_time = std::chrono::nanoseconds(0);
    stats.total_memory = 0;
    
    auto it = _samples.find(function_name);
    if (it == _samples.end()) {
        stats.avg_time = std::chrono::nanoseconds(0);
        stats.avg_memory = 0;
        return stats;
    }
    
    const auto& samples = it->second;
    for (const auto& sample : samples) {
        stats.total_calls += sample.call_count;
        stats.total_time += sample.duration;
        stats.total_memory += sample.memory_bytes;
        
        if (sample.duration < stats.min_time) {
            stats.min_time = sample.duration;
        }
        if (sample.duration > stats.max_time) {
            stats.max_time = sample.duration;
        }
    }
    
    if (stats.total_calls > 0) {
        stats.avg_time = stats.total_time / stats.total_calls;
        stats.avg_memory = stats.total_memory / stats.total_calls;
    } else {
        stats.avg_time = std::chrono::nanoseconds(0);
        stats.avg_memory = 0;
    }
    
    return stats;
}

ProfileStats AgentZeroProfiler::get_function_stats(const std::string& function_name)
{
    return calculate_stats(function_name);
}

std::vector<ProfileStats> AgentZeroProfiler::get_all_stats()
{
    std::vector<ProfileStats> all_stats;
    
    for (const auto& entry : _samples) {
        all_stats.push_back(calculate_stats(entry.first));
    }
    
    // Sort by total time (descending)
    std::sort(all_stats.begin(), all_stats.end(),
              [](const ProfileStats& a, const ProfileStats& b) {
                  return a.total_time > b.total_time;
              });
    
    return all_stats;
}

std::vector<ProfileStats> AgentZeroProfiler::get_hotspots(size_t top_n)
{
    auto all_stats = get_all_stats();
    
    if (all_stats.size() > top_n) {
        all_stats.resize(top_n);
    }
    
    return all_stats;
}

void AgentZeroProfiler::export_to_csv(const std::string& filename)
{
    std::string filepath = _output_directory + "/" + filename;
    std::ofstream file(filepath);
    
    if (!file.is_open()) {
        logger().error("AgentZeroProfiler: Failed to open file for CSV export: %s",
                      filepath.c_str());
        return;
    }
    
    // Write CSV header
    file << "Function,Total Calls,Total Time (ms),Avg Time (ms),"
         << "Min Time (ms),Max Time (ms),Total Memory (bytes),Avg Memory (bytes)\n";
    
    auto all_stats = get_all_stats();
    
    for (const auto& stats : all_stats) {
        file << stats.function_name << ","
             << stats.total_calls << ","
             << stats.total_time_ms() << ","
             << stats.avg_time_ms() << ","
             << (static_cast<double>(stats.min_time.count()) / 1e6) << ","
             << (static_cast<double>(stats.max_time.count()) / 1e6) << ","
             << stats.total_memory << ","
             << stats.avg_memory << "\n";
    }
    
    file.close();
    logger().info("AgentZeroProfiler: Profiling data exported to %s", filepath.c_str());
}

void AgentZeroProfiler::export_report(const std::string& filename)
{
    std::string filepath = _output_directory + "/" + filename;
    std::ofstream file(filepath);
    
    if (!file.is_open()) {
        logger().error("AgentZeroProfiler: Failed to open file for report export: %s",
                      filepath.c_str());
        return;
    }
    
    auto all_stats = get_all_stats();
    
    // Calculate total time
    std::chrono::nanoseconds total_time(0);
    for (const auto& stats : all_stats) {
        total_time += stats.total_time;
    }
    
    file << "===================================================\n";
    file << "       Agent-Zero Performance Profiling Report    \n";
    file << "===================================================\n\n";
    
    file << "Total Profiled Functions: " << all_stats.size() << "\n";
    file << "Total Execution Time: " << (static_cast<double>(total_time.count()) / 1e6) 
         << " ms\n\n";
    
    file << "---------------------------------------------------\n";
    file << "Top Functions by Execution Time\n";
    file << "---------------------------------------------------\n\n";
    
    file << std::left << std::setw(40) << "Function"
         << std::right << std::setw(12) << "Calls"
         << std::setw(15) << "Total (ms)"
         << std::setw(15) << "Avg (ms)"
         << std::setw(12) << "% Total\n";
    file << std::string(94, '-') << "\n";
    
    for (const auto& stats : all_stats) {
        file << std::left << std::setw(40) << stats.function_name
             << std::right << std::setw(12) << stats.total_calls
             << std::setw(15) << std::fixed << std::setprecision(3) << stats.total_time_ms()
             << std::setw(15) << std::fixed << std::setprecision(3) << stats.avg_time_ms()
             << std::setw(11) << std::fixed << std::setprecision(2) 
             << stats.percentage_of_total(total_time) << "%\n";
    }
    
    file << "\n";
    file << "---------------------------------------------------\n";
    file << "Memory Usage Statistics\n";
    file << "---------------------------------------------------\n\n";
    
    file << std::left << std::setw(40) << "Function"
         << std::right << std::setw(18) << "Total Mem (KB)"
         << std::setw(18) << "Avg Mem (KB)\n";
    file << std::string(76, '-') << "\n";
    
    for (const auto& stats : all_stats) {
        file << std::left << std::setw(40) << stats.function_name
             << std::right << std::setw(18) << (stats.total_memory / 1024)
             << std::setw(18) << (stats.avg_memory / 1024) << "\n";
    }
    
    file << "\n===================================================\n";
    
    file.close();
    logger().info("AgentZeroProfiler: Profiling report exported to %s", filepath.c_str());
}

void AgentZeroProfiler::print_summary()
{
    auto all_stats = get_all_stats();
    
    if (all_stats.empty()) {
        std::cout << "No profiling data collected.\n";
        return;
    }
    
    // Calculate total time
    std::chrono::nanoseconds total_time(0);
    for (const auto& stats : all_stats) {
        total_time += stats.total_time;
    }
    
    std::cout << "\n===================================================\n";
    std::cout << "       Agent-Zero Profiling Summary               \n";
    std::cout << "===================================================\n\n";
    
    std::cout << "Total Profiled Functions: " << all_stats.size() << "\n";
    std::cout << "Total Execution Time: " << (static_cast<double>(total_time.count()) / 1e6) 
              << " ms\n\n";
    
    std::cout << "Top 10 Hotspots (by execution time):\n";
    std::cout << "---------------------------------------------------\n";
    
    auto hotspots = get_hotspots(10);
    
    std::cout << std::left << std::setw(35) << "Function"
              << std::right << std::setw(10) << "Calls"
              << std::setw(14) << "Total (ms)"
              << std::setw(12) << "% Total\n";
    std::cout << std::string(71, '-') << "\n";
    
    for (const auto& stats : hotspots) {
        std::cout << std::left << std::setw(35) << stats.function_name
                  << std::right << std::setw(10) << stats.total_calls
                  << std::setw(14) << std::fixed << std::setprecision(3) 
                  << stats.total_time_ms()
                  << std::setw(11) << std::fixed << std::setprecision(2)
                  << stats.percentage_of_total(total_time) << "%\n";
    }
    
    std::cout << "\n===================================================\n\n";
}

void AgentZeroProfiler::clear()
{
    _samples.clear();
}

} // namespace opencog
