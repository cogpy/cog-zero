/*
 * standalone/include/cog0/ActionExecutor.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Action execution framework for the standalone cog0 agent.
 * Adapted from agentzero-core/ActionExecutor without OpenCog dependencies.
 */
#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "AtomStore.h"

namespace cog0 {

// -----------------------------------------------------------------------
// ActionStatus — execution state of an action

enum class ActionStatus {
    PENDING,    // Created but not started
    EXECUTING,  // Currently running
    COMPLETED,  // Finished successfully
    FAILED,     // Failed to complete
    CANCELLED,  // Explicitly cancelled
    TIMEOUT     // Exceeded time limit
};

std::string actionStatusName(ActionStatus s);

// -----------------------------------------------------------------------
// ActionPriority

enum class ActionPriority { LOW = 1, MEDIUM = 5, HIGH = 10, CRITICAL = 20 };

// -----------------------------------------------------------------------
// ActionResult — outcome of an executed action

struct ActionResult {
    ActionStatus              status = ActionStatus::PENDING;
    std::string               message;
    Handle                    outcomeAtom;  // Optional atom representing result
    std::chrono::milliseconds duration{0};
    double                    successProbability = 0.0;

    bool succeeded() const { return status == ActionStatus::COMPLETED; }
};

// -----------------------------------------------------------------------
// ActionExecutor — registers and executes named actions

class ActionExecutor {
public:
    // Callback type: (actionName, params) -> ActionResult
    using ActionCallback = std::function<ActionResult(
        const std::string& actionName,
        const std::map<std::string, std::string>& params)>;

    explicit ActionExecutor(std::shared_ptr<AtomStore> store);
    ~ActionExecutor();

    // Register a handler for a named action type
    void registerAction(const std::string& actionType, ActionCallback callback);
    bool hasAction(const std::string& actionType) const;
    void unregisterAction(const std::string& actionType);

    // Execute an action synchronously; returns result immediately
    ActionResult executeSync(const std::string& actionType,
                             const std::map<std::string, std::string>& params = {},
                             int timeoutMs = 5000);

    // Enqueue an action for asynchronous execution; returns action id
    size_t enqueue(const std::string& actionType,
                   const std::map<std::string, std::string>& params = {},
                   ActionPriority priority = ActionPriority::MEDIUM);

    // Process up to maxActions from the queue; returns number processed
    size_t processQueue(size_t maxActions = 10);

    // Cancel a queued (not yet executing) action by id
    bool cancel(size_t actionId);

    // Query status / result of an action by id
    ActionStatus   getStatus(size_t actionId) const;
    ActionResult   getResult(size_t actionId) const;

    // Stats
    size_t pendingCount()   const;
    size_t completedCount() const;
    size_t failedCount()    const;

    std::string statusReport() const;

private:
    struct QueuedAction {
        size_t       id;
        std::string  actionType;
        std::map<std::string, std::string> params;
        ActionPriority priority;
        bool operator<(const QueuedAction& o) const {
            return static_cast<int>(priority) < static_cast<int>(o.priority);
        }
    };

    std::shared_ptr<AtomStore>                          _store;
    std::map<std::string, ActionCallback>               _registry;
    std::priority_queue<QueuedAction>                   _queue;
    std::map<size_t, ActionStatus>                      _statusMap;
    std::map<size_t, ActionResult>                      _resultMap;
    size_t _nextId      = 1;
    size_t _completed   = 0;
    size_t _failed      = 0;
};

} // namespace cog0
