/*
 * src/AttentionManager.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * AttentionManager Implementation
 * ECAN-based attention allocation for incoming percepts
 * Part of the AGENT-ZERO-GENESIS project - Phase 2 Perception
 */

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <numeric>
#include <stdexcept>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/AttentionManager.h"

using namespace opencog;
using namespace opencog::agentzero;

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

AttentionManager::AttentionManager(AtomSpacePtr atomspace,
                                   AttentionConfig config)
    : _atomspace(atomspace)
    , _config(config)
{
    if (!_atomspace) {
        throw std::invalid_argument("[AttentionManager] AtomSpace cannot be null");
    }
    logger().info() << "[AttentionManager] Initialized (base_sti="
                    << _config.base_sti
                    << ", decay_rate=" << _config.decay_rate
                    << ", focus_boundary=" << _config.focus_boundary << ")";
}

AttentionManager::~AttentionManager()
{
    logger().info() << "[AttentionManager] Destroyed after "
                    << _allocations.load() << " allocations, "
                    << _decay_cycles.load() << " decay cycles";
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

double AttentionManager::clampSTI(double sti) const
{
    return std::max(_config.min_sti, std::min(_config.max_sti, sti));
}

void AttentionManager::evictFromFocus(
    std::vector<std::pair<Handle, double>>& focus_list)
{
    // Sort descending by STI
    std::sort(focus_list.begin(), focus_list.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Trim to max_focus_size
    if (focus_list.size() > _config.max_focus_size) {
        size_t evicted = focus_list.size() - _config.max_focus_size;
        focus_list.resize(_config.max_focus_size);
        _focus_evictions += evicted;
    }
}

// ---------------------------------------------------------------------------
// Attention allocation
// ---------------------------------------------------------------------------

double AttentionManager::allocateAttention(const Handle& percept, double salience)
{
    if (percept == Handle::UNDEFINED) {
        logger().warn() << "[AttentionManager] allocateAttention: undefined handle";
        return 0.0;
    }

    salience = std::max(0.0, std::min(1.0, salience));

    // Base STI + salience boost + optional novelty boost
    double sti = _config.base_sti + salience * (1.0 - _config.base_sti);
    sti = clampSTI(sti);

    {
        std::lock_guard<std::mutex> lock(_sti_mutex);
        _sti_map[percept] = sti;
    }

    // Persist as an atom value so downstream components can read it
    Handle sti_key = _atomspace->add_node(PREDICATE_NODE, "attention:sti");
    TruthValuePtr tv = SimpleTruthValue::createTV(sti, salience);
    percept->setTruthValue(tv);

    _allocations++;
    logger().debug() << "[AttentionManager] Allocated STI=" << sti
                     << " salience=" << salience;
    return sti;
}

void AttentionManager::updateAttention(const Handle& percept, double new_sti)
{
    if (percept == Handle::UNDEFINED) return;
    new_sti = clampSTI(new_sti);
    {
        std::lock_guard<std::mutex> lock(_sti_mutex);
        _sti_map[percept] = new_sti;
    }
    TruthValuePtr tv = SimpleTruthValue::createTV(new_sti, 0.9);
    percept->setTruthValue(tv);
}

void AttentionManager::decayAttention()
{
    std::lock_guard<std::mutex> lock(_sti_mutex);

    std::vector<Handle> to_remove;
    for (auto& [handle, sti] : _sti_map) {
        sti *= (1.0 - _config.decay_rate);
        if (sti <= _config.min_sti + 1e-9) {
            to_remove.push_back(handle);
        }
    }
    for (const auto& h : to_remove) {
        _sti_map.erase(h);
    }

    _decay_cycles++;
    logger().debug() << "[AttentionManager] Decay cycle " << _decay_cycles.load()
                     << ": " << to_remove.size() << " atoms removed from tracking";
}

// ---------------------------------------------------------------------------
// Attention focus
// ---------------------------------------------------------------------------

HandleSeq AttentionManager::getAttentionFocus() const
{
    std::vector<std::pair<Handle, double>> focus_list;

    {
        std::lock_guard<std::mutex> lock(_sti_mutex);
        for (const auto& [handle, sti] : _sti_map) {
            if (sti >= _config.focus_boundary) {
                focus_list.emplace_back(handle, sti);
            }
        }
    }

    // Sort descending by STI, cap at max_focus_size
    std::sort(focus_list.begin(), focus_list.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    if (focus_list.size() > _config.max_focus_size) {
        focus_list.resize(_config.max_focus_size);
    }

    HandleSeq result;
    result.reserve(focus_list.size());
    for (const auto& [h, sti_val] : focus_list) {
        (void)sti_val; // already sorted; only handle needed
        result.push_back(h);
    }
    return result;
}

bool AttentionManager::isInAttentionFocus(const Handle& atom) const
{
    return getSTI(atom) >= _config.focus_boundary;
}

double AttentionManager::getSTI(const Handle& atom) const
{
    std::lock_guard<std::mutex> lock(_sti_mutex);
    auto it = _sti_map.find(atom);
    return (it != _sti_map.end()) ? it->second : 0.0;
}

// ---------------------------------------------------------------------------
// Salience scoring
// ---------------------------------------------------------------------------

SalienceScore AttentionManager::calculateSalience(const SensoryInput& input)
{
    // Signal quality: use input confidence directly
    double sig = std::max(0.0, std::min(1.0, input.confidence));

    // Novelty: based on how often this modality has been seen
    std::string mod_key = input.sensor_type + ":" + input.modality;
    double novelty = 1.0;
    {
        std::lock_guard<std::mutex> lock(_novelty_mutex);
        auto& count = _seen_modalities[mod_key];
        // Novelty decays with log of frequency
        novelty = 1.0 / (1.0 + std::log1p(static_cast<double>(count)));
        count++;
    }

    // Relevance: uniform baseline (extended by subclasses if needed)
    double relevance = 0.5;

    // Overall: weighted average
    double overall = 0.5 * sig + 0.3 * novelty + 0.2 * relevance;
    overall = std::max(0.0, std::min(1.0, overall));

    return SalienceScore{sig, novelty, relevance, overall};
}

// ---------------------------------------------------------------------------
// Attention spreading
// ---------------------------------------------------------------------------

void AttentionManager::spreadAttention(const Handle& source, double amount)
{
    if (source == Handle::UNDEFINED) return;

    double source_sti = getSTI(source);
    if (source_sti <= _config.min_sti) return;

    double spread = (amount > 0.0) ? amount : _config.spread_factor * source_sti;

    // Neighbours: outgoing set of the source atom
    const HandleSeq& outgoing = source->getOutgoingSet();
    if (outgoing.empty()) return;

    double per_neighbour = spread / static_cast<double>(outgoing.size());

    // Reduce source STI
    {
        std::lock_guard<std::mutex> lock(_sti_mutex);
        auto it = _sti_map.find(source);
        if (it != _sti_map.end()) {
            it->second = clampSTI(it->second - spread);
        }
    }

    // Boost each neighbour
    for (const Handle& neighbour : outgoing) {
        if (neighbour == Handle::UNDEFINED) continue;
        std::lock_guard<std::mutex> lock(_sti_mutex);
        auto& n_sti = _sti_map[neighbour];
        n_sti = clampSTI(n_sti + per_neighbour);
    }

    logger().debug() << "[AttentionManager] Spread " << spread
                     << " STI from source to " << outgoing.size() << " neighbours";
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

std::string AttentionManager::getStats() const
{
    size_t tracked;
    {
        std::lock_guard<std::mutex> lock(_sti_mutex);
        tracked = _sti_map.size();
    }
    HandleSeq focus = getAttentionFocus();

    std::ostringstream ss;
    ss << "{";
    ss << "\"allocations\":" << _allocations.load() << ",";
    ss << "\"decay_cycles\":" << _decay_cycles.load() << ",";
    ss << "\"focus_evictions\":" << _focus_evictions.load() << ",";
    ss << "\"tracked_atoms\":" << tracked << ",";
    ss << "\"focus_size\":" << focus.size() << ",";
    ss << "\"focus_boundary\":" << std::fixed << std::setprecision(4)
       << _config.focus_boundary << ",";
    ss << "\"base_sti\":" << _config.base_sti << ",";
    ss << "\"decay_rate\":" << _config.decay_rate;
    ss << "}";
    return ss.str();
}

void AttentionManager::reset()
{
    {
        std::lock_guard<std::mutex> lock(_sti_mutex);
        _sti_map.clear();
    }
    {
        std::lock_guard<std::mutex> lock(_novelty_mutex);
        _seen_modalities.clear();
    }
    _allocations = 0;
    _decay_cycles = 0;
    _focus_evictions = 0;
    logger().info() << "[AttentionManager] Reset";
}

size_t AttentionManager::trackedAtomCount() const
{
    std::lock_guard<std::mutex> lock(_sti_mutex);
    return _sti_map.size();
}
