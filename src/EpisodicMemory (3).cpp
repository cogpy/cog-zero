/*
 * src/EpisodicMemory.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * EpisodicMemory - Temporal sequence and experience management
 * Part of Agent-Zero Memory & Context Management module
 * Part of the AGENT-ZERO-GENESIS project - AZ-MEM-001
 */

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/memory/EpisodicMemory.h"

using namespace opencog;
using namespace opencog::agentzero::memory;

// ===================================================================
// Constructor and Destructor
// ===================================================================

EpisodicMemory::EpisodicMemory(
    AtomSpacePtr atomspace,
    size_t max_episodes,
    double min_importance,
    Duration max_retention_time
)
    : _atomspace(atomspace)
    , _max_episodes(max_episodes)
    , _min_importance(min_importance)
    , _max_retention_time(max_retention_time)
    , _sequence_counter(0)
{
    if (!_atomspace) {
        throw std::runtime_error("EpisodicMemory requires valid AtomSpace");
    }

    logger().info() << "[EpisodicMemory] Initializing with max_episodes=" << max_episodes
                    << ", min_importance=" << min_importance;
}

EpisodicMemory::~EpisodicMemory()
{
    shutdown();
}

// ===================================================================
// Initialization and Shutdown
// ===================================================================

bool EpisodicMemory::initialize()
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);

    try {
        logger().info() << "[EpisodicMemory] Starting initialization...";
        updateStatistics();
        logger().info() << "[EpisodicMemory] Initialization complete";
        return true;

    } catch (const std::exception& e) {
        logger().error() << "[EpisodicMemory] Initialization failed: " << e.what();
        return false;
    }
}

bool EpisodicMemory::shutdown()
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);

    try {
        logger().info() << "[EpisodicMemory] Shutting down...";
        _episodes.clear();
        _episode_index.clear();
        _temporal_index.clear();
        _context_index.clear();
        logger().info() << "[EpisodicMemory] Shutdown complete";
        return true;

    } catch (const std::exception& e) {
        logger().error() << "[EpisodicMemory] Shutdown error: " << e.what();
        return false;
    }
}

// ===================================================================
// Core Episode Operations
// ===================================================================

std::string EpisodicMemory::storeEpisode(
    const Handle& atom,
    double importance,
    const std::string& context,
    const std::map<std::string, std::string>& metadata
)
{
    if (atom == Handle::UNDEFINED) {
        logger().warn() << "[EpisodicMemory] Cannot store episode with undefined atom";
        return "";
    }

    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);

    std::string episode_id = generateEpisodeId();

    auto episode = std::make_shared<Episode>(episode_id, atom, importance, context);
    episode->atoms.push_back(atom);
    episode->metadata = metadata;
    episode->sequence_index = _sequence_counter++;

    _episodes.push_back(episode);
    addToIndices(episode);

    // Enforce capacity limits after adding
    enforceCapacityLimits();

    updateStatistics();

    logger().debug() << "[EpisodicMemory] Stored episode " << episode_id
                     << " with importance=" << importance
                     << " context=" << context;

    return episode_id;
}

std::string EpisodicMemory::storeEpisodeMulti(
    const std::vector<Handle>& atoms,
    double importance,
    const std::string& context,
    const std::map<std::string, std::string>& metadata
)
{
    if (atoms.empty()) {
        logger().warn() << "[EpisodicMemory] Cannot store episode with no atoms";
        return "";
    }

    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);

    std::string episode_id = generateEpisodeId();

    auto episode = std::make_shared<Episode>(episode_id, atoms[0], importance, context);
    episode->atoms = atoms;
    episode->metadata = metadata;
    episode->sequence_index = _sequence_counter++;

    _episodes.push_back(episode);
    addToIndices(episode);

    enforceCapacityLimits();
    updateStatistics();

    logger().debug() << "[EpisodicMemory] Stored multi-atom episode " << episode_id
                     << " with " << atoms.size() << " atoms";

    return episode_id;
}

