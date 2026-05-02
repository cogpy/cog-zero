/*
 * standalone/src/EpisodicMemory.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <algorithm>
#include <sstream>

#include "cog0/EpisodicMemory.h"
#include "cog0/Logger.h"

namespace cog0 {

EpisodicMemory::EpisodicMemory(std::shared_ptr<AtomStore> store,
                                size_t maxEpisodes)
    : _store(std::move(store))
    , _maxEpisodes(maxEpisodes)
{
    logger().debug("[EpisodicMemory] Initialized (max=" +
                   std::to_string(maxEpisodes) + ")");
}

EpisodicMemory::~EpisodicMemory() = default;

size_t EpisodicMemory::record(const std::string& type,
                               const std::string& content,
                               double importance) {
    std::lock_guard<std::mutex> lock(_mutex);

    auto ep           = std::make_shared<Episode>();
    ep->id            = _nextId++;
    ep->type          = type;
    ep->content       = content;
    ep->importance    = importance;
    ep->timestamp     = std::chrono::steady_clock::now();
    ep->lastAccess    = ep->timestamp;

    // Create atom representation
    ep->atom = _store->addNode(AtomType::STATE,
                               "Episode:" + std::to_string(ep->id) + ":" + type);
    ep->atom->setTV(TruthValue{importance, 0.9});
    ep->atom->setSTI(importance);

    _episodes.push_back(ep);
    _byId[ep->id] = ep;

    trimToCapacity();
    return ep->id;
}

size_t EpisodicMemory::recordAtom(const Handle& atom,
                                   const std::string& type,
                                   double importance) {
    std::lock_guard<std::mutex> lock(_mutex);

    auto ep        = std::make_shared<Episode>();
    ep->id         = _nextId++;
    ep->type       = type;
    ep->content    = atom ? atom->toStr() : "";
    ep->atom       = atom;
    ep->importance = importance;
    ep->timestamp  = std::chrono::steady_clock::now();
    ep->lastAccess = ep->timestamp;

    _episodes.push_back(ep);
    _byId[ep->id] = ep;

    trimToCapacity();
    return ep->id;
}

bool EpisodicMemory::link(size_t id1, size_t id2) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it1 = _byId.find(id1);
    auto it2 = _byId.find(id2);
    if (it1 == _byId.end() || it2 == _byId.end()) return false;
    it1->second->relatedIds.push_back(id2);
    it2->second->relatedIds.push_back(id1);
    return true;
}

Episode::Ptr EpisodicMemory::recall(size_t id) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _byId.find(id);
    if (it == _byId.end()) return nullptr;
    it->second->recalled   = true;
    it->second->lastAccess = std::chrono::steady_clock::now();
    ++_recalledCount;
    return it->second;
}

std::vector<Episode::Ptr> EpisodicMemory::recentEpisodes(size_t n) const {
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<Episode::Ptr> result;
    auto it = _episodes.rbegin();
    while (it != _episodes.rend() && result.size() < n) {
        result.push_back(*it);
        ++it;
    }
    return result;
}

std::vector<Episode::Ptr> EpisodicMemory::episodesByType(
        const std::string& type) const {
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<Episode::Ptr> result;
    for (const auto& ep : _episodes)
        if (ep->type == type) result.push_back(ep);
    return result;
}

std::vector<Episode::Ptr> EpisodicMemory::importantEpisodes(
        double threshold) const {
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<Episode::Ptr> result;
    for (const auto& ep : _episodes)
        if (ep->importance >= threshold) result.push_back(ep);
    return result;
}

std::vector<Episode::Ptr> EpisodicMemory::search(
        const std::string& query) const {
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<Episode::Ptr> result;
    for (const auto& ep : _episodes)
        if (ep->content.find(query) != std::string::npos ||
            ep->type.find(query) != std::string::npos)
            result.push_back(ep);
    return result;
}

size_t EpisodicMemory::forget(double importanceThreshold) {
    std::lock_guard<std::mutex> lock(_mutex);
    size_t forgotten = 0;
    for (auto it = _episodes.begin(); it != _episodes.end(); ) {
        if ((*it)->importance < importanceThreshold && !(*it)->recalled) {
            _byId.erase((*it)->id);
            if ((*it)->atom) _store->remove((*it)->atom);
            it = _episodes.erase(it);
            ++forgotten;
        } else {
            ++it;
        }
    }
    if (forgotten > 0)
        logger().debug("[EpisodicMemory] Forgot " + std::to_string(forgotten) +
                       " low-importance episodes");
    return forgotten;
}

void EpisodicMemory::clear() {
    std::lock_guard<std::mutex> lock(_mutex);
    _episodes.clear();
    _byId.clear();
    _nextId = 1;
    _recalledCount = 0;
}

size_t EpisodicMemory::size() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _episodes.size();
}

size_t EpisodicMemory::recalledCount() const { return _recalledCount; }

std::string EpisodicMemory::statusReport() const {
    std::lock_guard<std::mutex> lock(_mutex);
    std::ostringstream oss;
    oss << "EpisodicMemory: episodes=" << _episodes.size()
        << "/" << _maxEpisodes
        << " recalled=" << _recalledCount << "\n";
    return oss.str();
}

void EpisodicMemory::trimToCapacity() {
    // Remove lowest-importance non-recalled episodes when over capacity.
    // Falls back to the oldest episode if all episodes have been recalled.
    while (_episodes.size() > _maxEpisodes) {
        auto victim = _episodes.end();
        for (auto it = _episodes.begin(); it != _episodes.end(); ++it) {
            if ((*it)->recalled) continue; // prefer not to evict recalled episodes
            if (victim == _episodes.end() ||
                (*it)->importance < (*victim)->importance)
                victim = it;
        }
        // All episodes recalled — fall back to oldest
        if (victim == _episodes.end()) victim = _episodes.begin();
        _byId.erase((*victim)->id);
        if ((*victim)->atom) _store->remove((*victim)->atom);
        _episodes.erase(victim);
    }
}

} // namespace cog0
