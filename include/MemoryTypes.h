/*
 * opencog/agentzero/memory/MemoryTypes.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Memory Types and Constants
 * Part of Agent-Zero Memory & Context Management module
 * Part of the AGENT-ZERO-GENESIS project - AZ-MEM-003
 */

#ifndef _OPENCOG_AGENTZERO_MEMORY_TYPES_H
#define _OPENCOG_AGENTZERO_MEMORY_TYPES_H

#include <memory>
#include <string>
#include <chrono>
#include <vector>
#include <map>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/TruthValue.h>

namespace opencog {
namespace agentzero {
namespace memory {

// Forward declarations
class LongTermMemory;
class EpisodicMemory;
class WorkingMemory;
class ContextManager;

// Type aliases for convenience
using AtomSpacePtr = std::shared_ptr<AtomSpace>;
using MemoryHandle = Handle;
using TimePoint = std::chrono::system_clock::time_point;
using Duration = std::chrono::milliseconds;

/**
 * Memory importance levels for retention decisions
 */
enum class MemoryImportance {
    CRITICAL = 100,    // Must persist (system knowledge, learned skills)
    HIGH = 75,         // Important experiences, successful patterns  
    MEDIUM = 50,       // Regular experiences, moderate success/failure
    LOW = 25,          // Minor experiences, routine operations
    MINIMAL = 10       // Temporary data, low-value information
};

/**
 * Memory persistence levels
 */
enum class PersistenceLevel {
    PERMANENT,         // Never removed, always persisted
    LONG_TERM,         // Persisted to disk, subject to consolidation
    MEDIUM_TERM,       // In memory, persisted periodically
    SHORT_TERM,        // In memory only, cleared on restart
    TEMPORARY          // Cleared automatically after timeout
};

/**
 * Memory access patterns for optimization
 */
enum class AccessPattern {
    FREQUENT,          // Accessed regularly, keep in cache
    PERIODIC,          // Accessed occasionally, cache on demand
    RARE,              // Rarely accessed, load from storage
    SEQUENTIAL,        // Accessed in temporal order
    RANDOM            // Unpredictable access pattern
};

/**
 * Context types for memory organization
 */
enum class ContextType {
    TEMPORAL,          // Time-based context
    SPATIAL,           // Location-based context  
    TASK,              // Task or goal context
    SOCIAL,            // Social interaction context
    EMOTIONAL,         // Emotional state context
    ENVIRONMENTAL,     // Environmental conditions
    COGNITIVE          // Cognitive state and processes
};

/**
 * Memory consolidation strategies
 */
enum class ConsolidationStrategy {
    IMPORTANCE_BASED,   // Retain based on importance scores
    RECENCY_BASED,      // Retain more recent memories
    ACCESS_BASED,       // Retain frequently accessed memories
    PATTERN_BASED,      // Retain memories that form patterns
    HYBRID              // Combination of multiple strategies
};

/**
 * Memory retrieval modes
 */
enum class RetrievalMode {
    EXACT_MATCH,       // Find exact matches only
    SIMILARITY,        // Find similar memories
    ASSOCIATIVE,       // Find related/associated memories
    TEMPORAL,          // Find memories from time period
    CONTEXTUAL,        // Find memories matching context
    IMPORTANCE         // Find memories above importance threshold
};

/**
 * Memory consolidation status
 */
struct ConsolidationStatus {
    size_t total_memories;
    size_t consolidated_memories;
    size_t removed_memories;
    TimePoint last_consolidation;
    Duration consolidation_time;
    
    ConsolidationStatus() 
        : total_memories(0)
        , consolidated_memories(0) 
        , removed_memories(0)
        , last_consolidation(std::chrono::system_clock::now())
        , consolidation_time(Duration::zero()) {}
};

/**
 * Memory statistics for monitoring
 */
struct MemoryStatistics {
    // Size statistics
    size_t total_memories;
    size_t persistent_memories;
    size_t cached_memories;
    size_t memory_usage_bytes;
    
    // Access statistics
    size_t read_operations;
    size_t write_operations;
    size_t cache_hits;
    size_t cache_misses;
    
    // Performance statistics
    Duration average_read_time;
    Duration average_write_time;
    Duration average_consolidation_time;
    
