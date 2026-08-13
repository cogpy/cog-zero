/*
 * opencog/agentzero/ActionScheduler.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Temporal action coordination with AtomSpace state tracking.
 * Part of AGENT-ZERO-GENESIS Phase 1.
 */
#ifndef _OPENCOG_AGENTZERO_ACTION_SCHEDULER_H
#define _OPENCOG_AGENTZERO_ACTION_SCHEDULER_H

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

namespace opencog {
namespace agentzero {

class AgentZeroCore;
class ActionExecutor;

class ActionScheduler {
public:
    enum class ScheduleResult {
        SCHEDULED,
        REJECTED,
        DUPLICATE,
        INVALID
    };

    struct ScheduledAction {
        Handle action_atom = Handle::UNDEFINED;
        std::chrono::steady_clock::time_point scheduled_time;
        std::chrono::steady_clock::time_point deadline{};
        int priority = 5;
        bool is_periodic = false;
        int repeat_count = 0;
        std::chrono::milliseconds period{0};
        std::vector<Handle> dependencies;
        std::vector<std::string> required_resources;
        Handle state_atom = Handle::UNDEFINED;
    };

    ActionScheduler(AgentZeroCore* agent_core, AtomSpacePtr atomspace);
    ~ActionScheduler();

    void setExecutor(std::shared_ptr<ActionExecutor> executor);

    ScheduleResult scheduleAction(const Handle& action_atom,
                                  std::chrono::steady_clock::time_point when,
                                  int priority = 5);

    ScheduleResult scheduleActionAfter(const Handle& action_atom, int delay_ms,
                                       int priority = 5);

    ScheduleResult scheduleActionBefore(const Handle& action_atom,
                                        std::chrono::steady_clock::time_point deadline,
                                        int priority = 5);

    ScheduleResult schedulePeriodicAction(const Handle& action_atom, int period_ms,
                                          int repeat_count, int priority = 5);

    ScheduleResult scheduleActionWithDependencies(const Handle& action_atom,
                                                  const std::vector<Handle>& dependencies,
                                                  int priority = 5);

    ScheduleResult scheduleActionWithResources(const Handle& action_atom,
                                               const std::vector<std::string>& resources,
                                               int priority = 5);

    std::vector<ScheduledAction> getScheduledActions() const;
    std::chrono::steady_clock::time_point getNextActionTime() const;

    int dispatchDueActions();
    bool cancelScheduled(const Handle& action_atom);
    size_t scheduledCount() const;

    std::string getStatusInfo() const;

private:
    Handle makeScheduleAtom(const ScheduledAction& sa);
    bool dependenciesSatisfied(const ScheduledAction& sa) const;

    AgentZeroCore* _agent_core;
    AtomSpacePtr _atomspace;
    std::shared_ptr<ActionExecutor> _executor;

    mutable std::mutex _mu;
    std::vector<ScheduledAction> _scheduled;
    Handle _schedule_root = Handle::UNDEFINED;
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_ACTION_SCHEDULER_H
