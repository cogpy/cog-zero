/*
 * standalone/include/cog0/EpisodicMemory.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Episodic memory management for the standalone cog0 agent.
 * Adapted from agentzero-memory/EpisodicMemory without OpenCog dependencies.
 */
#pragma once

#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "AtomStore.h"

namespace cog0 {

// -----------------------------------------------------------------------
// Episode — a single experience record

struct Episode {
    using Ptr = std::shared_ptr<Episode>;

    size_t      id       = 0;
    std::string type;        // e.g. "perception", "action", "reasoning"
    std::string content;     // textual description of the episode
    Handle      atom;        // AtomStore representation
    double      importance = 0.5;  // salience / importance [0,1]
    bool        recalled   = false;

    std::chrono::steady_clock::time_point timestamp;
    std::chrono::steady_clock::time_point lastAccess;

    std::vector<size_t> relatedIds;  // ids of related episodes

    Episode() : timestamp(std::chrono::steady_clock::now()),
                lastAccess(std::chrono::steady_clock::now()) {}
};

// -----------------------------------------------------------------------
// EpisodicMemory — thread-safe store for experience episodes

class EpisodicMemory {
public:
    explicit EpisodicMemory(std::shared_ptr<AtomStore> store,
                             size_t maxEpisodes = 1000);
    ~EpisodicMemory();

    // Record a new episode; returns the episode id
    size_t record(const std::string& type,
                  const std::string& content,
                  double importance = 0.5);

    // Record an episode already represented as an atom
    size_t recordAtom(const Handle& atom,
                      const std::string& type = "atom",
                      double importance = 0.5);

    // Link two episodes as related
    bool link(size_t id1, size_t id2);

    // Retrieve an episode by id (marks as recalled)
    Episode::Ptr recall(size_t id);

    // Retrieve most recent N episodes
    std::vector<Episode::Ptr> recentEpisodes(size_t n = 10) const;

    // Retrieve episodes of a specific type
    std::vector<Episode::Ptr> episodesByType(const std::string& type) const;

    // Retrieve high-importance episodes (importance >= threshold)
    std::vector<Episode::Ptr> importantEpisodes(double threshold = 0.7) const;

    // Search episodes by content substring
    std::vector<Episode::Ptr> search(const std::string& query) const;

    // Forget old, low-importance episodes to stay within capacity
    size_t forget(double importanceThreshold = 0.2);

    // Clear all episodes
    void clear();

    // Stats
    size_t size() const;
    size_t recalledCount() const;
    std::string statusReport() const;

private:
    std::shared_ptr<AtomStore>    _store;
    std::deque<Episode::Ptr>      _episodes;  // ordered by time (oldest first)
    std::map<size_t, Episode::Ptr> _byId;
    mutable std::mutex            _mutex;
    size_t _nextId      = 1;
    size_t _maxEpisodes;
    size_t _recalledCount = 0;

    void trimToCapacity();
};

} // namespace cog0
