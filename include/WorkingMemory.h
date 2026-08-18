/*
 * opencog/agentzero/WorkingMemory.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * WorkingMemory - Active context window / short-term memory
 * Part of Agent-Zero Memory & Context Management module (Phase 7)
 * Part of the AGENT-ZERO-GENESIS project - AZ-MEM-002
 */

#ifndef _OPENCOG_AGENTZERO_WORKING_MEMORY_H
#define _OPENCOG_AGENTZERO_WORKING_MEMORY_H

#include <atomic>
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

namespace opencog {
namespace agentzero {

/**
 * A single item held in working memory.
 */
struct MemoryItem {
    Handle atom;
    double importance;
    std::string context;
    size_t access_count;
    double decay_rate;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::steady_clock::time_point last_access;

    MemoryItem(Handle a = Handle::UNDEFINED,
               double imp = 0.5,
               const std::string& ctx = "default")
        : atom(a)
        , importance(imp)
        , context(ctx)
        , access_count(0)
        , decay_rate(0.1)
        , timestamp(std::chrono::steady_clock::now())
        , last_access(std::chrono::steady_clock::now())
    {}
};

/**
 * WorkingMemory — bounded active context window for the cognitive loop.
 *
 * Stores recently relevant atoms with importance scores, context tags,
 * temporal decay, and capacity enforcement. Optional attention-bank
 * synchronisation is enabled when HAVE_ATTENTION is defined.
 */
class WorkingMemory
{
public:
    explicit WorkingMemory(AtomSpacePtr atomspace);
    WorkingMemory(AtomSpacePtr atomspace,
                  size_t max_capacity,
                  double importance_threshold,
                  std::chrono::seconds max_retention_time);
    ~WorkingMemory();

    // Core operations
    bool addItem(Handle atom, double importance = 0.5,
                 const std::string& context = "");
    std::shared_ptr<MemoryItem> getItem(Handle atom);
    bool hasItem(Handle atom) const;
    bool removeItem(Handle atom);
    bool updateImportance(Handle atom, double importance);
    std::shared_ptr<MemoryItem> accessItem(Handle atom);

    // Context operations
    std::vector<std::shared_ptr<MemoryItem>>
        getItemsByContext(const std::string& context) const;
    void setActiveContext(const std::string& context);
    std::string getActiveContext() const;
    size_t clearContext(const std::string& context);

    // Importance-based retrieval
    std::vector<std::shared_ptr<MemoryItem>>
        getImportantItems(double min_importance) const;
    std::vector<std::shared_ptr<MemoryItem>>
        getMostImportantItems(size_t max_items) const;
    std::vector<std::shared_ptr<MemoryItem>>
        getLeastImportantItems(size_t max_items) const;

    // Maintenance
    size_t runCleanup(bool force_cleanup = false);
    void applyTemporalDecay();
    void clear();
    void compactMemory();

    // Configuration
    void setMaxCapacity(size_t capacity);
    size_t getMaxCapacity() const { return _max_capacity; }
    size_t getCurrentSize() const;
    bool isAtCapacity() const;
    void setImportanceThreshold(double threshold);
    double getImportanceThreshold() const { return _importance_threshold; }

    // AtomSpace representation
    void createAtomSpaceRepresentation();
    void updateAtomSpaceRepresentation();

    // Metrics
    std::map<std::string, double> getPerformanceStats() const;
    double getHitRate() const;
    void resetPerformanceCounters();
    std::map<std::string, size_t> getMemoryUsage() const;
    void printMemoryContents(const std::string& log_level = "info") const;
    bool validateMemoryConsistency() const;
    std::string getItemInfo(Handle atom) const;

#ifdef HAVE_ATTENTION
    void synchronizeWithAttentionBank();
    void setAttentionBank(void* attention_bank);
#else
    void synchronizeWithAttentionBank() {}
    void setAttentionBank(void*) {}
#endif

private:
    AtomSpacePtr _atomspace;

    std::deque<std::shared_ptr<MemoryItem>> _memory_buffer;
    std::map<Handle, std::shared_ptr<MemoryItem>> _memory_index;
    std::multimap<std::string, std::shared_ptr<MemoryItem>> _context_index;
    std::multimap<double, std::shared_ptr<MemoryItem>> _importance_index;

    size_t _max_capacity;
    double _importance_threshold;
    std::chrono::seconds _max_retention_time;
    std::chrono::seconds _decay_interval;
    std::atomic<bool> _cleanup_running;

    size_t _access_count;
    size_t _hit_count;
    size_t _miss_count;
    size_t _total_operations;
    std::string _active_context;

    std::chrono::steady_clock::time_point _creation_time;
    std::chrono::steady_clock::time_point _last_cleanup;
    std::chrono::steady_clock::time_point _last_performance_reset;

    Handle _working_memory_root;
    Handle _context_space;
    Handle _active_goals;
    Handle _recent_percepts;
    Handle _temporary_conclusions;

    mutable std::recursive_mutex _memory_mutex;

    void initializeAtomSpaceStructures();
    void removeFromIndices(std::shared_ptr<MemoryItem> item);
    void addToIndices(std::shared_ptr<MemoryItem> item);
    size_t enforceCapacityLimits();
    double calculateDecayFactor(const std::shared_ptr<MemoryItem>& item) const;
    void updateIndicesForImportanceChange(std::shared_ptr<MemoryItem> item,
                                          double old_importance);
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_WORKING_MEMORY_H
