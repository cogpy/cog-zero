/*
 * AgentZeroProfiler.h
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

#ifndef _OPENCOG_AGENTZERO_PROFILER_H
#define _OPENCOG_AGENTZERO_PROFILER_H

#include <string>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <memory>
#include <fstream>

#include <opencog/util/Logger.h>
#include <opencog/atomspace/AtomSpace.h>

namespace opencog
{

/**
 * Profiling sample structure for storing performance data
 */
struct ProfilingSample
{
    std::string function_name;
    std::chrono::nanoseconds duration;
    size_t memory_bytes;
    size_t call_count;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * Profiling statistics for a function
 */
struct ProfileStats
{
    std::string function_name;
    size_t total_calls;
    std::chrono::nanoseconds total_time;
    std::chrono::nanoseconds min_time;
    std::chrono::nanoseconds max_time;
    std::chrono::nanoseconds avg_time;
    size_t total_memory;
    size_t avg_memory;
    
    double total_time_ms() const {
        return static_cast<double>(total_time.count()) / 1e6;
    }
    
    double avg_time_ms() const {
        return static_cast<double>(avg_time.count()) / 1e6;
    }
    
    double percentage_of_total(std::chrono::nanoseconds total) const {
        if (total.count() == 0) return 0.0;
        return (static_cast<double>(total_time.count()) / 
                static_cast<double>(total.count())) * 100.0;
    }
};

/**
 * AgentZeroProfiler - Comprehensive profiling framework for Agent-Zero
 * 
 * Provides:
 * - Function-level timing profiling
 * - Memory usage tracking
 * - Call count statistics
 * - Hotspot identification
 * - Integration with external profilers (gprof, perf, valgrind)
 * - CSV export for analysis
 */
class AgentZeroProfiler
{
private:
    bool _enabled;
    std::string _output_directory;
    std::unordered_map<std::string, std::vector<ProfilingSample>> _samples;
    AtomSpacePtr _atomspace;
    
    // Memory tracking
    size_t get_current_memory_usage();
    size_t get_process_memory_usage();
    
    // Statistics calculation
    ProfileStats calculate_stats(const std::string& function_name);
    
public:
    AgentZeroProfiler(bool enabled = true,
                      const std::string& output_dir = "./profiling_results",
                      AtomSpacePtr asp = nullptr);
    ~AgentZeroProfiler();
    
    /**
     * Enable/disable profiling
     */
    void enable() { _enabled = true; }
    void disable() { _enabled = false; }
    bool is_enabled() const { return _enabled; }
    
    /**
     * Record a profiling sample
     */
    void record_sample(const std::string& function_name,
                      std::chrono::nanoseconds duration,
                      size_t memory_bytes = 0);
    
    /**
     * Get statistics for a specific function
     */
    ProfileStats get_function_stats(const std::string& function_name);
    
    /**
     * Get statistics for all profiled functions
     */
    std::vector<ProfileStats> get_all_stats();
    
    /**
     * Get hotspots (functions consuming most time)
     */
    std::vector<ProfileStats> get_hotspots(size_t top_n = 10);
    
    /**
     * Export profiling data to CSV
     */
    void export_to_csv(const std::string& filename);
    
    /**
     * Export profiling report
     */
    void export_report(const std::string& filename);
    
    /**
     * Print profiling summary to console
     */
    void print_summary();
    
    /**
     * Clear all profiling data
     */
    void clear();
    
    /**
     * Get/Set output directory
     */
    std::string get_output_directory() const { return _output_directory; }
    void set_output_directory(const std::string& dir) { _output_directory = dir; }
    
    /**
     * Get/Set AtomSpace
     */
    AtomSpacePtr get_atomspace() const { return _atomspace; }
    void set_atomspace(AtomSpacePtr asp) { _atomspace = asp; }
};

/**
 * RAII-style profiling scope helper
 * Automatically profiles function execution time
 */
class ProfileScope
{
private:
    std::string _function_name;
    std::chrono::high_resolution_clock::time_point _start_time;
    size_t _start_memory;
    AgentZeroProfiler* _profiler;
    bool _active;
    
public:
    ProfileScope(AgentZeroProfiler* profiler, 
                 const std::string& function_name)
        : _function_name(function_name),
          _profiler(profiler),
          _active(profiler && profiler->is_enabled())
    {
        if (_active) {
            _start_time = std::chrono::high_resolution_clock::now();
            _start_memory = 0; // Memory tracking is done at sample recording time
        }
    }
    
    ~ProfileScope()
    {
        if (_active && _profiler) {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_time - _start_time);
            _profiler->record_sample(_function_name, duration, 0);
        }
    }
};

// Convenience macro for profiling a scope
#define PROFILE_SCOPE(profiler, name) \
    opencog::ProfileScope _profile_scope_##__LINE__(profiler, name)

#define PROFILE_FUNCTION(profiler) \
    PROFILE_SCOPE(profiler, __FUNCTION__)

} // namespace opencog

#endif // _OPENCOG_AGENTZERO_PROFILER_H
