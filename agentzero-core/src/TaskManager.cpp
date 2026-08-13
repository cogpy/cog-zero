/*
 * agentzero-core/src/TaskManager.cpp
 *
 * Goal/task management using EvaluationLinks and StateLinks.
 */
#include "opencog/agentzero/TaskManager.h"
#include "opencog/agentzero/AgentZeroCore.h"

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>

using namespace opencog;
using namespace opencog::agentzero;

TaskManager::TaskManager(AgentZeroCore* agent_core, AtomSpacePtr atomspace)
    : _agent_core(agent_core)
    , _atomspace(std::move(atomspace))
{
    if (!_atomspace) {
        throw std::invalid_argument("TaskManager requires a valid AtomSpace");
    }
    _goals_root = _atomspace->add_node(CONCEPT_NODE, "AgentGoals");
    _tasks_root = _atomspace->add_node(CONCEPT_NODE, "AgentTasks");
}

TaskManager::~TaskManager() = default;

Handle TaskManager::setGoal(const std::string& name, const std::string& description,
                            double priority)
{
    std::lock_guard<std::mutex> lock(_mu);
    Goal g;
    g.id = _next_goal_id++;
    g.name = name;
    g.description = description;
    g.priority = priority;
    g.atom = makeGoalAtom(g);
    _goals[g.id] = g;
    _current_goal_id = g.id;
    logger().info() << "[TaskManager] Goal set: " << name;
    return g.atom;
}

Handle TaskManager::getCurrentGoalAtom() const
{
    std::lock_guard<std::mutex> lock(_mu);
    auto it = _goals.find(_current_goal_id);
    if (it == _goals.end()) return Handle::UNDEFINED;
    return it->second.atom;
}

bool TaskManager::achieveGoal(size_t goal_id)
{
    std::lock_guard<std::mutex> lock(_mu);
    auto it = _goals.find(goal_id);
    if (it == _goals.end()) return false;
    it->second.achieved = true;
    Handle status = _atomspace->add_node(CONCEPT_NODE, "achieved");
    Handle state = _atomspace->add_link(STATE_LINK, HandleSeq{it->second.atom, status});
    SimpleTruthValue::setTV(state, 1.0, 1.0);
    return true;
}

size_t TaskManager::getGoalCount() const
{
    std::lock_guard<std::mutex> lock(_mu);
    return _goals.size();
}

std::vector<TaskManager::Goal> TaskManager::getGoals() const
{
    std::lock_guard<std::mutex> lock(_mu);
    std::vector<Goal> out;
    out.reserve(_goals.size());
    for (const auto& kv : _goals) out.push_back(kv.second);
    return out;
}

Handle TaskManager::createTask(const std::string& name, const std::string& description,
                               Priority priority, std::function<bool()> action)
{
    std::lock_guard<std::mutex> lock(_mu);
    Task t;
    t.id = _next_task_id++;
    t.name = name;
    t.description = description;
    t.priority = priority;
    t.action = std::move(action);
    t.status = TaskStatus::PENDING;
    t.atom = makeTaskAtom(t);
    updateTaskStateLink(t);
    _tasks[t.id] = t;
    _queue.push_back(t.id);
    sortQueue();
    return t.atom;
}

bool TaskManager::enqueueTask(size_t task_id)
{
    std::lock_guard<std::mutex> lock(_mu);
    if (_tasks.find(task_id) == _tasks.end()) return false;
    if (std::find(_queue.begin(), _queue.end(), task_id) == _queue.end()) {
        _queue.push_back(task_id);
        sortQueue();
    }
    return true;
}

bool TaskManager::enqueueTask(const Handle& task_atom)
{
    std::lock_guard<std::mutex> lock(_mu);
    for (const auto& kv : _tasks) {
        if (kv.second.atom == task_atom) {
            if (std::find(_queue.begin(), _queue.end(), kv.first) == _queue.end()) {
                _queue.push_back(kv.first);
                sortQueue();
            }
            return true;
        }
    }
    return false;
}

bool TaskManager::attachTaskToGoal(size_t goal_id, size_t task_id)
{
    std::lock_guard<std::mutex> lock(_mu);
    auto git = _goals.find(goal_id);
    auto tit = _tasks.find(task_id);
    if (git == _goals.end() || tit == _tasks.end()) return false;
    git->second.task_ids.push_back(task_id);
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "has_task");
    Handle link = _atomspace->add_link(
        EVALUATION_LINK,
        HandleSeq{pred, _atomspace->add_link(LIST_LINK, HandleSeq{git->second.atom, tit->second.atom})});
    SimpleTruthValue::setTV(link, 1.0, 1.0);
    return true;
}

