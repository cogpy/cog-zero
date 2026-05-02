/*
 * standalone/src/ActionExecutor.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <chrono>
#include <sstream>
#include <stdexcept>

#include "cog0/ActionExecutor.h"
#include "cog0/Logger.h"

namespace cog0 {

std::string actionStatusName(ActionStatus s) {
    switch (s) {
        case ActionStatus::PENDING:   return "PENDING";
        case ActionStatus::EXECUTING: return "EXECUTING";
        case ActionStatus::COMPLETED: return "COMPLETED";
        case ActionStatus::FAILED:    return "FAILED";
        case ActionStatus::CANCELLED: return "CANCELLED";
        case ActionStatus::TIMEOUT:   return "TIMEOUT";
    }
    return "UNKNOWN";
}

ActionExecutor::ActionExecutor(std::shared_ptr<AtomStore> store)
    : _store(std::move(store))
{
    logger().debug("[ActionExecutor] Initialized");
}

ActionExecutor::~ActionExecutor() = default;

void ActionExecutor::registerAction(const std::string& actionType,
                                    ActionCallback callback) {
    _registry[actionType] = std::move(callback);
    logger().debug("[ActionExecutor] Registered action type: " + actionType);
}

bool ActionExecutor::hasAction(const std::string& actionType) const {
    return _registry.count(actionType) > 0;
}

void ActionExecutor::unregisterAction(const std::string& actionType) {
    _registry.erase(actionType);
}

ActionResult ActionExecutor::executeSync(const std::string& actionType,
                                          const std::map<std::string, std::string>& params,
                                          int timeoutMs) {
    auto it = _registry.find(actionType);
    if (it == _registry.end()) {
        ActionResult r;
        r.status  = ActionStatus::FAILED;
        r.message = "Unknown action type: " + actionType;
        logger().warn("[ActionExecutor] " + r.message);
        return r;
    }

    auto start = std::chrono::steady_clock::now();
    ActionResult result;
    result.status = ActionStatus::EXECUTING;

    try {
        result = it->second(actionType, params);
    } catch (const std::exception& e) {
        result.status  = ActionStatus::FAILED;
        result.message = std::string("Exception: ") + e.what();
        logger().error("[ActionExecutor] Action '" + actionType + "' threw: " + e.what());
    }

    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Check timeout
    if (timeoutMs > 0 &&
        result.duration > std::chrono::milliseconds(timeoutMs) &&
        result.status != ActionStatus::COMPLETED) {
        result.status  = ActionStatus::TIMEOUT;
        result.message = "Action timed out after " + std::to_string(timeoutMs) + "ms";
    }

    return result;
}

size_t ActionExecutor::enqueue(const std::string& actionType,
                                const std::map<std::string, std::string>& params,
                                ActionPriority priority) {
    size_t id = _nextId++;
    _queue.push(QueuedAction{id, actionType, params, priority});
    _statusMap[id] = ActionStatus::PENDING;
    return id;
}

size_t ActionExecutor::processQueue(size_t maxActions) {
    size_t processed = 0;
    while (!_queue.empty() && processed < maxActions) {
        QueuedAction qa = _queue.top();
        _queue.pop();

        // Check if cancelled
        auto it = _statusMap.find(qa.id);
        if (it != _statusMap.end() && it->second == ActionStatus::CANCELLED) {
            continue;
        }

        _statusMap[qa.id] = ActionStatus::EXECUTING;
        ActionResult result = executeSync(qa.actionType, qa.params);
        _statusMap[qa.id]  = result.status;
        _resultMap[qa.id]  = result;

        if (result.status == ActionStatus::COMPLETED) ++_completed;
        else if (result.status == ActionStatus::FAILED) ++_failed;

        ++processed;
    }
    return processed;
}

bool ActionExecutor::cancel(size_t actionId) {
    auto it = _statusMap.find(actionId);
    if (it == _statusMap.end() || it->second != ActionStatus::PENDING) return false;
    it->second = ActionStatus::CANCELLED;
    return true;
}

ActionStatus ActionExecutor::getStatus(size_t actionId) const {
    auto it = _statusMap.find(actionId);
    if (it == _statusMap.end()) return ActionStatus::FAILED;
    return it->second;
}

ActionResult ActionExecutor::getResult(size_t actionId) const {
    auto it = _resultMap.find(actionId);
    if (it == _resultMap.end()) {
        ActionResult r;
        r.status  = ActionStatus::PENDING;
        r.message = "Action not yet executed";
        return r;
    }
    return it->second;
}

size_t ActionExecutor::pendingCount() const {
    return _queue.size();
}

size_t ActionExecutor::completedCount() const { return _completed; }
size_t ActionExecutor::failedCount()    const { return _failed; }

std::string ActionExecutor::statusReport() const {
    std::ostringstream oss;
    oss << "ActionExecutor: registered=" << _registry.size()
        << " pending=" << _queue.size()
        << " completed=" << _completed
        << " failed=" << _failed << "\n";
    return oss.str();
}

} // namespace cog0
