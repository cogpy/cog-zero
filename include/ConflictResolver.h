/*
 * standalone/include/cog0/ConflictResolver.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Priority-based conflict resolution for cog0 task scheduling.
 *
 * Supports three resolution strategies:
 *
 *   STRICT_PRIORITY    — highest-priority task always wins; lower-priority
 *                        tasks are preempted or deferred.
 *
 *   FAIRNESS_WEIGHTED  — priority is tempered by a fairness score that
 *                        increases over time as tasks wait; prevents
 *                        starvation of low-priority work.
 *
 *   SLA_PRIORITY       — tasks carry an SLA deadline; tasks that are close
 *                        to missing their deadline are boosted regardless of
 *                        their nominal priority (EDF-inspired).
 *
 * Usage
 * ─────
 *   ConflictResolver cr;
 *   cr.setStrategy(ResolutionStrategy::FAIRNESS_WEIGHTED);
 *   auto ticket = cr.submit("task-A", 5, resource_set);
 *   auto winner = cr.resolve();   // returns the ticket with highest effective prio
 */

#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace cog0 {

// =========================================================================
// Resolution strategy
// =========================================================================

enum class ResolutionStrategy {
    STRICT_PRIORITY,    ///< Highest nominal priority always wins
    FAIRNESS_WEIGHTED,  ///< Anti-starvation: waiting time boosts effective prio
    SLA_PRIORITY        ///< Earliest-deadline-first with priority tie-break
};

std::string resolutionStrategyName(ResolutionStrategy s);

// =========================================================================
// ConflictTicket — a pending task waiting for resource access
// =========================================================================

struct ConflictTicket {
    std::string id;          ///< Unique task / request identifier
    int         priority;    ///< Nominal priority (higher = more urgent)
    double      slaSeconds;  ///< SLA deadline from submission (0 = none)

    std::chrono::steady_clock::time_point submitTime;  ///< When ticket was submitted
    std::vector<std::string> resources;  ///< Resources this ticket needs

    // Derived / internal
    double effectivePriority = 0.0;  ///< Computed by the resolver
};

// =========================================================================
// ConflictResult — outcome of a resolution call
// =========================================================================

struct ConflictResult {
    bool                           resolved;   ///< false if queue was empty
    ConflictTicket                 winner;     ///< Winning ticket
    std::vector<ConflictTicket>    deferred;   ///< Tickets that must wait
    std::vector<std::string>       reasons;    ///< Human-readable rationale
};

// =========================================================================
// ConflictResolver
// =========================================================================

class ConflictResolver {
public:
    explicit ConflictResolver(
        ResolutionStrategy strategy = ResolutionStrategy::STRICT_PRIORITY);

    // ----------------------------------------------------------------
    // Configuration

    void setStrategy(ResolutionStrategy s);
    ResolutionStrategy strategy() const { return _strategy; }

    /// Fairness weight: effective_priority += waiting_seconds * fairnessWeight.
    /// Only used in FAIRNESS_WEIGHTED mode.
    void setFairnessWeight(double w) { _fairnessWeight = w; }
    double fairnessWeight() const    { return _fairnessWeight; }

    // ----------------------------------------------------------------
    // Queue management

    /// Submit a ticket for conflict resolution.
    ConflictTicket submit(const std::string& taskId,
                          int priority,
                          std::vector<std::string> resources,
                          double slaSeconds = 0.0);

    /// Remove a ticket (e.g. task was cancelled).
    bool cancel(const std::string& taskId);

    /// How many tickets are pending.
    size_t pending() const;

    /// Clear all pending tickets.
    void clear();

    // ----------------------------------------------------------------
    // Resolution

    /// Pick the winning ticket according to the current strategy.
    /// The winning ticket is *removed* from the pending queue.
    ConflictResult resolve();

    /// Return all pending tickets sorted by effective priority (no removal).
    std::vector<ConflictTicket> ranked() const;

private:
    ResolutionStrategy _strategy;
    double             _fairnessWeight = 1.0;  // priority units per second of wait

    mutable std::mutex          _mu;
    std::vector<ConflictTicket> _queue;

    // Compute effective priority for a ticket (must hold _mu)
    double _effectivePriority(const ConflictTicket& t) const;

    // Check whether two tickets conflict on resources
    static bool _conflicts(const ConflictTicket& a, const ConflictTicket& b);
};

} // namespace cog0
