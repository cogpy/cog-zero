/*
 * opencog/agentzero/GoalHierarchy.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Goal Hierarchy Management Implementation
 * Part of AZ-PLAN-001: Implement GoalHierarchy management
 */

#include <opencog/agentzero/GoalHierarchy.h>

#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include <algorithm>
#include <sstream>

using namespace opencog;
using namespace opencog::agentzero;

GoalHierarchy::GoalHierarchy(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
    , _max_hierarchy_depth(10)
    , _max_subgoals_per_goal(20)
    , _enable_automatic_activation(true)
    , _enable_satisfaction_propagation(true)
{
    if (!_atomspace) {
        throw std::invalid_argument("GoalHierarchy requires valid AtomSpace");
    }
    
    initializeGoalHierarchy();
    logger().info("GoalHierarchy: Initialized goal management system");
}

GoalHierarchy::~GoalHierarchy()
{
    logger().info("GoalHierarchy: Destroyed - %d goals managed", (int)_goal_nodes.size());
}

void GoalHierarchy::initializeGoalHierarchy()
{
    createGoalContexts();
    logger().debug("GoalHierarchy: Initialization complete");
}

void GoalHierarchy::createGoalContexts()
{
    // Create AtomSpace contexts for organizing goal hierarchy
    _goal_hierarchy_context = _atomspace->add_node(CONCEPT_NODE, "GoalHierarchyContext");
    _goal_dependency_context = _atomspace->add_node(CONCEPT_NODE, "GoalDependencyContext");
    _goal_satisfaction_context = _atomspace->add_node(CONCEPT_NODE, "GoalSatisfactionContext");
    
    // Set initial truth values
    _goal_hierarchy_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    _goal_dependency_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    _goal_satisfaction_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    
    logger().debug("GoalHierarchy: Created goal hierarchy contexts");
}

bool GoalHierarchy::validateGoalStructure(const Handle& goal_atom)
{
    if (goal_atom == Handle::UNDEFINED) {
        return false;
    }
    
    if (!_atomspace->is_valid_handle(goal_atom)) {
        return false;
    }
    
    return true;
}

bool GoalHierarchy::addGoal(const Handle& goal_atom, 
                           const Handle& parent_goal,
                           GoalPriority priority)
{
    if (!validateGoalStructure(goal_atom)) {
        logger().warn("GoalHierarchy: Invalid goal structure");
        return false;
    }
    
    // Check if goal already exists
    if (_goal_nodes.find(goal_atom) != _goal_nodes.end()) {
        logger().debug("GoalHierarchy: Goal already exists in hierarchy");
        return true;  // Already exists, consider success
    }
    
    // Validate parent goal if specified
    if (parent_goal != Handle::UNDEFINED) {
        if (_goal_nodes.find(parent_goal) == _goal_nodes.end()) {
            logger().warn("GoalHierarchy: Parent goal not found in hierarchy");
            return false;
        }
        
        // Check subgoal limit
        auto& parent_node = _goal_nodes[parent_goal];
        if (parent_node.subgoals.size() >= _max_subgoals_per_goal) {
            logger().warn("GoalHierarchy: Maximum subgoals per goal exceeded");
            return false;
        }
    }
    
    // Create goal node
    GoalNode node;
    node.goal_atom = goal_atom;
    node.parent_goal = parent_goal;
    node.priority = priority;
    node.status = GoalStatus::INACTIVE;
    node.satisfaction_level = 0.0f;
    node.created_time = std::chrono::steady_clock::now();
    
    // Add to hierarchy
    _goal_nodes[goal_atom] = node;
    
    // Update parent's subgoals if applicable
    if (parent_goal != Handle::UNDEFINED) {
        _goal_nodes[parent_goal].subgoals.push_back(goal_atom);
    } else {
        _root_goals.push_back(goal_atom);
    }
    
    // Create AtomSpace representation
    createGoalNodeAtom(node);
    
    // Automatic activation if enabled
    if (_enable_automatic_activation && parent_goal == Handle::UNDEFINED) {
        activateGoal(goal_atom);
    }
    
    logger().debug("GoalHierarchy: Added goal with priority %d", (int)priority);
    return true;
}

bool GoalHierarchy::removeGoal(const Handle& goal_atom)
{
    if (_goal_nodes.find(goal_atom) == _goal_nodes.end()) {
        return false;
    }
    
    const auto& node = _goal_nodes[goal_atom];
    
    // Remove from parent's subgoals
    if (node.parent_goal != Handle::UNDEFINED) {
        auto& parent_subgoals = _goal_nodes[node.parent_goal].subgoals;
        parent_subgoals.erase(
            std::remove(parent_subgoals.begin(), parent_subgoals.end(), goal_atom),
            parent_subgoals.end());
    } else {
        // Remove from root goals
        _root_goals.erase(
            std::remove(_root_goals.begin(), _root_goals.end(), goal_atom),
            _root_goals.end());
    }
    
    // Recursively remove all subgoals
    for (const Handle& subgoal : node.subgoals) {
        removeGoal(subgoal);
    }
    
    // Remove from hierarchy
    _goal_nodes.erase(goal_atom);
    
    logger().debug("GoalHierarchy: Removed goal and all subgoals");
    return true;
}

bool GoalHierarchy::setGoalStatus(const Handle& goal_atom, GoalStatus status)
{
    if (_goal_nodes.find(goal_atom) == _goal_nodes.end()) {
        return false;
    }
    
    auto& node = _goal_nodes[goal_atom];
    GoalStatus old_status = node.status;
    node.status = status;
    
    // Update AtomSpace representation
    Handle status_predicate = _atomspace->add_node(PREDICATE_NODE, "goal-status");
    Handle status_concept = _atomspace->add_node(CONCEPT_NODE, 
        status == GoalStatus::ACTIVE ? "active" :
        status == GoalStatus::SATISFIED ? "satisfied" :
        status == GoalStatus::FAILED ? "failed" :
        status == GoalStatus::SUSPENDED ? "suspended" :
        status == GoalStatus::CANCELLED ? "cancelled" : "inactive");
    
    _atomspace->add_link(EVALUATION_LINK, {
        status_predicate,
        _atomspace->add_link(LIST_LINK, {goal_atom, status_concept})
    });
    
    // Propagate satisfaction if enabled
    if (_enable_satisfaction_propagation && status == GoalStatus::SATISFIED) {
        propagateSatisfaction(goal_atom);
    }
    
    logger().debug("GoalHierarchy: Goal status changed from %d to %d", (int)old_status, (int)status);
    return true;
}

GoalHierarchy::GoalStatus GoalHierarchy::getGoalStatus(const Handle& goal_atom) const
{
    auto it = _goal_nodes.find(goal_atom);
    return (it != _goal_nodes.end()) ? it->second.status : GoalStatus::INACTIVE;
}

bool GoalHierarchy::setGoalSatisfaction(const Handle& goal_atom, float satisfaction)
{
    if (_goal_nodes.find(goal_atom) == _goal_nodes.end()) {
        return false;
    }
    
    // Clamp satisfaction to valid range
    satisfaction = std::max(0.0f, std::min(1.0f, satisfaction));
    
    auto& node = _goal_nodes[goal_atom];
    node.satisfaction_level = satisfaction;
    
    // Update status based on satisfaction
    if (satisfaction >= 1.0f && node.status != GoalStatus::SATISFIED) {
        setGoalStatus(goal_atom, GoalStatus::SATISFIED);
    } else if (satisfaction > 0.0f && node.status == GoalStatus::INACTIVE) {
        setGoalStatus(goal_atom, GoalStatus::ACTIVE);
    }
    
    // Update AtomSpace representation
    Handle satisfaction_predicate = _atomspace->add_node(PREDICATE_NODE, "goal-satisfaction");
    Handle satisfaction_number = _atomspace->add_node(NUMBER_NODE, std::to_string(satisfaction));
    
    _atomspace->add_link(EVALUATION_LINK, {
        satisfaction_predicate,
        _atomspace->add_link(LIST_LINK, {goal_atom, satisfaction_number})
    });
    
    return true;
}

float GoalHierarchy::getGoalSatisfaction(const Handle& goal_atom) const
{
    auto it = _goal_nodes.find(goal_atom);
    return (it != _goal_nodes.end()) ? it->second.satisfaction_level : 0.0f;
}

std::vector<Handle> GoalHierarchy::getSubgoals(const Handle& goal_atom) const
{
    auto it = _goal_nodes.find(goal_atom);
    return (it != _goal_nodes.end()) ? it->second.subgoals : std::vector<Handle>();
}

Handle GoalHierarchy::getParentGoal(const Handle& goal_atom) const
{
    auto it = _goal_nodes.find(goal_atom);
    return (it != _goal_nodes.end()) ? it->second.parent_goal : Handle::UNDEFINED;
}

std::vector<Handle> GoalHierarchy::getActiveGoals() const
{
    return getGoalsByStatus(GoalStatus::ACTIVE);
}

std::vector<Handle> GoalHierarchy::getGoalsByPriority(GoalPriority priority) const
{
    std::vector<Handle> goals;
    for (const auto& [goal_atom, node] : _goal_nodes) {
        if (node.priority == priority) {
            goals.push_back(goal_atom);
        }
    }
    return goals;
}

std::vector<Handle> GoalHierarchy::getGoalsByStatus(GoalStatus status) const
{
    std::vector<Handle> goals;
    for (const auto& [goal_atom, node] : _goal_nodes) {
        if (node.status == status) {
            goals.push_back(goal_atom);
        }
    }
    return goals;
}

bool GoalHierarchy::addGoalDependency(const Handle& dependent_goal, 
                                     const Handle& prerequisite_goal)
{
    if (_goal_nodes.find(dependent_goal) == _goal_nodes.end() ||
        _goal_nodes.find(prerequisite_goal) == _goal_nodes.end()) {
        return false;
    }
    
    auto& dependencies = _goal_nodes[dependent_goal].dependencies;
    
    // Check if dependency already exists
    if (std::find(dependencies.begin(), dependencies.end(), prerequisite_goal) != dependencies.end()) {
        return true;  // Already exists
    }
    
    dependencies.push_back(prerequisite_goal);
    
    // Update dependency graph
    _dependency_graph[dependent_goal].push_back(prerequisite_goal);
    
    // Create AtomSpace representation
    Handle dependency_predicate = _atomspace->add_node(PREDICATE_NODE, "goal-depends-on");
    _atomspace->add_link(EVALUATION_LINK, {
        dependency_predicate,
        _atomspace->add_link(LIST_LINK, {dependent_goal, prerequisite_goal})
    });
    
    logger().debug("GoalHierarchy: Added goal dependency");
    return true;
}

std::vector<Handle> GoalHierarchy::getGoalDependencies(const Handle& goal_atom) const
{
    auto it = _goal_nodes.find(goal_atom);
    return (it != _goal_nodes.end()) ? it->second.dependencies : std::vector<Handle>();
}

bool GoalHierarchy::areGoalDependenciesSatisfied(const Handle& goal_atom) const
{
    auto dependencies = getGoalDependencies(goal_atom);
    
    for (const Handle& dependency : dependencies) {
        if (getGoalStatus(dependency) != GoalStatus::SATISFIED) {
            return false;
        }
    }
    
    return true;
}

bool GoalHierarchy::activateGoal(const Handle& goal_atom)
{
    if (_goal_nodes.find(goal_atom) == _goal_nodes.end()) {
        return false;
    }
    
    // Check if dependencies are satisfied
    if (!areGoalDependenciesSatisfied(goal_atom)) {
        logger().debug("GoalHierarchy: Cannot activate goal - dependencies not satisfied");
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
    // Get all active goals sorted by priority
    std::vector<std::pair<Handle, GoalPriority>> priority_goals;
    
    for (const auto& [goal_atom, node] : _goal_nodes) {
        if (node.status == GoalStatus::ACTIVE && areGoalDependenciesSatisfied(goal_atom)) {
            priority_goals.emplace_back(goal_atom, node.priority);
        }
    }
    
    if (priority_goals.empty()) {
        return Handle::UNDEFINED;
    }
    
    // Sort by priority (lower number = higher priority)
    std::sort(priority_goals.begin(), priority_goals.end(),
              [](const auto& a, const auto& b) {
                  return a.second < b.second;
              });
    
    return priority_goals.front().first;
}

void GoalHierarchy::propagateSatisfaction(const Handle& goal_atom)
{
    if (!_enable_satisfaction_propagation) {
        return;
    }
    
    auto it = _goal_nodes.find(goal_atom);
    if (it == _goal_nodes.end()) {
        return;
    }
    
    const auto& node = it->second;
    
    // Propagate to parent goal if all siblings are satisfied
    if (node.parent_goal != Handle::UNDEFINED) {
        auto& parent_node = _goal_nodes[node.parent_goal];
        bool all_satisfied = true;
        float total_satisfaction = 0.0f;
        
        for (const Handle& subgoal : parent_node.subgoals) {
            float satisfaction = getGoalSatisfaction(subgoal);
            total_satisfaction += satisfaction;
            if (satisfaction < 1.0f) {
                all_satisfied = false;
            }
        }
        
        if (all_satisfied) {
            setGoalSatisfaction(node.parent_goal, 1.0f);
        } else {
            // Set partial satisfaction based on average of subgoals
            float avg_satisfaction = total_satisfaction / parent_node.subgoals.size();
            setGoalSatisfaction(node.parent_goal, avg_satisfaction);
        }
    }
}

Handle GoalHierarchy::createGoalNodeAtom(const GoalNode& node)
{
    // Create AtomSpace representation of goal node
    Handle node_concept = _atomspace->add_node(CONCEPT_NODE, "goal-node");
    
    // Link to goal hierarchy context
    _atomspace->add_link(MEMBER_LINK, {node.goal_atom, _goal_hierarchy_context});
    
    // Set priority
    Handle priority_predicate = _atomspace->add_node(PREDICATE_NODE, "goal-priority");
    Handle priority_number = _atomspace->add_node(NUMBER_NODE, std::to_string((int)node.priority));
    
    _atomspace->add_link(EVALUATION_LINK, {
        priority_predicate,
        _atomspace->add_link(LIST_LINK, {node.goal_atom, priority_number})
    });
    
    return node.goal_atom;
}

int GoalHierarchy::updateGoalHierarchy()
{
    int updated_count = 0;
    
    // Update goal satisfactions and statuses
    for (auto& [goal_atom, node] : _goal_nodes) {
        // Check if inactive goals should be activated
        if (_enable_automatic_activation && 
            node.status == GoalStatus::INACTIVE && 
            areGoalDependenciesSatisfied(goal_atom)) {
            activateGoal(goal_atom);
            updated_count++;
        }
    }
    
    return updated_count;
}

std::string GoalHierarchy::getStatusInfo() const
{
    std::ostringstream info;
    info << "{\n";
    info << "  \"total_goals\": " << _goal_nodes.size() << ",\n";
    info << "  \"root_goals\": " << _root_goals.size() << ",\n";
    info << "  \"active_goals\": " << getActiveGoals().size() << ",\n";
    info << "  \"satisfied_goals\": " << getGoalsByStatus(GoalStatus::SATISFIED).size() << ",\n";
    info << "  \"failed_goals\": " << getGoalsByStatus(GoalStatus::FAILED).size() << ",\n";
    info << "  \"max_hierarchy_depth\": " << _max_hierarchy_depth << ",\n";
    info << "  \"max_subgoals_per_goal\": " << _max_subgoals_per_goal << "\n";
    info << "}";
    return info.str();
}