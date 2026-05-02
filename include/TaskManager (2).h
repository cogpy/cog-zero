/*
 * standalone/include/cog0/TaskManager.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Task and goal management for the standalone cog0 agent.
 */

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "AtomStore.h"

namespace cog0 {

// -----------------------------------------------------------------------
// Task priority levels

enum class Priority { CRITICAL = 0, HIGH, NORMAL, LOW, BACKGROUND };

std::string priorityName(Priority p);

// -----------------------------------------------------------------------
// Task — a unit of work with optional sub-tasks

struct Task {
    using Ptr = std::shared_ptr<Task>;

    size_t      id       = 0;
    std::string name;
    std::string description;
    Priority    priority  = Priority::NORMAL;
    double      progress  = 0.0;  // 0.0 – 1.0
    bool        completed = false;
    bool        failed    = false;
    Handle      atom;             // representation in AtomStore

    std::vector<Ptr> subtasks;

    // Optional action callback — called when the task is executed
    std::function<bool()> action;

    std::chrono::steady_clock::time_point createdAt;
    std::chrono::steady_clock::time_point completedAt;

    Task(size_t id, std::string name, std::string desc, Priority prio = Priority::NORMAL);

    void addSubtask(Ptr sub);
    bool allSubtasksComplete() const;
    std::string toStr() const;
};

// -----------------------------------------------------------------------
// Goal — high-level objective that decomposes into tasks

struct Goal {
    using Ptr = std::shared_ptr<Goal>;

    size_t      id        = 0;
    std::string name;
    std::string description;
    double      priority  = 1.0;   // higher = more important
    bool        achieved  = false;
    Handle      atom;

    std::vector<Task::Ptr> tasks;

    Goal(size_t id, std::string name, std::string desc, double prio = 1.0);
    std::string toStr() const;
};

// -----------------------------------------------------------------------
// TaskManager

class TaskManager {
public:
    explicit TaskManager(std::shared_ptr<AtomStore> store);

    // Goal management
    Goal::Ptr setGoal(const std::string& name, const std::string& desc = "",
                      double priority = 1.0);
    [[nodiscard]] Goal::Ptr currentGoal() const { return _currentGoal; }
    [[nodiscard]] const std::vector<Goal::Ptr>& goals() const { return _goals; }
    bool achieveGoal(size_t goalId);

    // Task creation and queuing
    Task::Ptr createTask(const std::string& name,
                         const std::string& desc  = "",
                         Priority           prio  = Priority::NORMAL,
                         std::function<bool()> action = nullptr);

    void enqueue(Task::Ptr task);
    void attachToGoal(size_t goalId, Task::Ptr task);

    // Execution
    bool executeNext();              // Execute highest-priority pending task
    bool executeAll();               // Execute all pending tasks
    [[nodiscard]] size_t pendingCount() const;

    // Re-sort the task queue (e.g. after external priority changes)
    void resortQueue() { sortQueue(); }

    // Introspection
    [[nodiscard]] std::vector<Task::Ptr> pendingTasks() const;
    [[nodiscard]] std::string statusReport() const;

private:
    std::shared_ptr<AtomStore> _store;
    std::vector<Goal::Ptr>     _goals;
    std::vector<Task::Ptr>     _queue;
    Goal::Ptr                  _currentGoal;
    size_t _nextTaskId = 1;
    size_t _nextGoalId = 1;

    void sortQueue();
    Handle makeTaskAtom(const Task& t);
    Handle makeGoalAtom(const Goal& g);
};

} // namespace cog0
