/*
 * standalone/include/cog0/ActionScheduler.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Temporal action scheduling for the standalone cog0 agent.
 * Adapted from agentzero-core/ActionScheduler without OpenCog dependencies.
 */
#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "AtomStore.h"
#include "ActionExecutor.h"

namespace cog0 {

// -----------------------------------------------------------------------
// ScheduleResult — outcome of scheduling an action

enum class ScheduleResult {
    SCHEDULED,           // Successfully added to schedule
    CONFLICT,            // Resource conflict
    DEPENDENCY_UNMET,    // Dependencies not satisfied
    INVALID,             // Invalid parameters
    QUEUE_FULL           // Schedule at capacity
};

std::string scheduleResultName(ScheduleResult r);

// -----------------------------------------------------------------------
// ScheduledAction — entry in the temporal schedule

struct ScheduledAction {
    size_t      id;
    std::string actionType;
    std::map<std::string, std::string> params;

    std::chrono::steady_clock::time_point scheduledTime;
    std::chrono::steady_clock::time_point deadline;

    std::vector<size_t>      dependencies;   // ids of actions that must complete first
    std::vector<std::string> requiredResources;
    int  priority  = 5;
    bool periodic  = false;
    std::chrono::milliseconds period{0};
    int  repeatCount = 1;

    bool isDue() const {
        return std::chrono::steady_clock::now() >= scheduledTime;
    }
    bool isPastDeadline() const {
        return deadline.time_since_epoch().count() > 0 &&
               std::chrono::steady_clock::now() > deadline;
    }
};

// -----------------------------------------------------------------------
// ActionScheduler — temporal scheduling with dependency management

class ActionScheduler {
public:
    ActionScheduler(std::shared_ptr<AtomStore>      store,
                    std::shared_ptr<ActionExecutor>  executor);
    ~ActionScheduler();

    // Schedule at an absolute time; returns action ID, or 0 on failure.
    size_t scheduleAt(const std::string& actionType,
                      const std::map<std::string, std::string>& params,
                      std::chrono::steady_clock::time_point when,
                      int priority = 5);

    // Schedule after a delay; returns action ID, or 0 on failure.
    size_t scheduleAfter(const std::string& actionType,
                         const std::map<std::string, std::string>& params,
                         std::chrono::milliseconds delay,
                         int priority = 5);

    // Schedule with a hard deadline; returns action ID, or 0 on failure.
    size_t scheduleBefore(const std::string& actionType,
                          const std::map<std::string, std::string>& params,
                          std::chrono::steady_clock::time_point deadline,
                          int priority = 5);

    // Schedule a periodic action; returns action ID, or 0 on failure.
    size_t schedulePeriodic(const std::string& actionType,
                             const std::map<std::string, std::string>& params,
                             std::chrono::milliseconds period,
                             int priority = 5);

    // Add dependencies (actionId must complete before dependentId can run)
    bool addDependency(size_t dependentId, size_t prerequisiteId);

    // Acquire/release named resources
    bool acquireResource(const std::string& resource, size_t actionId);
    void releaseResource(const std::string& resource, size_t actionId);

    // Process due actions — called from the cognitive loop each cycle
    // Returns number of actions executed
    size_t tick();

    // Cancel a scheduled action
    bool cancel(size_t actionId);

    // Query
    bool isPending(size_t actionId) const;
    size_t pendingCount() const;
    std::vector<ScheduledAction> pendingActions() const;

    std::string statusReport() const;

    // Configure maximum scheduled actions
    void setMaxScheduled(size_t n) { _maxScheduled = n; }

private:
    bool areDependenciesMet(const ScheduledAction& sa) const;
    bool areResourcesAvailable(const ScheduledAction& sa) const;

    std::shared_ptr<AtomStore>     _store;
    std::shared_ptr<ActionExecutor> _executor;

    std::map<size_t, ScheduledAction>      _schedule;
    std::map<std::string, size_t>          _resourceLocks; // resource → holder id
    std::map<size_t, ActionStatus>         _completedActions; // completed id → status

    size_t _nextId      = 1;
    size_t _maxScheduled = 1000;
};

} // namespace cog0
