/*
 * opencog/agentzero/memory/LongTermMemory.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * LongTermMemory - Persistent knowledge storage with RocksDB backing
 * Part of Agent-Zero Memory & Context Management module
 * Part of the AGENT-ZERO-GENESIS project - AZ-MEM-003
 */

#ifndef _OPENCOG_AGENTZERO_MEMORY_LONG_TERM_MEMORY_H
#define _OPENCOG_AGENTZERO_MEMORY_LONG_TERM_MEMORY_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

#ifdef HAVE_ATOMSPACE_ROCKS
#include <opencog/persist/rocks/RocksStorage.h>
#endif

#ifdef HAVE_ATTENTION_BANK
#include <opencog/attentionbank/avalue/AttentionValue.h>
#endif

#include "MemoryTypes.h"

namespace opencog {
namespace agentzero {
namespace memory {

/**
 * LongTermMemory - Persistent knowledge storage and retrieval
 *
 * This class provides long-term memory capabilities with persistence
 * using RocksDB backing store. It integrates with OpenCog's AtomSpace
 * and attention mechanisms for importance-based memory management.
 *
 * Key Features:
 * - Persistent storage using atomspace-rocks (RocksDB)
 * - Attention-based importance filtering for retention
 * - Memory consolidation and garbage collection
 * - Efficient retrieval with indexing and caching
 * - Integration with existing OpenCog ecosystem
 * - Support for different persistence levels
 * - Automatic backup and recovery
 */
class LongTermMemory
{
private:
    // Core components
    AtomSpacePtr _atomspace;
#ifdef HAVE_ATOMSPACE_ROCKS
    std::unique_ptr<RocksStorage> _persistent_storage;
#endif

    // Memory management
    MemoryConfig _config;
    std::map<Handle, MemoryRecord> _memory_records;
#ifdef HAVE_ATTENTION_BANK
    std::map<Handle, AttentionValuePtr> _importance_cache;
#endif
    std::set<Handle> _persistent_handles;
    
    // Indexing for efficient retrieval
    std::map<ContextType, std::set<Handle>> _context_index;
    std::map<TimePoint, std::set<Handle>> _temporal_index;
    std::map<MemoryImportance, std::set<Handle>> _importance_index;
    std::multimap<std::string, Handle> _keyword_index;
    
    // Caching for performance
    std::map<Handle, std::pair<TimePoint, Handle>> _access_cache;
    std::set<Handle> _dirty_handles;  // Handles that need persistence
    
    // Statistics and monitoring
    MemoryStatistics _statistics;
    ConsolidationStatus _consolidation_status;
    std::atomic<bool> _consolidation_running;
    
    // Threading and synchronization
    std::mutex _memory_mutex;
    std::mutex _persistence_mutex;
    std::mutex _statistics_mutex;
    std::thread _consolidation_thread;
    std::thread _backup_thread;
    std::atomic<bool> _shutdown_requested;
    
    // Internal memory management
    void initializePersistentStorage();
    void initializeMemoryStructures();
    void startBackgroundTasks();
    void stopBackgroundTasks();
    
    // Persistence operations
    bool persistHandle(const Handle& handle, bool force = false);
    bool loadHandle(const Handle& handle);
    bool removeFromPersistence(const Handle& handle);
    void flushDirtyHandles();
    
    // Memory consolidation
    void performMemoryConsolidation();
    void consolidationWorker();
    std::vector<Handle> identifyConsolidationCandidates();
    bool shouldRetainMemory(const Handle& handle);
    void updateMemoryImportance(const Handle& handle);
    
    // Indexing operations
    void addToIndices(const Handle& handle);
    void removeFromIndices(const Handle& handle);
    void updateIndices(const Handle& handle);
    void rebuildIndices();
    
    // Cache management
    void updateAccessCache(const Handle& handle);
    void cleanupAccessCache();
    bool isInCache(const Handle& handle) const;
    void evictFromCache(const Handle& handle);
    
    // Statistics and monitoring
    void updateStatistics(const std::string& operation, Duration duration);
    void logMemoryStatus();
    
    // Backup operations
    void backupWorker();
    bool createBackup(const std::string& backup_path);
    bool restoreFromBackup(const std::string& backup_path);
    
    // Utility methods
    MemoryImportance calculateMemoryImportance(const Handle& handle);
#ifdef HAVE_ATTENTION_BANK
    AttentionValuePtr getAttentionValue(const Handle& handle);
#endif
    std::string generateHandleKey(const Handle& handle);
    std::vector<Handle> getHandlesByImportance(MemoryImportance min_importance);

public:
    /**
     * Constructor
     * @param atomspace Shared pointer to the AtomSpace
     * @param config Memory configuration parameters
     */
    explicit LongTermMemory(AtomSpacePtr atomspace, 
                           const MemoryConfig& config = MemoryConfig());
    
    /**
     * Constructor with storage path
     * @param atomspace Shared pointer to the AtomSpace
     * @param storage_path Path for persistent storage
     * @param config Memory configuration parameters
     */
    LongTermMemory(AtomSpacePtr atomspace,
                  const std::string& storage_path,
                  const MemoryConfig& config = MemoryConfig());
    
    /**
     * Destructor - ensures clean shutdown and data persistence
     */
    ~LongTermMemory();
    
    /**
     * Initialize the long-term memory system
     * @return True if initialization succeeded
     */
    bool initialize();
    
    /**
     * Shutdown the long-term memory system gracefully
     * @return True if shutdown completed successfully
     */
    bool shutdown();
    