    MemoryStatistics()
        : total_memories(0)
        , persistent_memories(0)
        , cached_memories(0) 
        , memory_usage_bytes(0)
        , read_operations(0)
        , write_operations(0)
        , cache_hits(0)
        , cache_misses(0)
        , average_read_time(Duration::zero())
        , average_write_time(Duration::zero())
        , average_consolidation_time(Duration::zero()) {}
};

/**
 * Memory configuration parameters
 */
struct MemoryConfig {
    // Retention parameters
    MemoryImportance min_retention_importance;
    Duration max_retention_period;
    size_t max_memory_count;
    size_t max_memory_size_bytes;
    
    // Consolidation parameters
    ConsolidationStrategy consolidation_strategy;
    Duration consolidation_interval;
    double consolidation_threshold;
    
    // Cache parameters
    size_t cache_size;
    Duration cache_expiry_time;
    AccessPattern default_access_pattern;
    
    // Persistence parameters
    std::string persistence_directory;
    bool enable_compression;
    bool enable_incremental_backup;
    Duration backup_interval;
    
    MemoryConfig()
        : min_retention_importance(MemoryImportance::LOW)
        , max_retention_period(std::chrono::hours(24 * 30))  // 30 days
        , max_memory_count(100000)
        , max_memory_size_bytes(1024 * 1024 * 1024)  // 1GB
        , consolidation_strategy(ConsolidationStrategy::HYBRID)
        , consolidation_interval(std::chrono::hours(6))
        , consolidation_threshold(0.7)
        , cache_size(10000)
        , cache_expiry_time(std::chrono::hours(1))
        , default_access_pattern(AccessPattern::FREQUENT)
        , persistence_directory("./memory_data")
        , enable_compression(true)
        , enable_incremental_backup(true)
        , backup_interval(std::chrono::hours(24)) {}
};

/**
 * Memory record structure for tracking individual memories
 */
struct MemoryRecord {
    MemoryHandle handle;
    MemoryImportance importance;
    PersistenceLevel persistence_level;
    AccessPattern access_pattern;
    
    TimePoint created_time;
    TimePoint last_accessed_time;
    TimePoint last_modified_time;
    
    size_t access_count;
    size_t modification_count;
    size_t memory_size_bytes;
    
    std::vector<ContextType> contexts;
    std::map<std::string, std::string> metadata;
    
    MemoryRecord(const MemoryHandle& h = Handle::UNDEFINED)
        : handle(h)
        , importance(MemoryImportance::MEDIUM)
        , persistence_level(PersistenceLevel::MEDIUM_TERM)
        , access_pattern(AccessPattern::PERIODIC)
        , created_time(std::chrono::system_clock::now())
        , last_accessed_time(std::chrono::system_clock::now())
        , last_modified_time(std::chrono::system_clock::now())
        , access_count(0)
        , modification_count(0)
        , memory_size_bytes(0) {}
};

/**
 * Constants for memory management
 */
namespace Constants {
    // Default values
    const size_t DEFAULT_CACHE_SIZE = 10000;
    const size_t DEFAULT_MAX_MEMORIES = 1000000;
    const double DEFAULT_IMPORTANCE_THRESHOLD = 0.3;
    const size_t DEFAULT_CONSOLIDATION_BATCH_SIZE = 1000;
    
    // Timing constants
    const Duration DEFAULT_CONSOLIDATION_INTERVAL = std::chrono::hours(6);
    const Duration DEFAULT_BACKUP_INTERVAL = std::chrono::hours(24);
    const Duration DEFAULT_CACHE_EXPIRY = std::chrono::hours(1);
    
    // Performance constants
    const size_t MAX_CONCURRENT_OPERATIONS = 100;
    const Duration MAX_OPERATION_TIMEOUT = std::chrono::seconds(30);
    const size_t MIN_FREE_MEMORY_MB = 100;
    
    // Persistence constants
    const std::string DEFAULT_PERSISTENCE_DIR = "./agentzero_memory";
    const std::string BACKUP_FILE_EXTENSION = ".backup";
    const std::string METADATA_FILE_NAME = "memory_metadata.json";
}

} // namespace memory
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_MEMORY_TYPES_H