/*
 * standalone/src/ConflictResolver.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "cog0/ConflictResolver.h"
#include "cog0/Logger.h"

#include <algorithm>
#include <cassert>
#include <sstream>
#include <stdexcept>

namespace cog0 {

// =========================================================================
// Helpers
// =========================================================================

std::string resolutionStrategyName(ResolutionStrategy s)
{
    switch (s) {
        case ResolutionStrategy::STRICT_PRIORITY:   return "STRICT_PRIORITY";
        case ResolutionStrategy::FAIRNESS_WEIGHTED: return "FAIRNESS_WEIGHTED";
        case ResolutionStrategy::SLA_PRIORITY:      return "SLA_PRIORITY";
    }
    return "UNKNOWN";
}

// =========================================================================
// ConflictResolver
// =========================================================================

ConflictResolver::ConflictResolver(ResolutionStrategy strategy)
    : _strategy(strategy)
{}

// -------------------------------------------------------------------------
// Configuration
// -------------------------------------------------------------------------

void ConflictResolver::setStrategy(ResolutionStrategy s)
{
    std::lock_guard<std::mutex> lk(_mu);
    _strategy = s;
}

// -------------------------------------------------------------------------
// Queue management
// -------------------------------------------------------------------------

ConflictTicket ConflictResolver::submit(
    const std::string& taskId,
    int priority,
    std::vector<std::string> resources,
    double slaSeconds)
{
    ConflictTicket t;
    t.id          = taskId;
    t.priority    = priority;
    t.slaSeconds  = slaSeconds;
    t.submitTime  = std::chrono::steady_clock::now();
    t.resources   = std::move(resources);

    {
        std::lock_guard<std::mutex> lk(_mu);
        _queue.push_back(t);
    }

    logger().debug("ConflictResolver: submitted ticket id=" + taskId +
                   " priority=" + std::to_string(priority));
    return t;
}

bool ConflictResolver::cancel(const std::string& taskId)
{
    std::lock_guard<std::mutex> lk(_mu);
    auto it = std::find_if(_queue.begin(), _queue.end(),
                           [&taskId](const ConflictTicket& t) {
                               return t.id == taskId;
                           });
    if (it == _queue.end()) return false;
    _queue.erase(it);
    return true;
}

size_t ConflictResolver::pending() const
{
    std::lock_guard<std::mutex> lk(_mu);
    return _queue.size();
}

void ConflictResolver::clear()
{
    std::lock_guard<std::mutex> lk(_mu);
    _queue.clear();
}

// -------------------------------------------------------------------------
// Effective priority computation
// -------------------------------------------------------------------------

double ConflictResolver::_effectivePriority(const ConflictTicket& t) const
{
    // Must hold _mu
    using namespace std::chrono;
    auto now     = steady_clock::now();
    double waitS = duration_cast<microseconds>(now - t.submitTime).count()
                   / 1e6;

    switch (_strategy) {
        case ResolutionStrategy::STRICT_PRIORITY:
            return static_cast<double>(t.priority);

        case ResolutionStrategy::FAIRNESS_WEIGHTED:
            // Boost by waiting time to prevent starvation
            return static_cast<double>(t.priority) + waitS * _fairnessWeight;

        case ResolutionStrategy::SLA_PRIORITY: {
            if (t.slaSeconds <= 0.0) {
                // No SLA — use nominal priority, slight penalty for being late
                return static_cast<double>(t.priority);
            }
            double timeRemaining = t.slaSeconds - waitS;
            if (timeRemaining <= 0.0) {
                // Past deadline — maximum urgency
                return static_cast<double>(t.priority) + 1e6;
            }
            // Boost = inverse of remaining time (EDF-inspired)
            // Normalised so that 1s remaining == priority + 100
            return static_cast<double>(t.priority) + 100.0 / timeRemaining;
        }
    }
    return static_cast<double>(t.priority);
}

// -------------------------------------------------------------------------
// Resource conflict check
// -------------------------------------------------------------------------

/*static*/ bool ConflictResolver::_conflicts(const ConflictTicket& a,
                                              const ConflictTicket& b)
{
    for (const auto& ra : a.resources)
        for (const auto& rb : b.resources)
            if (ra == rb) return true;
    return false;
}

// -------------------------------------------------------------------------
// Resolution
// -------------------------------------------------------------------------

ConflictResult ConflictResolver::resolve()
{
    std::lock_guard<std::mutex> lk(_mu);

    ConflictResult result;
    result.resolved = false;

    if (_queue.empty()) return result;

    // Compute effective priorities
    for (auto& t : _queue)
        t.effectivePriority = _effectivePriority(t);

    // Find ticket with highest effective priority
    auto bestIt = std::max_element(
        _queue.begin(), _queue.end(),
        [](const ConflictTicket& a, const ConflictTicket& b) {
            return a.effectivePriority < b.effectivePriority;
        });

    result.winner   = *bestIt;
    result.resolved = true;

    // Identify conflicting tickets
    for (auto it = _queue.begin(); it != _queue.end(); ++it) {
        if (it == bestIt) continue;
        if (_conflicts(*bestIt, *it)) {
            std::ostringstream oss;
            oss << "ticket " << it->id
                << " conflicts with winner " << bestIt->id
                << " on shared resource";
            result.reasons.push_back(oss.str());
            result.deferred.push_back(*it);
        }
    }

    // Remove winner from queue
    _queue.erase(bestIt);

    logger().debug("ConflictResolver: resolved winner=" + result.winner.id +
                   " strategy=" + resolutionStrategyName(_strategy));

    return result;
}

std::vector<ConflictTicket> ConflictResolver::ranked() const
{
    std::lock_guard<std::mutex> lk(_mu);

    std::vector<ConflictTicket> copy = _queue;
    for (auto& t : copy)
        t.effectivePriority = _effectivePriority(t);

    std::sort(copy.begin(), copy.end(),
              [](const ConflictTicket& a, const ConflictTicket& b) {
                  return a.effectivePriority > b.effectivePriority;
              });
    return copy;
}

} // namespace cog0