    // === Core Memory Operations ===
    
    /**
     * Store a handle in long-term memory
     * @param handle Handle to store
     * @param importance Importance level for retention decisions
     * @param persistence_level How the memory should be persisted
     * @param contexts Associated context types
     * @return True if storage succeeded
     */
    bool store(const Handle& handle,
              MemoryImportance importance = MemoryImportance::MEDIUM,
              PersistenceLevel persistence_level = PersistenceLevel::LONG_TERM,
              const std::vector<ContextType>& contexts = {});
    
    /**
     * Retrieve a handle from long-term memory
     * @param handle Handle to retrieve
     * @return Handle if found, Handle::UNDEFINED if not found
     */
    Handle retrieve(const Handle& handle);
    
    /**
     * Check if a handle exists in long-term memory
     * @param handle Handle to check
     * @return True if handle exists
     */
    bool contains(const Handle& handle) const;
    
    /**
     * Remove a handle from long-term memory
     * @param handle Handle to remove
     * @param remove_from_persistence Whether to remove from persistent storage
     * @return True if removal succeeded
     */
    bool remove(const Handle& handle, bool remove_from_persistence = true);
    
    /**
     * Update the importance of a stored memory
     * @param handle Handle to update
     * @param new_importance New importance level
     * @return True if update succeeded
     */
    bool updateImportance(const Handle& handle, MemoryImportance new_importance);
    
    // === Memory Retrieval and Search ===
    
    /**
     * Find memories by importance level
     * @param min_importance Minimum importance threshold
     * @param max_results Maximum number of results
     * @return Vector of handles matching criteria
     */
    std::vector<Handle> findByImportance(MemoryImportance min_importance,
                                        size_t max_results = 1000);
    
    /**
     * Find memories by context type
     * @param context_type Context type to match
     * @param max_results Maximum number of results
     * @return Vector of handles matching context
     */
    std::vector<Handle> findByContext(ContextType context_type,
                                     size_t max_results = 1000);
    
    /**
     * Find memories within a time range
     * @param start_time Start of time range
     * @param end_time End of time range
     * @param max_results Maximum number of results
     * @return Vector of handles from time period
     */
    std::vector<Handle> findByTimeRange(const TimePoint& start_time,
                                       const TimePoint& end_time,
                                       size_t max_results = 1000);
    
    /**
     * Find similar memories to a given handle
     * @param reference_handle Handle to use as similarity reference
     * @param max_results Maximum number of results
     * @param similarity_threshold Minimum similarity score
     * @return Vector of similar handles with similarity scores
     */
    std::vector<std::pair<Handle, double>> findSimilar(
        const Handle& reference_handle,
        size_t max_results = 10,
        double similarity_threshold = 0.5);
    
    /**
     * Search memories by keywords or patterns
     * @param keywords Keywords to search for
     * @param mode Search mode (exact, similarity, etc.)
     * @param max_results Maximum number of results
     * @return Vector of matching handles
     */
    std::vector<Handle> search(const std::vector<std::string>& keywords,
                              RetrievalMode mode = RetrievalMode::SIMILARITY,
                              size_t max_results = 100);
    
    // === Memory Consolidation and Management ===
    
    /**
     * Trigger immediate memory consolidation
     * @param force Force consolidation even if not needed
     * @return Consolidation status after operation
     */
    ConsolidationStatus consolidate(bool force = false);
    
    /**
     * Set memory consolidation strategy
     * @param strategy New consolidation strategy
     */
    void setConsolidationStrategy(ConsolidationStrategy strategy);
    
    /**
     * Get current consolidation status
     * @return Current consolidation status
     */
    ConsolidationStatus getConsolidationStatus() const;
    
    /**
     * Force persistence of all dirty memories
     * @return Number of handles persisted
     */
    size_t flushToPersistence();
    
    /**
     * Load all persistent memories into active memory
     * @return Number of handles loaded
     */
    size_t loadFromPersistence();
    
    // === Statistics and Monitoring ===
    
    /**
     * Get current memory statistics
     * @return Current memory statistics
     */
    MemoryStatistics getStatistics() const;
    
    /**
     * Get memory usage information
     * @return Map of usage metrics
     */
    std::map<std::string, size_t> getMemoryUsage() const;
    
    /**
     * Get performance metrics
     * @return Map of performance metrics
     */
    std::map<std::string, Duration> getPerformanceMetrics() const;
    
    /**
     * Reset statistics counters
     */
    void resetStatistics();
    
    // === Configuration and Management ===
    
    /**
     * Update memory configuration
     * @param new_config New configuration parameters
     * @return True if configuration update succeeded
     */
    bool updateConfig(const MemoryConfig& new_config);
    
    /**
     * Get current configuration
     * @return Current memory configuration
     */
    MemoryConfig getConfig() const;
    
    /**
     * Create a backup of the persistent memory
     * @param backup_path Path for backup file
     * @return True if backup succeeded
     */
    bool backup(const std::string& backup_path = "");
    
    /**
     * Restore from a backup file
     * @param backup_path Path to backup file
     * @return True if restore succeeded
     */
    bool restore(const std::string& backup_path);
    
    /**
     * Get system status information
     * @return Status information as string
     */
    std::string getSystemStatus() const;
    
    /**
     * Check if the system is healthy
     * @return True if all components are functioning normally
     */
    bool isHealthy() const;
};

} // namespace memory
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_MEMORY_LONG_TERM_MEMORY_H