std::shared_ptr<Episode> EpisodicMemory::retrieveEpisode(const std::string& episode_id) const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);

    auto it = _episode_index.find(episode_id);
    if (it != _episode_index.end()) {
        return it->second;
    }

    return nullptr;
}

bool EpisodicMemory::hasEpisode(const std::string& episode_id) const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    return _episode_index.find(episode_id) != _episode_index.end();
}

bool EpisodicMemory::removeEpisode(const std::string& episode_id)
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);

    auto it = _episode_index.find(episode_id);
    if (it == _episode_index.end()) {
        return false;
    }

    auto episode = it->second;
    removeFromIndices(episode_id);

    // Remove from deque
    auto deque_it = std::find_if(
        _episodes.begin(), _episodes.end(),
        [&episode_id](const std::shared_ptr<Episode>& ep) {
            return ep->episode_id == episode_id;
        }
    );
    if (deque_it != _episodes.end()) {
        _episodes.erase(deque_it);
    }

    updateStatistics();

    logger().debug() << "[EpisodicMemory] Removed episode " << episode_id;
    return true;
}

// ===================================================================
// Temporal Sequence Retrieval
// ===================================================================

std::vector<std::shared_ptr<Episode>> EpisodicMemory::getRecentEpisodes(size_t count) const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);

    std::vector<std::shared_ptr<Episode>> result;
    size_t start = (_episodes.size() > count) ? _episodes.size() - count : 0;

    for (size_t i = _episodes.size(); i > start; --i) {
        result.push_back(_episodes[i - 1]);
    }

    return result;
}

std::vector<std::shared_ptr<Episode>> EpisodicMemory::getEpisodesByTimeRange(
    const TimePoint& start,
    const TimePoint& end,
    size_t max_results
) const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);

    std::vector<std::shared_ptr<Episode>> result;

    auto it_begin = _temporal_index.lower_bound(start);
    auto it_end = _temporal_index.upper_bound(end);

    for (auto it = it_begin; it != it_end && result.size() < max_results; ++it) {
        auto ep_it = _episode_index.find(it->second);
        if (ep_it != _episode_index.end()) {
            result.push_back(ep_it->second);
        }
    }

    return result;
}

std::vector<std::shared_ptr<Episode>> EpisodicMemory::getTemporalSequence(
    size_t max_results
) const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);

    std::vector<std::shared_ptr<Episode>> result;
    size_t count = std::min(_episodes.size(), max_results);

    for (size_t i = 0; i < count; ++i) {
        result.push_back(_episodes[i]);
    }

    return result;
}

// ===================================================================
// Context-Based Retrieval
// ===================================================================

std::vector<std::shared_ptr<Episode>> EpisodicMemory::getEpisodesByContext(
    const std::string& context,
    size_t max_results
) const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);

    std::vector<std::shared_ptr<Episode>> result;

    auto range = _context_index.equal_range(context);
    for (auto it = range.first; it != range.second && result.size() < max_results; ++it) {
        auto ep_it = _episode_index.find(it->second);
        if (ep_it != _episode_index.end()) {
            result.push_back(ep_it->second);
        }
    }

    return result;
}

// ===================================================================
// Importance-Based Operations
// ===================================================================

std::vector<std::shared_ptr<Episode>> EpisodicMemory::getEpisodesByImportance(
    double min_importance,
    size_t max_results
) const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);

    std::vector<std::shared_ptr<Episode>> result;

    for (const auto& episode : _episodes) {
        if (episode->importance >= min_importance) {
            result.push_back(episode);
            if (result.size() >= max_results) {
                break;
            }
        }
    }

    return result;
}

bool EpisodicMemory::updateEpisodeImportance(
    const std::string& episode_id,
    double importance
)
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);

    auto it = _episode_index.find(episode_id);
    if (it == _episode_index.end()) {
        return false;
    }

    it->second->importance = std::max(0.0, std::min(1.0, importance));
    updateStatistics();
    return true;
}

// ===================================================================
// Query Interface
// ===================================================================

