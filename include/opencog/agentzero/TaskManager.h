/*
 * opencog/agentzero/TaskManager.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Goal/task management using EvaluationLinks and StateLinks.
 * Part of AGENT-ZERO-GENESIS Phase 1.
 */
#ifndef _OPENCOG_AGENTZERO_TASK_MANAGER_H
#define _OPENCOG_AGENTZERO_TASK_MANAGER_H

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

class TaskManager {
public:
    enum class Priority {
        CRITICAL = 0,
        HIGH = 1,
        NORMAL = 2,
        LOW = 3,
        BACKGROUND = 4
    };

    enum class TaskStatus {
        PENDING,
        RUNNING,
        COMPLETED,
        FAILED,
        CANCELLED
    };

    struct Task {
        size_t id = 0;
        std::string name;
        std::string description;
        Priority priority = Priority::NORMAL;
        TaskStatus status = TaskStatus::PENDING;
        double progress = 0.0;
        Handle atom = Handle::UNDEFINED;
        std::function<bool()> action;
        std::vector<size_t> subtask_ids;
    };

    struct Goal {
        size_t id = 0;
        std::string name;
        std::string description;
        double priority = 1.0;
        bool achieved = false;
        Handle atom = Handle::UNDEFINED;
        std::vector<size_t> task_ids;
    };

    TaskManager(AgentZeroCore* agent_core, AtomSpacePtr atomspace);
    ~TaskManager();

    // Goals (represented as EvaluationLinks / StateLinks in AtomSpace)
    Handle setGoal(const std::string& name, const std::string& description = "",
                   double priority = 1.0);
    Handle getCurrentGoalAtom() const;
    bool achieveGoal(size_t goal_id);
    size_t getGoalCount() const;
    std::vector<Goal> getGoals() const;

    // Tasks
    Handle createTask(const std::string& name, const std::string& description = "",
                      Priority priority = Priority::NORMAL,
                      std::function<bool()> action = nullptr);
    bool enqueueTask(size_t task_id);
    bool enqueueTask(const Handle& task_atom);
    bool attachTaskToGoal(size_t goal_id, size_t task_id);
    bool executeNext();
    size_t executeAll();
    size_t getPendingTaskCount() const;
    TaskStatus getTaskStatus(size_t task_id) const;

    // Cognitive-step hook used by AgentZeroCore
    bool processTaskManagement();

    std::string getStatusInfo() const;

private:
    Handle makeGoalAtom(const Goal& g);
    Handle makeTaskAtom(const Task& t);
    void updateTaskStateLink(const Task& t);
    void sortQueue();

    AgentZeroCore* _agent_core;
    AtomSpacePtr _atomspace;

    mutable std::mutex _mu;
    size_t _next_goal_id = 1;
    size_t _next_task_id = 1;
    std::map<size_t, Goal> _goals;
    std::map<size_t, Task> _tasks;
    std::vector<size_t> _queue;
    size_t _current_goal_id = 0;

    Handle _goals_root = Handle::UNDEFINED;
    Handle _tasks_root = Handle::UNDEFINED;
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_TASK_MANAGER_H