bool TaskManager::executeNext()
{
    size_t id = 0;
    Task task_copy;
    {
        std::lock_guard<std::mutex> lock(_mu);
        if (_queue.empty()) return false;
        id = _queue.front();
        _queue.erase(_queue.begin());
        auto it = _tasks.find(id);
        if (it == _tasks.end()) return false;
        it->second.status = TaskStatus::RUNNING;
        updateTaskStateLink(it->second);
        task_copy = it->second;
    }

    bool ok = true;
    if (task_copy.action) {
        try {
            ok = task_copy.action();
        } catch (const std::exception& e) {
            logger().error() << "[TaskManager] Task threw: " << e.what();
            ok = false;
        }
    }

    {
        std::lock_guard<std::mutex> lock(_mu);
        auto it = _tasks.find(id);
        if (it == _tasks.end()) return false;
        if (ok) {
            it->second.status = TaskStatus::COMPLETED;
            it->second.progress = 1.0;
        } else {
            it->second.status = TaskStatus::FAILED;
        }
        updateTaskStateLink(it->second);
    }
    return ok;
}

size_t TaskManager::executeAll()
{
    size_t n = 0;
    while (true) {
        size_t pending = 0;
        {
            std::lock_guard<std::mutex> lock(_mu);
            pending = _queue.size();
        }
        if (pending == 0) break;
        if (executeNext()) ++n;
        else ++n; // count attempts including failures
    }
    return n;
}

size_t TaskManager::getPendingTaskCount() const
{
    std::lock_guard<std::mutex> lock(_mu);
    return _queue.size();
}

TaskManager::TaskStatus TaskManager::getTaskStatus(size_t task_id) const
{
    std::lock_guard<std::mutex> lock(_mu);
    auto it = _tasks.find(task_id);
    if (it == _tasks.end()) return TaskStatus::FAILED;
    return it->second.status;
}

bool TaskManager::processTaskManagement()
{
    // Lightweight maintenance: ensure AtomSpace state links stay consistent
    std::lock_guard<std::mutex> lock(_mu);
    for (auto& kv : _tasks) {
        updateTaskStateLink(kv.second);
    }
    return true;
}

std::string TaskManager::getStatusInfo() const
{
    std::lock_guard<std::mutex> lock(_mu);
    std::ostringstream oss;
    oss << "{\"goals\":" << _goals.size()
        << ",\"tasks\":" << _tasks.size()
        << ",\"pending\":" << _queue.size() << "}";
    return oss.str();
}

Handle TaskManager::makeGoalAtom(const Goal& g)
{
    Handle goal_node = _atomspace->add_node(CONCEPT_NODE, "Goal:" + g.name);
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "is_goal");
    Handle eval = _atomspace->add_link(
        EVALUATION_LINK,
        HandleSeq{pred, _atomspace->add_link(LIST_LINK, HandleSeq{goal_node, _goals_root})});
    SimpleTruthValue::setTV(eval, std::clamp(g.priority, 0.0, 1.0), 0.95);

    Handle status = _atomspace->add_node(CONCEPT_NODE, g.achieved ? "achieved" : "active");
    Handle state = _atomspace->add_link(STATE_LINK, HandleSeq{goal_node, status});
    SimpleTruthValue::setTV(state, 1.0, 1.0);
    return goal_node;
}

Handle TaskManager::makeTaskAtom(const Task& t)
{
    Handle task_node = _atomspace->add_node(CONCEPT_NODE, "Task:" + t.name);
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "is_task");
    Handle eval = _atomspace->add_link(
        EVALUATION_LINK,
        HandleSeq{pred, _atomspace->add_link(LIST_LINK, HandleSeq{task_node, _tasks_root})});
    double prio = 1.0 - (static_cast<int>(t.priority) * 0.2);
    SimpleTruthValue::setTV(eval, std::clamp(prio, 0.1, 1.0), 0.9);
    return task_node;
}

void TaskManager::updateTaskStateLink(const Task& t)
{
    if (!_atomspace || !t.atom) return;
    std::string status_name = "pending";
    switch (t.status) {
        case TaskStatus::PENDING: status_name = "pending"; break;
        case TaskStatus::RUNNING: status_name = "running"; break;
        case TaskStatus::COMPLETED: status_name = "completed"; break;
        case TaskStatus::FAILED: status_name = "failed"; break;
        case TaskStatus::CANCELLED: status_name = "cancelled"; break;
    }
    Handle status = _atomspace->add_node(CONCEPT_NODE, status_name);
    Handle state = _atomspace->add_link(STATE_LINK, HandleSeq{t.atom, status});
    SimpleTruthValue::setTV(state, std::clamp(t.progress, 0.0, 1.0), 1.0);
}

void TaskManager::sortQueue()
{
    std::stable_sort(_queue.begin(), _queue.end(), [this](size_t a, size_t b) {
        return static_cast<int>(_tasks[a].priority) < static_cast<int>(_tasks[b].priority);
    });
}
