/*
 * standalone/src/ActionScheduler.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <algorithm>
#include <sstream>

#include "cog0/ActionScheduler.h"
#include "cog0/Logger.h"

namespace cog0 {

std::string scheduleResultName(ScheduleResult r) {
    switch (r) {
        case ScheduleResult::SCHEDULED:        return "SCHEDULED";
        case ScheduleResult::CONFLICT:         return "CONFLICT";
        case ScheduleResult::DEPENDENCY_UNMET: return "DEPENDENCY_UNMET";
        case ScheduleResult::INVALID:          return "INVALID";
        case ScheduleResult::QUEUE_FULL:       return "QUEUE_FULL";
    }
    return "UNKNOWN";
}

ActionScheduler::ActionScheduler(std::shared_ptr<AtomStore>      store,
                                  std::shared_ptr<ActionExecutor>  executor)
    : _store(std::move(store))
    , _executor(std::move(executor))
{
    logger().debug("[ActionScheduler] Initialized");
}

ActionScheduler::~ActionScheduler() = default;

size_t ActionScheduler::scheduleAt(
        const std::string& actionType,
        const std::map<std::string, std::string>& params,
        std::chrono::steady_clock::time_point when,
        int priority) {
    if (_schedule.size() >= _maxScheduled) return 0;
    if (!_executor->hasAction(actionType)) return 0;

    ScheduledAction sa;
    sa.id            = _nextId++;
    sa.actionType    = actionType;
    sa.params        = params;
    sa.scheduledTime = when;
    sa.priority      = priority;
    _schedule[sa.id] = sa;
    return sa.id;
}

size_t ActionScheduler::scheduleAfter(
        const std::string& actionType,
        const std::map<std::string, std::string>& params,
        std::chrono::milliseconds delay,
        int priority) {
    return scheduleAt(actionType, params,
                      std::chrono::steady_clock::now() + delay, priority);
}

size_t ActionScheduler::scheduleBefore(
        const std::string& actionType,
        const std::map<std::string, std::string>& params,
        std::chrono::steady_clock::time_point deadline,
        int priority) {
    if (_schedule.size() >= _maxScheduled) return 0;
    if (!_executor->hasAction(actionType)) return 0;

    ScheduledAction sa;
    sa.id            = _nextId++;
    sa.actionType    = actionType;
    sa.params        = params;
    sa.scheduledTime = std::chrono::steady_clock::now(); // due immediately on next tick()
    sa.deadline      = deadline;
    sa.priority      = priority;
    _schedule[sa.id] = sa;
    return sa.id;
}

size_t ActionScheduler::schedulePeriodic(
        const std::string& actionType,
        const std::map<std::string, std::string>& params,
        std::chrono::milliseconds period,
        int priority) {
    if (_schedule.size() >= _maxScheduled) return 0;
    if (!_executor->hasAction(actionType)) return 0;
    if (period.count() <= 0) return 0;

    ScheduledAction sa;
    sa.id            = _nextId++;
    sa.actionType    = actionType;
    sa.params        = params;
    sa.scheduledTime = std::chrono::steady_clock::now();
    sa.periodic      = true;
    sa.period        = period;
    sa.priority      = priority;
    _schedule[sa.id] = sa;
    return sa.id;
}

bool ActionScheduler::addDependency(size_t dependentId, size_t prerequisiteId) {
    auto it = _schedule.find(dependentId);
    if (it == _schedule.end()) return false;
    it->second.dependencies.push_back(prerequisiteId);
    return true;
}

bool ActionScheduler::acquireResource(const std::string& resource, size_t actionId) {
    if (_resourceLocks.count(resource)) return false;
    _resourceLocks[resource] = actionId;
    return true;
}

void ActionScheduler::releaseResource(const std::string& resource, size_t actionId) {
    auto it = _resourceLocks.find(resource);
    if (it != _resourceLocks.end() && it->second == actionId)
        _resourceLocks.erase(it);
}

bool ActionScheduler::areDependenciesMet(const ScheduledAction& sa) const {
    for (size_t dep : sa.dependencies) {
        auto it = _completedActions.find(dep);
        if (it == _completedActions.end()) return false;
        if (it->second != ActionStatus::COMPLETED) return false;
    }
    return true;
}

bool ActionScheduler::areResourcesAvailable(const ScheduledAction& sa) const {
    for (const auto& res : sa.requiredResources) {
        auto it = _resourceLocks.find(res);
        if (it != _resourceLocks.end() && it->second != sa.id)
            return false;
    }
    return true;
}

size_t ActionScheduler::tick() {
    size_t executed = 0;
    std::vector<size_t> toRemove;

    // Collect due actions sorted by priority (higher = first)
    std::vector<ScheduledAction*> due;
    for (auto& [id, sa] : _schedule) {
        if (sa.isDue()) due.push_back(&sa);
    }
    std::sort(due.begin(), due.end(),
              [](const ScheduledAction* a, const ScheduledAction* b) {
                  return a->priority > b->priority;
              });

    for (ScheduledAction* sa : due) {
        // Skip if past deadline
        if (sa->isPastDeadline()) {
            logger().warn("[ActionScheduler] Action " + sa->actionType +
                          " missed its deadline");
            _completedActions[sa->id] = ActionStatus::FAILED;
            toRemove.push_back(sa->id);
            continue;
        }

        if (!areDependenciesMet(*sa) || !areResourcesAvailable(*sa)) continue;

        // Acquire resources
        for (const auto& res : sa->requiredResources) acquireResource(res, sa->id);

        ActionResult result = _executor->executeSync(sa->actionType, sa->params);
        _completedActions[sa->id] = result.status;
        ++executed;

        // Release resources
        for (const auto& res : sa->requiredResources) releaseResource(res, sa->id);

        if (sa->periodic && result.status == ActionStatus::COMPLETED) {
            // Reschedule
            sa->scheduledTime = std::chrono::steady_clock::now() + sa->period;
        } else {
            toRemove.push_back(sa->id);
        }
    }

    for (size_t id : toRemove) _schedule.erase(id);
    return executed;
}

bool ActionScheduler::cancel(size_t actionId) {
    auto it = _schedule.find(actionId);
    if (it == _schedule.end()) return false;
    _completedActions[actionId] = ActionStatus::CANCELLED;
    _schedule.erase(it);
    return true;
}

bool ActionScheduler::isPending(size_t actionId) const {
    return _schedule.count(actionId) > 0;
}

size_t ActionScheduler::pendingCount() const { return _schedule.size(); }

std::vector<ScheduledAction> ActionScheduler::pendingActions() const {
    std::vector<ScheduledAction> out;
    out.reserve(_schedule.size());
    for (const auto& [id, sa] : _schedule) out.push_back(sa);
    return out;
}

std::string ActionScheduler::statusReport() const {
    std::ostringstream oss;
    oss << "ActionScheduler: pending=" << _schedule.size()
        << " completed=" << _completedActions.size()
        << " resourceLocks=" << _resourceLocks.size() << "\n";
    return oss.str();
}

} // namespace cog0
