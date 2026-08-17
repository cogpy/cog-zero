/*
 * opencog/agentzero/planning/GoalHierarchy.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/planning/GoalHierarchy.h"

using namespace opencog;
using namespace opencog::agentzero::planning;

GoalHierarchy::GoalHierarchy(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
{
    if (!_atomspace)
        throw std::runtime_error("GoalHierarchy requires a valid AtomSpace");
}

GoalHierarchy::~GoalHierarchy() = default;

bool GoalHierarchy::initialize()
{
    if (_initialized) return true;
    createGoalContexts();
    _initialized = true;
    logger().info() << "[GoalHierarchy] Initialized";
    return true;
}

bool GoalHierarchy::shutdown()
{
    _goal_nodes.clear();
    _root_goals.clear();
    _dependency_graph.clear();
    _initialized = false;
    return true;
}

void GoalHierarchy::createGoalContexts()
{
    _goal_hierarchy_context =
        _atomspace->add_node(CONCEPT_NODE, "GoalHierarchyContext");
    _goal_dependency_context =
        _atomspace->add_node(CONCEPT_NODE, "GoalDependencyContext");
    _goal_satisfaction_context =
        _atomspace->add_node(CONCEPT_NODE, "GoalSatisfactionContext");

    _goal_hierarchy_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    _goal_dependency_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    _goal_satisfaction_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
}

bool GoalHierarchy::validateGoalStructure(const Handle& goal_atom) const
{
    if (goal_atom == Handle::UNDEFINED) return false;
    return _atomspace->is_valid_handle(goal_atom);
}

bool GoalHierarchy::addGoal(const Handle& goal_atom,
                            const Handle& parent_goal,
                            GoalPriority priority)
{
    if (!_initialized) initialize();
    if (!validateGoalStructure(goal_atom)) return false;
    if (_goal_nodes.count(goal_atom)) return false;

    if (parent_goal != Handle::UNDEFINED) {
        if (!_goal_nodes.count(parent_goal)) return false;
        auto& parent = _goal_nodes[parent_goal];
        if (static_cast<int>(parent.subgoals.size()) >= _max_subgoals_per_goal)
            return false;
        // depth check
        if (getHierarchyDepth(parent_goal) + 1 > _max_hierarchy_depth)
            return false;
    }

    GoalNode node;
    node.goal_atom = goal_atom;
    node.parent_goal = parent_goal;
    node.priority = priority;
    node.created_time = std::chrono::steady_clock::now();
    node.deadline = node.created_time + std::chrono::hours(24 * 365);

    _goal_nodes[goal_atom] = node;

    if (parent_goal == Handle::UNDEFINED) {
        _root_goals.push_back(goal_atom);
    } else {
        _goal_nodes[parent_goal].subgoals.push_back(goal_atom);
        // Mirror hierarchy: INHERITANCE_LINK(subgoal, parent)
        _atomspace->add_link(INHERITANCE_LINK, {goal_atom, parent_goal});
    }

    mirrorGoalInAtomSpace(_goal_nodes[goal_atom]);
    _atomspace->add_link(MEMBER_LINK, {goal_atom, _goal_hierarchy_context});
    return true;
}

bool GoalHierarchy::removeGoal(const Handle& goal_atom, bool recursive)
{
    auto it = _goal_nodes.find(goal_atom);
    if (it == _goal_nodes.end()) return false;

    GoalNode node = it->second;

    if (recursive) {
        auto children = node.subgoals;
        for (const Handle& child : children)
            removeGoal(child, true);
    } else {
        // re-parent children to grandparent
        for (const Handle& child : node.subgoals) {
            if (!_goal_nodes.count(child)) continue;
            _goal_nodes[child].parent_goal = node.parent_goal;
            if (node.parent_goal == Handle::UNDEFINED) {
                _root_goals.push_back(child);
            } else if (_goal_nodes.count(node.parent_goal)) {
                _goal_nodes[node.parent_goal].subgoals.push_back(child);
            }
        }
    }

    if (node.parent_goal != Handle::UNDEFINED && _goal_nodes.count(node.parent_goal)) {
        auto& subs = _goal_nodes[node.parent_goal].subgoals;
        subs.erase(std::remove(subs.begin(), subs.end(), goal_atom), subs.end());
    } else {
        _root_goals.erase(
            std::remove(_root_goals.begin(), _root_goals.end(), goal_atom),
            _root_goals.end());
    }

    _dependency_graph.erase(goal_atom);
    for (auto& kv : _dependency_graph) {
        auto& deps = kv.second;
        deps.erase(std::remove(deps.begin(), deps.end(), goal_atom), deps.end());
    }
    // clean reverse deps stored on nodes
    for (auto& kv : _goal_nodes) {
        auto& deps = kv.second.dependencies;
        deps.erase(std::remove(deps.begin(), deps.end(), goal_atom), deps.end());
    }

    _goal_nodes.erase(goal_atom);
    return true;
}

bool GoalHierarchy::addSubgoal(const Handle& parent_goal, const Handle& subgoal)
{
    if (!_goal_nodes.count(parent_goal)) return false;
    if (_goal_nodes.count(subgoal)) {
        // already present — reparent if needed
        if (_goal_nodes[subgoal].parent_goal == parent_goal) return true;
        return false;
    }
    return addGoal(subgoal, parent_goal, _goal_nodes[parent_goal].priority);
}

bool GoalHierarchy::removeSubgoal(const Handle& parent_goal, const Handle& subgoal)
{
    if (!_goal_nodes.count(parent_goal) || !_goal_nodes.count(subgoal)) return false;
    if (_goal_nodes[subgoal].parent_goal != parent_goal) return false;
    // Detach: make subgoal a root (preserve orphans)
    auto& subs = _goal_nodes[parent_goal].subgoals;
    subs.erase(std::remove(subs.begin(), subs.end(), subgoal), subs.end());
    _goal_nodes[subgoal].parent_goal = Handle::UNDEFINED;
    if (std::find(_root_goals.begin(), _root_goals.end(), subgoal) == _root_goals.end())
        _root_goals.push_back(subgoal);
    return true;
}

bool GoalHierarchy::setGoalStatus(const Handle& goal_atom, GoalStatus status)
{
    auto it = _goal_nodes.find(goal_atom);
    if (it == _goal_nodes.end()) return false;
    it->second.status = status;

    if (status == GoalStatus::SATISFIED) {
        it->second.satisfaction_level = 1.0f;
        goal_atom->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));
        if (_enable_satisfaction_propagation)
            propagateSatisfaction(goal_atom);
    }
    mirrorGoalInAtomSpace(it->second);
    return true;
}

GoalHierarchy::GoalStatus GoalHierarchy::getGoalStatus(const Handle& goal_atom) const
{
    auto it = _goal_nodes.find(goal_atom);
    if (it == _goal_nodes.end()) return GoalStatus::INACTIVE;
    return it->second.status;
}

bool GoalHierarchy::setGoalSatisfaction(const Handle& goal_atom, float satisfaction)
{
    auto it = _goal_nodes.find(goal_atom);
    if (it == _goal_nodes.end()) return false;
    satisfaction = std::max(0.0f, std::min(1.0f, satisfaction));
    it->second.satisfaction_level = satisfaction;
    goal_atom->setTruthValue(SimpleTruthValue::createTV(satisfaction, 0.9));
    if (satisfaction >= 0.99f)
        it->second.status = GoalStatus::SATISFIED;
    if (_enable_satisfaction_propagation)
        propagateSatisfaction(goal_atom);
    return true;
}

float GoalHierarchy::getGoalSatisfaction(const Handle& goal_atom) const
{
    auto it = _goal_nodes.find(goal_atom);
    if (it == _goal_nodes.end()) return 0.0f;
    return it->second.satisfaction_level;
}

bool GoalHierarchy::setGoalPriority(const Handle& goal_atom, GoalPriority priority)
{
    auto it = _goal_nodes.find(goal_atom);
    if (it == _goal_nodes.end()) return false;
    it->second.priority = priority;
    return true;
}

GoalHierarchy::GoalPriority GoalHierarchy::getGoalPriority(const Handle& goal_atom) const
{
    auto it = _goal_nodes.find(goal_atom);
    if (it == _goal_nodes.end()) return GoalPriority::NORMAL;
    return it->second.priority;
}

std::vector<Handle> GoalHierarchy::getSubgoals(const Handle& goal_atom) const
{
    auto it = _goal_nodes.find(goal_atom);
    if (it == _goal_nodes.end()) return {};
    return it->second.subgoals;
}

Handle GoalHierarchy::getParentGoal(const Handle& goal_atom) const
{
    auto it = _goal_nodes.find(goal_atom);
    if (it == _goal_nodes.end()) return Handle::UNDEFINED;
    return it->second.parent_goal;
}

std::vector<Handle> GoalHierarchy::getAncestors(const Handle& goal_atom) const
{
    std::vector<Handle> ancestors;
    Handle cur = getParentGoal(goal_atom);
    int guard = 0;
    while (cur != Handle::UNDEFINED && guard++ < _max_hierarchy_depth) {
        ancestors.push_back(cur);
        cur = getParentGoal(cur);
    }
    return ancestors;
}

std::vector<Handle> GoalHierarchy::getLeafGoals(const Handle& root_goal) const
{
    std::vector<Handle> leaves;
    if (!_goal_nodes.count(root_goal)) return leaves;
    std::vector<Handle> stack{root_goal};
    while (!stack.empty()) {
        Handle g = stack.back();
        stack.pop_back();
        const auto& subs = _goal_nodes.at(g).subgoals;
        if (subs.empty()) {
            leaves.push_back(g);
        } else {
            for (const Handle& s : subs) stack.push_back(s);
        }
    }
    return leaves;
}

int GoalHierarchy::depthFrom(const Handle& goal_atom, int current, int limit) const
{
    if (current > limit) return current;
    auto it = _goal_nodes.find(goal_atom);
    if (it == _goal_nodes.end() || it->second.subgoals.empty()) return current;
    int max_d = current;
    for (const Handle& s : it->second.subgoals)
        max_d = std::max(max_d, depthFrom(s, current + 1, limit));
    return max_d;
}

int GoalHierarchy::getHierarchyDepth(const Handle& goal_atom) const
{
    if (!_goal_nodes.count(goal_atom)) return 0;
    return depthFrom(goal_atom, 1, _max_hierarchy_depth);
}

std::vector<Handle> GoalHierarchy::getActiveGoals() const
{
    return getGoalsByStatus(GoalStatus::ACTIVE);
}

std::vector<Handle> GoalHierarchy::getGoalsByPriority(GoalPriority priority) const
{
    std::vector<Handle> out;
    for (const auto& kv : _goal_nodes)
        if (kv.second.priority == priority) out.push_back(kv.first);
    return out;
}

std::vector<Handle> GoalHierarchy::getGoalsByStatus(GoalStatus status) const
{
    std::vector<Handle> out;
    for (const auto& kv : _goal_nodes)
        if (kv.second.status == status) out.push_back(kv.first);
    return out;
}

bool GoalHierarchy::hasGoal(const Handle& goal_atom) const
{
    return _goal_nodes.count(goal_atom) > 0;
}

bool GoalHierarchy::dependsOnDFS(const Handle& current, const Handle& target,
                                 std::map<Handle, bool>& visiting) const
{
    if (current == target) return true;
    if (visiting[current]) return false;
    visiting[current] = true;
    auto it = _dependency_graph.find(current);
    if (it != _dependency_graph.end()) {
        for (const Handle& pre : it->second) {
            if (dependsOnDFS(pre, target, visiting)) return true;
        }
    }
    // also walk parent chain as soft dependency
    auto git = _goal_nodes.find(current);
    if (git != _goal_nodes.end() && git->second.parent_goal != Handle::UNDEFINED) {
        if (dependsOnDFS(git->second.parent_goal, target, visiting)) return true;
    }
    return false;
}

bool GoalHierarchy::hasDependencyCycle(const Handle& from, const Handle& to) const
{
    // Adding edge from -> to (from depends on to) creates a cycle if to already depends on from
    std::map<Handle, bool> visiting;
    return dependsOnDFS(to, from, visiting);
}

bool GoalHierarchy::addGoalDependency(const Handle& dependent_goal,
                                      const Handle& prerequisite_goal)
{
    if (!_goal_nodes.count(dependent_goal) || !_goal_nodes.count(prerequisite_goal))
        return false;
    if (dependent_goal == prerequisite_goal) return false;
    if (hasDependencyCycle(dependent_goal, prerequisite_goal)) return false;

    auto& deps = _goal_nodes[dependent_goal].dependencies;
    if (std::find(deps.begin(), deps.end(), prerequisite_goal) != deps.end())
        return true;
    deps.push_back(prerequisite_goal);
    _dependency_graph[dependent_goal].push_back(prerequisite_goal);
    mirrorDependency(dependent_goal, prerequisite_goal);
    return true;
}

bool GoalHierarchy::removeGoalDependency(const Handle& dependent_goal,
                                         const Handle& prerequisite_goal)
{
    if (!_goal_nodes.count(dependent_goal)) return false;
    auto& deps = _goal_nodes[dependent_goal].dependencies;
    auto it = std::remove(deps.begin(), deps.end(), prerequisite_goal);
    if (it == deps.end()) return false;
    deps.erase(it, deps.end());
    auto& g = _dependency_graph[dependent_goal];
    g.erase(std::remove(g.begin(), g.end(), prerequisite_goal), g.end());
    return true;
}

std::vector<Handle> GoalHierarchy::getGoalDependencies(const Handle& goal_atom) const
{
    auto it = _goal_nodes.find(goal_atom);
    if (it == _goal_nodes.end()) return {};
    return it->second.dependencies;
}

bool GoalHierarchy::areGoalDependenciesSatisfied(const Handle& goal_atom) const
{
    auto it = _goal_nodes.find(goal_atom);
    if (it == _goal_nodes.end()) return false;
    for (const Handle& pre : it->second.dependencies) {
        auto pit = _goal_nodes.find(pre);
        if (pit == _goal_nodes.end()) return false;
        if (pit->second.status != GoalStatus::SATISFIED &&
            pit->second.satisfaction_level < 0.99f)
            return false;
    }
    return true;
}

bool GoalHierarchy::activateGoal(const Handle& goal_atom)
{
    if (!_goal_nodes.count(goal_atom)) return false;
    if (!areGoalDependenciesSatisfied(goal_atom) &&
        !_goal_nodes[goal_atom].dependencies.empty()) {
        // still allow activation when there are no deps; block when unsatisfied deps
        bool any = !_goal_nodes[goal_atom].dependencies.empty();
        if (any && !areGoalDependenciesSatisfied(goal_atom))
            return false;
    }
    return setGoalStatus(goal_atom, GoalStatus::ACTIVE);
}

bool GoalHierarchy::deactivateGoal(const Handle& goal_atom)
{
    return setGoalStatus(goal_atom, GoalStatus::INACTIVE);
}

Handle GoalHierarchy::getNextGoalToPlan() const
{
    Handle best = Handle::UNDEFINED;
    GoalPriority best_p = GoalPriority::DEFERRED;
    for (const auto& kv : _goal_nodes) {
        const GoalNode& n = kv.second;
        if (n.status != GoalStatus::ACTIVE && n.status != GoalStatus::INACTIVE)
            continue;
        if (n.status == GoalStatus::SATISFIED || n.status == GoalStatus::CANCELLED)
            continue;
        // prefer leaves that are ready
        if (!n.subgoals.empty()) continue;
        if (!areGoalDependenciesSatisfied(kv.first) && !n.dependencies.empty())
            continue;
        if (best == Handle::UNDEFINED || n.priority < best_p) {
            best = kv.first;
            best_p = n.priority;
        }
    }
    return best;
}

float GoalHierarchy::calculateHierarchicalAchievement(const Handle& goal_atom) const
{
    auto it = _goal_nodes.find(goal_atom);
    if (it == _goal_nodes.end()) return 0.0f;

    const GoalNode& n = it->second;
    if (n.subgoals.empty())
        return n.satisfaction_level;

    float child_sum = 0.0f;
    int count = 0;
    for (const Handle& s : n.subgoals) {
        child_sum += calculateHierarchicalAchievement(s);
        ++count;
    }
    float child_avg = count > 0 ? child_sum / static_cast<float>(count) : 0.0f;
    return 0.3f * n.satisfaction_level + 0.7f * child_avg;
}

void GoalHierarchy::propagateSatisfaction(const Handle& goal_atom)
{
    // Update parent TVs / status from hierarchical achievement without
    // overwriting each parent's own satisfaction_level (used as the 0.3 weight).
    Handle parent = getParentGoal(goal_atom);
    int guard = 0;
    while (parent != Handle::UNDEFINED && guard++ < _max_hierarchy_depth) {
        float ach = calculateHierarchicalAchievement(parent);
        parent->setTruthValue(SimpleTruthValue::createTV(ach, 0.8));
        if (ach >= 0.99f)
            _goal_nodes[parent].status = GoalStatus::SATISFIED;
        parent = getParentGoal(parent);
    }
}

bool GoalHierarchy::propagatePriority(const Handle& goal_atom, GoalPriority priority)
{
    if (!_goal_nodes.count(goal_atom)) return false;
    _goal_nodes[goal_atom].priority = priority;
    for (const Handle& s : _goal_nodes[goal_atom].subgoals) {
        int p = static_cast<int>(priority) + 1;
        if (p > static_cast<int>(GoalPriority::DEFERRED))
            p = static_cast<int>(GoalPriority::DEFERRED);
        propagatePriority(s, static_cast<GoalPriority>(p));
    }
    return true;
}

int GoalHierarchy::updateGoalHierarchy()
{
    int updated = 0;
    for (auto& kv : _goal_nodes) {
        if (kv.second.subgoals.empty()) continue;
        float ach = calculateHierarchicalAchievement(kv.first);
        if (std::fabs(ach - kv.second.satisfaction_level) > 1e-4f) {
            kv.second.satisfaction_level = ach;
            ++updated;
        }
        if (ach >= 0.99f && kv.second.status != GoalStatus::SATISFIED) {
            kv.second.status = GoalStatus::SATISFIED;
            ++updated;
        }
    }
    return updated;
}

void GoalHierarchy::mirrorGoalInAtomSpace(const GoalNode& node)
{
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "goal-status");
    Handle status_node = _atomspace->add_node(
        CONCEPT_NODE, statusToString(node.status));
    _atomspace->add_link(EVALUATION_LINK, {
        pred,
        _atomspace->add_link(LIST_LINK, {node.goal_atom, status_node})
    });

    Handle ppred = _atomspace->add_node(PREDICATE_NODE, "goal_priority");
    Handle pval = _atomspace->add_node(
        NUMBER_NODE, std::to_string(static_cast<int>(node.priority)));
    _atomspace->add_link(EVALUATION_LINK, {ppred, node.goal_atom, pval});
}

void GoalHierarchy::mirrorDependency(const Handle& dependent,
                                     const Handle& prerequisite)
{
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "goal-depends-on");
    Handle list = _atomspace->add_link(LIST_LINK, {dependent, prerequisite});
    Handle eval = _atomspace->add_link(EVALUATION_LINK, {pred, list});
    _atomspace->add_link(MEMBER_LINK, {eval, _goal_dependency_context});
}

std::string GoalHierarchy::getStatusInfo() const
{
    std::ostringstream oss;
    oss << "{\"goals\":" << _goal_nodes.size()
        << ",\"roots\":" << _root_goals.size()
        << ",\"active\":" << getActiveGoals().size()
        << ",\"initialized\":" << (_initialized ? "true" : "false")
        << "}";
    return oss.str();
}

std::string GoalHierarchy::statusToString(GoalStatus s)
{
    switch (s) {
        case GoalStatus::INACTIVE: return "inactive";
        case GoalStatus::ACTIVE: return "active";
        case GoalStatus::SATISFIED: return "satisfied";
        case GoalStatus::FAILED: return "failed";
        case GoalStatus::SUSPENDED: return "suspended";
        case GoalStatus::CANCELLED: return "cancelled";
    }
    return "unknown";
}

std::string GoalHierarchy::priorityToString(GoalPriority p)
{
    switch (p) {
        case GoalPriority::CRITICAL: return "critical";
        case GoalPriority::HIGH: return "high";
        case GoalPriority::NORMAL: return "normal";
        case GoalPriority::LOW: return "low";
        case GoalPriority::DEFERRED: return "deferred";
    }
    return "normal";
}
