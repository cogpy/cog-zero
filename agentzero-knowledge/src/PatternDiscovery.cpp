/*
 * opencog/agentzero/knowledge/PatternDiscovery.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * PatternDiscovery Implementation
 * Part of Agent-Zero Knowledge Representation & Reasoning module
 * Part of the AGENT-ZERO-GENESIS project - Phase 3
 */

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/knowledge/PatternDiscovery.h"

using namespace opencog;
using namespace opencog::agentzero::knowledge;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

PatternDiscovery::PatternDiscovery(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
{
    if (!_atomspace)
        throw std::runtime_error("PatternDiscovery requires a valid AtomSpace");
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool PatternDiscovery::initialize()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_initialized) return true;
    logger().info() << "[PatternDiscovery] Initializing";
    _initialized = true;
    return true;
}

bool PatternDiscovery::shutdown()
{
    std::lock_guard<std::mutex> lock(_mutex);
    logger().info() << "[PatternDiscovery] Shutting down";
    _initialized = false;
    return true;
}

// ---------------------------------------------------------------------------
// Episode Management
// ---------------------------------------------------------------------------

bool PatternDiscovery::recordEpisode(
    const Handle& root,
    const std::string& episode_id,
    const std::map<std::string, std::string>& metadata)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!root) return false;

    EpisodeRecord rec;
    rec.root = root;
    rec.episode_id = episode_id;
    rec.timestamp = std::chrono::system_clock::now();
    rec.metadata = metadata;
    _episodes.push_back(std::move(rec));
    logger().debug() << "[PatternDiscovery] Recorded episode: " << episode_id;
    return true;
}

bool PatternDiscovery::removeEpisode(const std::string& episode_id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = std::remove_if(_episodes.begin(), _episodes.end(),
                              [&](const EpisodeRecord& r) {
                                  return r.episode_id == episode_id;
                              });
    if (it == _episodes.end()) return false;
    _episodes.erase(it, _episodes.end());
    return true;
}

size_t PatternDiscovery::episodeCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _episodes.size();
}

std::vector<EpisodeRecord> PatternDiscovery::getEpisodes() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _episodes;
}

void PatternDiscovery::clearEpisodes()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _episodes.clear();
    _cached_patterns.clear();
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void PatternDiscovery::collectSubHandles(const Handle& root,
                                          std::set<Handle>& out,
                                          size_t current_depth,
                                          size_t max_depth) const
{
    if (!root || current_depth > max_depth) return;
    if (!out.insert(root).second) return; // already visited

    if (root->is_link()) {
        for (auto& child : root->getOutgoingSet())
            collectSubHandles(child, out, current_depth + 1, max_depth);
    }
}

std::map<Handle, std::set<size_t>> PatternDiscovery::buildSupportMap(
    size_t max_depth) const
{
    std::map<Handle, std::set<size_t>> support;
    for (size_t i = 0; i < _episodes.size(); ++i) {
        std::set<Handle> subs;
        collectSubHandles(_episodes[i].root, subs, 0, max_depth);
        for (auto& h : subs)
            support[h].insert(i);
    }
    return support;
}

std::vector<Handle> PatternDiscovery::abstractify(const Handle& templ,
                                                    size_t max_vars) const
{
    // Simple abstraction: return the template itself plus a version where
    // each leaf ConceptNode is replaced by a variable node.
    // For a full implementation this would use URE/Miner's abstraction.
    std::vector<Handle> results;
    results.push_back(templ);

    if (!templ->is_link() || max_vars == 0) return results;

    // Create one abstract version: replace all leaf ConceptNodes with
    // a single shared variable $X
    HandleSeq new_outgoing;
    bool abstracted = false;
    for (auto& child : templ->getOutgoingSet()) {
        if (child->is_node() && child->get_type() == CONCEPT_NODE && max_vars > 0) {
            Handle var = _atomspace->add_node(VARIABLE_NODE, "$X");
            new_outgoing.push_back(var);
            abstracted = true;
        } else {
            new_outgoing.push_back(child);
        }
    }
    if (abstracted) {
        try {
            Handle abstract = _atomspace->add_link(templ->get_type(), new_outgoing);
            results.push_back(abstract);
        } catch (...) {
            // Skip if creation fails
        }
    }
    return results;
}

double PatternDiscovery::computeSurprisingness(
    const Handle& pattern,
    double pattern_freq,
    const std::map<Handle, double>& freq_map) const
{
    if (!pattern->is_link()) return 0.0;

    double denom = 1.0;
    for (auto& child : pattern->getOutgoingSet()) {
        auto it = freq_map.find(child);
        if (it != freq_map.end() && it->second > 0.0)
            denom *= it->second;
        else
            denom *= 0.01; // unseen child → low prior
    }
    if (denom <= 0.0) return 0.0;
    double raw = pattern_freq / denom;
    // Clamp to [0,1]
    return std::min(1.0, raw);
}

std::string PatternDiscovery::describePattern(const DiscoveredPattern& p) const
{
    std::ostringstream ss;
    if (p.pattern->is_node()) {
        ss << p.pattern->get_type_name() << "(" << p.pattern->get_name() << ")";
    } else {
        ss << p.pattern->get_type_name()
           << "[arity=" << p.pattern->getArity() << "]";
    }
    ss << " freq=" << p.frequency
       << " support=" << p.support
       << " surprise=" << p.surprisingness;
    return ss.str();
}

