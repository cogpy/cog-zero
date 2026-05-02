/*
 * opencog/agentzero/memory/EpisodicMemory.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * EpisodicMemory - Manages temporal sequences and experiences
 * Part of Agent-Zero Memory & Context Management module
 * Part of the AGENT-ZERO-GENESIS project - AZ-MEM-001
 */

#ifndef _OPENCOG_AGENTZERO_MEMORY_EPISODIC_MEMORY_H
#define _OPENCOG_AGENTZERO_MEMORY_EPISODIC_MEMORY_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <mutex>
#include <chrono>
#include <functional>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/util/Logger.h>

#include "MemoryTypes.h"

namespace opencog {
namespace agentzero {
namespace memory {

/**
 * An episode represents a single temporal event or experience
 */
struct Episode {
    std::string episode_id;                          // Unique episode identifier
    Handle root_atom;                                // Root atom of the episode
    std::vector<Handle> atoms;                       // All atoms in the episode
    std::map<std::string, std::string> metadata;     // Episode metadata
    TimePoint timestamp;                             // When this episode occurred
    double importance;                               // Episode importance (0.0-1.0)
    std::string context;                             // Context tag for the episode
    size_t sequence_index;                           // Position in temporal sequence

    Episode()
        : episode_id("")
        , root_atom(Handle::UNDEFINED)
        , timestamp(std::chrono::system_clock::now())
        , importance(0.5)
        , context("")
        , sequence_index(0) {}

    Episode(const std::string& id, Handle atom, double imp = 0.5,
            const std::string& ctx = "")
        : episode_id(id)
        , root_atom(atom)
        , timestamp(std::chrono::system_clock::now())
        , importance(imp)
        , context(ctx)
        , sequence_index(0) {}
};

/**
 * Episodic memory query for retrieval
 */
struct EpisodicQuery {
    std::string context_filter;          // Filter by context (empty = any)
    TimePoint time_start;                // Start of time range
    TimePoint time_end;                  // End of time range
    double min_importance;               // Minimum importance threshold
    size_t max_results;                  // Maximum number of results
    bool use_time_filter;                // Whether to apply time filter

    EpisodicQuery()
        : context_filter("")
        , time_start(std::chrono::system_clock::time_point::min())
        , time_end(std::chrono::system_clock::now())
        , min_importance(0.0)
        , max_results(100)
        , use_time_filter(false) {}
};

/**
 * Statistics for episodic memory
 */
struct EpisodicMemoryStatistics {
    size_t total_episodes;
    size_t total_sequences;
    double average_importance;
    size_t total_atoms;
    TimePoint oldest_episode;
    TimePoint newest_episode;

    EpisodicMemoryStatistics()
        : total_episodes(0)
        , total_sequences(0)
        , average_importance(0.0)
        , total_atoms(0)
        , oldest_episode(std::chrono::system_clock::now())
        , newest_episode(std::chrono::system_clock::now()) {}
};

/**
 * EpisodicMemory - Manages temporal sequences and experiences
 *
 * This class provides episodic memory capabilities for Agent-Zero, storing
 * temporal sequences of experiences and enabling retrieval by time, context,
 * or content similarity.
 *
 * Key Features:
 * - Temporal sequence storage with ordered indexing
 * - Context-aware episode organization
 * - Time-range and importance-based retrieval
 * - AtomSpace integration for semantic representation
 * - Thread-safe operations
 * - Configurable capacity and retention policies
 */
class EpisodicMemory
{
private:
    AtomSpacePtr _atomspace;

    // Episode storage
    std::deque<std::shared_ptr<Episode>> _episodes;                   // Ordered sequence
    std::map<std::string, std::shared_ptr<Episode>> _episode_index;  // ID -> episode
    std::multimap<TimePoint, std::string> _temporal_index;           // time -> episode_id
    std::multimap<std::string, std::string> _context_index;          // context -> episode_id

    // Configuration
    size_t _max_episodes;
    double _min_importance;
    Duration _max_retention_time;
    size_t _sequence_counter;

    // Statistics
    EpisodicMemoryStatistics _statistics;

    // Thread safety
    mutable std::recursive_mutex _memory_mutex;

    // Internal methods
    std::string generateEpisodeId();
    void enforceCapacityLimits();
    void updateStatistics();
    void addToIndices(std::shared_ptr<Episode> episode);
    void removeFromIndices(const std::string& episode_id);

public:
    /**
     * Constructor
     * @param atomspace Shared pointer to AtomSpace
     * @param max_episodes Maximum number of episodes to retain (default: 10000)
     * @param min_importance Minimum importance for retention (default: 0.0)
     * @param max_retention_time Maximum retention duration (default: 30 days)
     */
    explicit EpisodicMemory(
        AtomSpacePtr atomspace,
        size_t max_episodes = 10000,
        double min_importance = 0.0,
        Duration max_retention_time = std::chrono::hours(24 * 30)
    );

