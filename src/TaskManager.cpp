/*
 * standalone/src/TaskManager.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include "cog0/Logger.h"
#include "cog0/TaskManager.h"

namespace cog0 {

// -----------------------------------------------------------------------
// helpers

std::string priorityName(Priority p) {
    switch (p) {
        case Priority::CRITICAL:   return "CRITICAL";
        case Priority::HIGH:       return "HIGH";
        case Priority::NORMAL:     return "NORMAL";
        case Priority::LOW:        return "LOW";
        case Priority::BACKGROUND: return "BACKGROUND";
    }
    return "UNKNOWN";
}

// -----------------------------------------------------------------------
// Task

Task::Task(size_t id_, std::string name_, std::string desc_, Priority prio)
    : id(id_)
    , name(std::move(name_))
    , description(std::move(desc_))
    , priority(prio)
    , createdAt(std::chrono::steady_clock::now())
{}

void Task::addSubtask(Ptr sub) { subtasks.push_back(std::move(sub)); }

bool Task::allSubtasksComplete() const {
    for (const auto& st : subtasks)
        if (!st->completed) return false;
    return true;
}

std::string Task::toStr() const {
    std::ostringstream oss;
    oss << "[Task#" << id << " \"" << name << "\" "
        << priorityName(priority)
        << " progress=" << (int)(progress * 100) << "%"
        << (completed ? " DONE" : "") << (failed ? " FAILED" : "") << "]";
    return oss.str();
}

// -----------------------------------------------------------------------
// Goal

Goal::Goal(size_t id_, std::string name_, std::string desc_, double prio)
    : id(id_)
    , name(std::move(name_))
    , description(std::move(desc_))
    , priority(prio)
{}

std::string Goal::toStr() const {
    std::ostringstream oss;
    oss << "[Goal#" << id << " \"" << name << "\" prio=" << priority
        << (achieved ? " ACHIEVED" : "") << " tasks=" << tasks.size() << "]";
    return oss.str();
}

// -----------------------------------------------------------------------
// TaskManager

TaskManager::TaskManager(std::shared_ptr<AtomStore> store)
    : _store(std::move(store)) {}

Goal::Ptr TaskManager::setGoal(const std::string& name,
                                const std::string& desc,
                                double priority) {
    auto g  = std::make_shared<Goal>(_nextGoalId++, name, desc, priority);
    g->atom = makeGoalAtom(*g);
    _goals.push_back(g);
    _currentGoal = g;
    logger().info("[TaskManager] Goal set: " + g->toStr());
    return g;
}

bool TaskManager::achieveGoal(size_t goalId) {
    for (auto& g : _goals) {
        if (g->id == goalId) {
            g->achieved = true;
            logger().info("[TaskManager] Goal achieved: " + g->toStr());
            if (_currentGoal && _currentGoal->id == goalId) _currentGoal = nullptr;
            return true;
        }
    }
    return false;
}

Task::Ptr TaskManager::createTask(const std::string& name,
                                   const std::string& desc,
                                   Priority prio,
                                   std::function<bool()> action) {
    auto t   = std::make_shared<Task>(_nextTaskId++, name, desc, prio);
    t->action = std::move(action);
    t->atom  = makeTaskAtom(*t);
    logger().debug("[TaskManager] Task created: " + t->toStr());
    return t;
}

void TaskManager::enqueue(Task::Ptr task) {
    _queue.push_back(std::move(task));
    sortQueue();
}

void TaskManager::attachToGoal(size_t goalId, Task::Ptr task) {
    for (auto& g : _goals) {
        if (g->id == goalId) {
            g->tasks.push_back(task);
            enqueue(task);
            return;
        }
    }
    // No matching goal — just enqueue
    enqueue(std::move(task));
}

bool TaskManager::executeNext() {
    // Remove completed/failed tasks first
    _queue.erase(
        std::remove_if(_queue.begin(), _queue.end(),
                       [](const Task::Ptr& t) { return t->completed || t->failed; }),
        _queue.end());

    if (_queue.empty()) return false;

    auto& t = _queue.front();
    logger().info("[TaskManager] Executing: " + t->toStr());

    bool ok = true;
    if (t->action) {
        try {
            ok = t->action();
        } catch (const std::exception& e) {
            logger().error("[TaskManager] Task threw: " + std::string(e.what()));
            ok = false;
        }
    }
    t->progress    = 1.0;
    t->completed   = ok;
    t->failed      = !ok;
    t->completedAt = std::chrono::steady_clock::now();

    _queue.erase(_queue.begin());
    return true;
}

bool TaskManager::executeAll() {
    bool any = false;
    while (executeNext()) any = true;
    return any;
}

size_t TaskManager::pendingCount() const {
    size_t n = 0;
    for (const auto& t : _queue)
        if (!t->completed && !t->failed) ++n;
    return n;
}

std::vector<Task::Ptr> TaskManager::pendingTasks() const {
    std::vector<Task::Ptr> result;
    for (const auto& t : _queue)
        if (!t->completed && !t->failed) result.push_back(t);
    return result;
}

std::string TaskManager::statusReport() const {
    std::ostringstream oss;
    oss << "=== TaskManager Status ===\n";
    oss << "Goals (" << _goals.size() << "):\n";
    for (const auto& g : _goals) oss << "  " << g->toStr() << "\n";
    oss << "Pending tasks (" << pendingCount() << "):\n";
    for (const auto& t : _queue)
        if (!t->completed && !t->failed)
            oss << "  " << t->toStr() << "\n";
    return oss.str();
}

// -----------------------------------------------------------------------
// private helpers

void TaskManager::sortQueue() {
    std::stable_sort(_queue.begin(), _queue.end(),
        [](const Task::Ptr& a, const Task::Ptr& b) {
            // lower enum value = higher priority
            return static_cast<int>(a->priority) < static_cast<int>(b->priority);
        });
}

Handle TaskManager::makeTaskAtom(const Task& t) {
    auto taskNode = _store->addNode(AtomType::CONCEPT, "Task:" + t.name);
    auto prioNode = _store->addNode(AtomType::CONCEPT,
                                    "Priority:" + priorityName(t.priority));
    auto link = _store->addLink(AtomType::EVALUATION,
                                {_store->addNode(AtomType::PREDICATE, "has-priority"),
                                 _store->addLink(AtomType::LIST, {taskNode, prioNode})});
    link->setTV(TruthValue{1.0, 1.0});
    return taskNode;
}

Handle TaskManager::makeGoalAtom(const Goal& g) {
    auto goalNode = _store->addNode(AtomType::CONCEPT, "Goal:" + g.name);
    goalNode->setTV(TruthValue{g.priority > 0.0 ? g.priority : 1.0, 1.0});
    return goalNode;
}

} // namespace cog0
