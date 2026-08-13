/*
 * opencog/agentzero/ActionExecutor.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Action execution framework with status tracking via AtomSpace.
 * Part of AGENT-ZERO-GENESIS Phase 1.
 */
#ifndef _OPENCOG_AGENTZERO_ACTION_EXECUTOR_H
#define _OPENCOG_AGENTZERO_ACTION_EXECUTOR_H

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

namespace opencog {
namespace agentzero {

class AgentZeroCore;
class ActionScheduler;

class ActionExecutor {
public:
    enum class ActionStatus {
        PENDING,
        RUNNING,
        COMPLETED,
        FAILED,
        CANCELLED,
        TIMEOUT
    };

    enum class Priority {
        LOW = 1,
        MEDIUM = 5,
        HIGH = 10,
        CRITICAL = 20
    };

    struct ActionResult {
        ActionStatus status = ActionStatus::PENDING;
        std::string message;
        double success_probability = 0.0;
        std::chrono::milliseconds duration{0};
        Handle result_atom = Handle::UNDEFINED;
    };

    using ActionCallback = std::function<ActionResult(const Handle&,
                                                      const std::map<std::string, Handle>&)>;

    ActionExecutor(AgentZeroCore* agent_core, AtomSpacePtr atomspace);
    ~ActionExecutor();

    void setScheduler(std::shared_ptr<ActionScheduler> scheduler);

    bool executeAction(const Handle& action_atom, Priority priority = Priority::MEDIUM);
    ActionResult executeActionSync(const Handle& action_atom, int timeout_ms = 5000);

    Handle executeSimpleAction(const std::string& action_name,
                               const std::map<std::string, Handle>& params = {});

    bool registerAction(const std::string& name, ActionCallback callback);
    bool isActionRegistered(const std::string& name) const;

    bool cancelAction(const Handle& action_atom);
    ActionStatus getActionStatus(const Handle& action_atom) const;
    ActionResult getActionResult(const Handle& action_atom) const;

    std::vector<Handle> getPendingActions() const;
    std::vector<Handle> getExecutingActions() const;

    int processActionQueue();
    int monitorExecutingActions();

    void setMaxConcurrentActions(size_t n) { _max_concurrent = n; }
    void setDefaultTimeout(int ms) { _default_timeout_ms = ms; }

    std::string getStatusInfo() const;

private:
    void updateStatusAtom(const Handle& action_atom, ActionStatus status);
    ActionResult runDefaultAction(const Handle& action_atom,
                                  const std::map<std::string, Handle>& params);

    AgentZeroCore* _agent_core;
    AtomSpacePtr _atomspace;
    std::shared_ptr<ActionScheduler> _scheduler;

    mutable std::mutex _mu;
    std::map<std::string, ActionCallback> _registry;
    std::vector<Handle> _pending;
    std::vector<Handle> _executing;
    std::map<Handle, ActionResult> _results;
    std::map<Handle, Priority> _priorities;
    std::map<Handle, std::chrono::steady_clock::time_point> _start_times;

    size_t _max_concurrent = 4;
    int _default_timeout_ms = 5000;
    Handle _actions_root = Handle::UNDEFINED;
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_ACTION_EXECUTOR_H