    ~EpisodicMemory();

    /**
     * Initialize the episodic memory system
     * @return True if initialization succeeded
     */
    bool initialize();

    /**
     * Shutdown the episodic memory system gracefully
     * @return True if shutdown succeeded
     */
    bool shutdown();

    // === Core Episode Operations ===

    /**
     * Store a new episode
     * @param atom Root atom of the episode
     * @param importance Episode importance (0.0-1.0)
     * @param context Optional context tag
     * @param metadata Optional metadata map
     * @return Episode ID if stored successfully, empty string on failure
     */
    std::string storeEpisode(
        const Handle& atom,
        double importance = 0.5,
        const std::string& context = "",
        const std::map<std::string, std::string>& metadata = {}
    );

    /**
     * Store a multi-atom episode
     * @param atoms Atoms composing the episode
     * @param importance Episode importance (0.0-1.0)
     * @param context Optional context tag
     * @param metadata Optional metadata map
     * @return Episode ID if stored successfully, empty string on failure
     */
    std::string storeEpisodeMulti(
        const std::vector<Handle>& atoms,
        double importance = 0.5,
        const std::string& context = "",
        const std::map<std::string, std::string>& metadata = {}
    );

    /**
     * Retrieve an episode by ID
     * @param episode_id Episode identifier
     * @return Shared pointer to Episode, nullptr if not found
     */
    std::shared_ptr<Episode> retrieveEpisode(const std::string& episode_id) const;

    /**
     * Check if an episode exists
     * @param episode_id Episode identifier
     * @return True if the episode exists
     */
    bool hasEpisode(const std::string& episode_id) const;

    /**
     * Remove an episode
     * @param episode_id Episode identifier
     * @return True if removed successfully
     */
    bool removeEpisode(const std::string& episode_id);

    // === Temporal Sequence Retrieval ===

    /**
     * Get the most recent N episodes
     * @param count Number of episodes to retrieve
     * @return Vector of episodes (most recent first)
     */
    std::vector<std::shared_ptr<Episode>> getRecentEpisodes(size_t count = 10) const;

    /**
     * Get episodes within a time range
     * @param start Start time
     * @param end End time
     * @param max_results Maximum results
     * @return Vector of episodes in time range
     */
    std::vector<std::shared_ptr<Episode>> getEpisodesByTimeRange(
        const TimePoint& start,
        const TimePoint& end,
        size_t max_results = 100
    ) const;

    /**
     * Get all episodes in temporal order (oldest first)
     * @param max_results Maximum number of results
     * @return Ordered vector of episodes
     */
    std::vector<std::shared_ptr<Episode>> getTemporalSequence(
        size_t max_results = 1000
    ) const;

    // === Context-Based Retrieval ===

    /**
     * Get episodes by context
     * @param context Context tag to filter by
     * @param max_results Maximum number of results
     * @return Vector of episodes matching context
     */
    std::vector<std::shared_ptr<Episode>> getEpisodesByContext(
        const std::string& context,
        size_t max_results = 100
    ) const;

    // === Importance-Based Operations ===

    /**
     * Get episodes above an importance threshold
     * @param min_importance Minimum importance value
     * @param max_results Maximum number of results
     * @return Vector of episodes above threshold
     */
    std::vector<std::shared_ptr<Episode>> getEpisodesByImportance(
        double min_importance,
        size_t max_results = 100
    ) const;

    /**
     * Update the importance of an episode
     * @param episode_id Episode identifier
     * @param importance New importance value
     * @return True if updated successfully
     */
    bool updateEpisodeImportance(const std::string& episode_id, double importance);

    // === Query Interface ===

    /**
     * Query episodes with filtering options
     * @param query EpisodicQuery with filter parameters
     * @return Vector of matching episodes
     */
    std::vector<std::shared_ptr<Episode>> query(const EpisodicQuery& query) const;

    // === Statistics and Monitoring ===

    /**
     * Get episodic memory statistics
     * @return Current statistics
     */
    EpisodicMemoryStatistics getStatistics() const;

    /**
     * Get total number of stored episodes
     * @return Episode count
     */
    size_t getEpisodeCount() const;

    /**
     * Clear all episodes
     * @return True if cleared successfully
     */
    bool clearAll();
};

} // namespace memory
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_MEMORY_EPISODIC_MEMORY_H