std::vector<std::shared_ptr<Episode>> EpisodicMemory::query(
    const EpisodicQuery& query_params
) const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);

    std::vector<std::shared_ptr<Episode>> result;

    for (const auto& episode : _episodes) {
        // Apply importance filter
        if (episode->importance < query_params.min_importance) {
            continue;
        }

        // Apply context filter
        if (!query_params.context_filter.empty() &&
            episode->context != query_params.context_filter) {
            continue;
        }

        // Apply time filter
        if (query_params.use_time_filter) {
            if (episode->timestamp < query_params.time_start ||
                episode->timestamp > query_params.time_end) {
                continue;
            }
        }

        result.push_back(episode);

        if (result.size() >= query_params.max_results) {
            break;
        }
    }

    return result;
}

// ===================================================================
// Statistics and Monitoring
// ===================================================================

EpisodicMemoryStatistics EpisodicMemory::getStatistics() const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    return _statistics;
}

size_t EpisodicMemory::getEpisodeCount() const
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);
    return _episodes.size();
}

bool EpisodicMemory::clearAll()
{
    std::lock_guard<std::recursive_mutex> lock(_memory_mutex);

    try {
        _episodes.clear();
        _episode_index.clear();
        _temporal_index.clear();
        _context_index.clear();
        _sequence_counter = 0;
        updateStatistics();
        logger().info() << "[EpisodicMemory] Cleared all episodes";
        return true;

    } catch (const std::exception& e) {
        logger().error() << "[EpisodicMemory] Clear failed: " << e.what();
        return false;
    }
}

// ===================================================================
// Private Helper Methods
// ===================================================================

std::string EpisodicMemory::generateEpisodeId()
{
    auto now = std::chrono::system_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();

    std::ostringstream ss;
    ss << "ep_" << std::hex << ns << "_" << std::dec << _sequence_counter;
    return ss.str();
}

void EpisodicMemory::enforceCapacityLimits()
{
    if (_max_episodes == 0) {
        return;
    }

    while (_episodes.size() > _max_episodes) {
        // Remove the oldest (lowest sequence index) episode
        auto oldest = _episodes.front();
        removeFromIndices(oldest->episode_id);
        _episode_index.erase(oldest->episode_id);
        _episodes.pop_front();
    }
}

void EpisodicMemory::updateStatistics()
{
    _statistics.total_episodes = _episodes.size();
    _statistics.total_sequences = _sequence_counter;
    _statistics.total_atoms = 0;

    if (_episodes.empty()) {
        _statistics.average_importance = 0.0;
        return;
    }

    double total_importance = 0.0;
    _statistics.oldest_episode = _episodes.front()->timestamp;
    _statistics.newest_episode = _episodes.back()->timestamp;

    for (const auto& episode : _episodes) {
        total_importance += episode->importance;
        _statistics.total_atoms += episode->atoms.size();
    }

    _statistics.average_importance = total_importance / _episodes.size();
}

void EpisodicMemory::addToIndices(std::shared_ptr<Episode> episode)
{
    _episode_index[episode->episode_id] = episode;
    _temporal_index.emplace(episode->timestamp, episode->episode_id);

    if (!episode->context.empty()) {
        _context_index.emplace(episode->context, episode->episode_id);
    }
}

void EpisodicMemory::removeFromIndices(const std::string& episode_id)
{
    auto ep_it = _episode_index.find(episode_id);
    if (ep_it == _episode_index.end()) {
        return;
    }

    auto episode = ep_it->second;

    // Remove from temporal index
    auto temporal_range = _temporal_index.equal_range(episode->timestamp);
    for (auto it = temporal_range.first; it != temporal_range.second; ++it) {
        if (it->second == episode_id) {
            _temporal_index.erase(it);
            break;
        }
    }

    // Remove from context index
    if (!episode->context.empty()) {
        auto context_range = _context_index.equal_range(episode->context);
        for (auto it = context_range.first; it != context_range.second; ++it) {
            if (it->second == episode_id) {
                _context_index.erase(it);
                break;
            }
        }
    }

    _episode_index.erase(ep_it);
}