// ---------------------------------------------------------------------------
// Pattern Mining (public)
// ---------------------------------------------------------------------------

std::vector<DiscoveredPattern> PatternDiscovery::minePatterns(
    const MiningConfig& config)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_episodes.empty()) return {};

    const size_t n = _episodes.size();

    // Step 1: Build support map (atom → set of episode indices)
    auto support_map = buildSupportMap(config.max_pattern_size);

    // Step 2: Compute frequency map
    std::map<Handle, double> freq_map;
    for (auto& [h, ep_set] : support_map)
        freq_map[h] = static_cast<double>(ep_set.size()) / static_cast<double>(n);

    // Step 3: Collect atoms meeting min_support
    std::vector<std::pair<Handle, size_t>> frequent;
    for (auto& [h, ep_set] : support_map) {
        if (ep_set.size() >= config.min_support)
            frequent.emplace_back(h, ep_set.size());
    }

    // Step 4: Build DiscoveredPattern objects (cap at max_iterations)
    std::vector<DiscoveredPattern> patterns;
    size_t iterations = 0;
    for (auto& [h, sup] : frequent) {
        if (iterations++ >= config.max_iterations) break;
        if (patterns.size() >= config.max_results) break;

        double freq = static_cast<double>(sup) / static_cast<double>(n);
        double surprise = h->is_link()
            ? computeSurprisingness(h, freq, freq_map)
            : 0.0;

        if (surprise < config.min_surprisingness && h->is_link()) continue;

        DiscoveredPattern dp;
        dp.pattern = h;
        dp.frequency = freq;
        dp.surprisingness = surprise;
        dp.support = sup;
        dp.discovered_at = std::chrono::system_clock::now();

        // Collect concrete instances
        auto& ep_set = support_map.at(h);
        for (size_t idx : ep_set)
            dp.instances.push_back(_episodes[idx].root);

        dp.description = describePattern(dp);
        patterns.push_back(std::move(dp));
    }

    // Step 5: Sort by (surprisingness * frequency) descending
    std::sort(patterns.begin(), patterns.end(),
              [](const DiscoveredPattern& a, const DiscoveredPattern& b) {
                  return (a.surprisingness * a.frequency) >
                         (b.surprisingness * b.frequency);
              });

    if (patterns.size() > config.max_results)
        patterns.resize(config.max_results);

    _cached_patterns = patterns;
    logger().info() << "[PatternDiscovery] Mined " << patterns.size() << " patterns";
    return patterns;
}

std::vector<DiscoveredPattern> PatternDiscovery::mineRecentPatterns(
    std::chrono::system_clock::time_point since,
    const MiningConfig& config)
{
    // Build a filtered copy of the episode list while holding the lock,
    // then release the lock before calling minePatterns (which acquires it).
    std::vector<EpisodeRecord> saved;
    std::vector<EpisodeRecord> recent;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        saved = _episodes;
        for (auto& r : _episodes)
            if (r.timestamp >= since) recent.push_back(r);
    }

    // Temporarily replace episode list, mine, then restore.
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _episodes = std::move(recent);
    }
    auto result = minePatterns(config);
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _episodes = std::move(saved);
    }
    return result;
}

std::vector<DiscoveredPattern> PatternDiscovery::updatePatterns(const Handle& root)
{
    recordEpisode(root, "incremental-" +
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    return minePatterns();
}

// ---------------------------------------------------------------------------
// Result Access
// ---------------------------------------------------------------------------

std::vector<DiscoveredPattern> PatternDiscovery::getCachedPatterns() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _cached_patterns;
}

std::vector<DiscoveredPattern> PatternDiscovery::findMatchingPatterns(
    const Handle& atom) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<DiscoveredPattern> result;
    for (auto& p : _cached_patterns) {
        for (auto& inst : p.instances) {
            if (inst == atom) {
                result.push_back(p);
                break;
            }
        }
    }
    return result;
}

std::vector<DiscoveredPattern> PatternDiscovery::getPatternsByFrequency(
    size_t top_k) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto sorted = _cached_patterns;
    std::sort(sorted.begin(), sorted.end(),
              [](const DiscoveredPattern& a, const DiscoveredPattern& b) {
                  return a.frequency > b.frequency;
              });
    if (sorted.size() > top_k) sorted.resize(top_k);
    return sorted;
}

std::vector<DiscoveredPattern> PatternDiscovery::getPatternsBySurprisingness(
    size_t top_k) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto sorted = _cached_patterns;
    std::sort(sorted.begin(), sorted.end(),
              [](const DiscoveredPattern& a, const DiscoveredPattern& b) {
                  return a.surprisingness > b.surprisingness;
              });
    if (sorted.size() > top_k) sorted.resize(top_k);
    return sorted;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

std::string PatternDiscovery::getStatsSummary() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::ostringstream ss;
    ss << "[PatternDiscovery] episodes=" << _episodes.size()
       << " cached_patterns=" << _cached_patterns.size()
       << " initialized=" << _initialized;
    return ss.str();
}

bool PatternDiscovery::isHealthy() const
{
    return _initialized && (_atomspace != nullptr);
}
