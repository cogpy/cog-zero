/*
 * opencog/agentzero/memory/LongTermMemory.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * LongTermMemory - Persistent storage via AtomSpace / optional RocksDB backend
 * Part of Agent-Zero Memory & Context Management module (Phase 7)
 * Part of the AGENT-ZERO-GENESIS project - AZ-MEM-003
 */

#ifndef _OPENCOG_AGENTZERO_MEMORY_LONG_TERM_MEMORY_H
#define _OPENCOG_AGENTZERO_MEMORY_LONG_TERM_MEMORY_H

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

#include <opencog/agentzero/memory/MemoryTypes.h>

namespace opencog {
namespace agentzero {
namespace memory {

/**
 * LongTermMemory — durable atom storage with consolidation and optional
 * AtomSpace-RocksDB persistence (HAVE_ATOMSPACE_ROCKS).
 *
 * Without rocks, all operations remain fully functional in-memory and
 * still honour importance / context / temporal indices and backup hooks.
 */
class LongTermMemory
{
public:
    explicit LongTermMemory(AtomSpacePtr atomspace,
                            const MemoryConfig& config = MemoryConfig());
    LongTermMemory(AtomSpacePtr atomspace,
                   const std::string& storage_path,
                   const MemoryConfig& config = MemoryConfig());
    ~LongTermMemory();

    bool initialize();
    bool shutdown();

    // Core storage
    bool store(const Handle& handle,
               MemoryImportance importance = MemoryImportance::MEDIUM,
               PersistenceLevel persistence_level = PersistenceLevel::MEDIUM_TERM,
               const std::vector<ContextType>& contexts = {});
    Handle retrieve(const Handle& handle);
    bool contains(const Handle& handle) const;
    bool remove(const Handle& handle, bool remove_from_persistence = true);
    bool updateImportance(const Handle& handle, MemoryImportance new_importance);

    // Queries
    std::vector<Handle> findByImportance(MemoryImportance min_importance,
                                         size_t max_results = 100);
    std::vector<Handle> findByContext(ContextType context_type,
                                      size_t max_results = 100);
    std::vector<Handle> findByTimeRange(const TimePoint& start_time,
                                        const TimePoint& end_time,
                                        size_t max_results = 100);
    std::vector<std::pair<Handle, double>> findSimilar(
        const Handle& reference_handle,
        size_t max_results = 10,
        double similarity_threshold = 0.5);
    std::vector<Handle> search(const std::vector<std::string>& keywords,
                               RetrievalMode mode = RetrievalMode::ASSOCIATIVE,
                               size_t max_results = 100);
    std::vector<Handle> getHandlesByImportance(MemoryImportance min_importance);

    // Consolidation & persistence
    ConsolidationStatus consolidate(bool force = false);
    void setConsolidationStrategy(ConsolidationStrategy strategy);
    ConsolidationStatus getConsolidationStatus() const;
    size_t flushToPersistence();
    size_t loadFromPersistence();
    bool backup(const std::string& backup_path = "");
    bool restore(const std::string& backup_path);

    // Status / metrics
    MemoryStatistics getStatistics() const;
    MemoryConfig getConfig() const;
    bool updateConfig(const MemoryConfig& new_config);
    std::string getSystemStatus() const;
    bool isHealthy() const;
    std::map<std::string, size_t> getMemoryUsage() const;
    std::map<std::string, Duration> getPerformanceMetrics() const;
    void resetStatistics();
    void logMemoryStatus();

private:
    AtomSpacePtr _atomspace;
    MemoryConfig _config;

    std::map<Handle, MemoryRecord> _memory_records;
    std::set<Handle> _persistent_handles;
    std::set<Handle> _dirty_handles;

    std::map<MemoryImportance, std::set<Handle>> _importance_index;
    std::map<TimePoint, std::set<Handle>> _temporal_index;
    std::map<ContextType, std::set<Handle>> _context_index;
    std::multimap<std::string, Handle> _keyword_index;
    std::map<Handle, std::pair<TimePoint, size_t>> _access_cache;

    MemoryStatistics _statistics;
    ConsolidationStatus _consolidation_status;

    std::atomic<bool> _consolidation_running;
    std::atomic<bool> _shutdown_requested;
    std::thread _consolidation_thread;
    std::thread _backup_thread;
    mutable std::mutex _memory_mutex;
    mutable std::mutex _statistics_mutex;
    std::condition_variable _consolidation_cv;

#ifdef HAVE_ATOMSPACE_ROCKS
    std::unique_ptr<class RocksStorage> _persistent_storage;
#endif

    void initializePersistentStorage();
    void initializeMemoryStructures();
    void startBackgroundTasks();
    void stopBackgroundTasks();

    bool persistHandle(const Handle& handle, bool force = false);
    bool loadHandle(const Handle& handle);
    void removeFromPersistence(const Handle& handle);
    void flushDirtyHandles();

    void addToIndices(const Handle& handle);
    void removeFromIndices(const Handle& handle);
    void updateIndices(const Handle& handle);
    void rebuildIndices();
    void updateAccessCache(const Handle& handle);
    void cleanupAccessCache();
    bool isInCache(const Handle& handle) const;
    void evictFromCache(const Handle& handle);

    MemoryImportance calculateMemoryImportance(const Handle& handle);
    void updateMemoryImportance(const Handle& handle);
    std::vector<Handle> identifyConsolidationCandidates();
    bool shouldRetainMemory(const Handle& handle);
    void performMemoryConsolidation();
    void consolidationWorker();
    void backupWorker();
    bool createBackup(const std::string& backup_path);
    bool restoreFromBackup(const std::string& backup_path);

    void updateStatistics(const std::string& operation, Duration duration);
    std::string generateHandleKey(const Handle& handle);
};

} // namespace memory
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_MEMORY_LONG_TERM_MEMORY_H
