/*
 * opencog/agentzero/GoalHierarchy.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Goal Hierarchy Management
 * Manages goal trees and dependencies for Agent-Zero planning
 * Part of AZ-PLAN-001: Implement GoalHierarchy management
 */

#ifndef _OPENCOG_AGENTZERO_GOAL_HIERARCHY_H
#define _OPENCOG_AGENTZERO_GOAL_HIERARCHY_H

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <functional>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {

/**
 * GoalHierarchy - Goal tree and dependency management
 *
 * This class manages hierarchical goal structures with dependencies,
 * priorities, and satisfaction tracking using AtomSpace representation.
 *
 * Key features:
 * - Hierarchical goal decomposition
 * - Goal dependency tracking
 * - Priority-based goal ordering
 * - Goal satisfaction monitoring
 * - AtomSpace integration for goal representation
 */
class GoalHierarchy
{
public:
    // Goal status
    enum class GoalStatus {
        INACTIVE,     // Goal is not currently active
        ACTIVE,       // Goal is actively being pursued
        SATISFIED,    // Goal has been achieved
        FAILED,       // Goal cannot be achieved
        SUSPENDED,    // Goal is temporarily suspended
        CANCELLED     // Goal was explicitly cancelled
    };
    
    // Goal priority levels
    enum class GoalPriority {
        CRITICAL = 1,  // Must be achieved immediately
        HIGH = 2,      // High priority
        NORMAL = 3,    // Normal priority (default)
        LOW = 4,       // Low priority
        DEFERRED = 5   // Can be deferred indefinitely
    };

    // Goal node structure
    struct GoalNode {
        Handle goal_atom;
        Handle parent_goal;
        std::vector<Handle> subgoals;
        std::vector<Handle> dependencies;
        GoalStatus status;
        GoalPriority priority;
        float satisfaction_level;
        std::chrono::steady_clock::time_point created_time;
        std::chrono::steady_clock::time_point deadline;
        
        GoalNode() : status(GoalStatus::INACTIVE), priority(GoalPriority::NORMAL),
                    satisfaction_level(0.0f) {}
    };

private:
    // Core references
    AtomSpacePtr _atomspace;
    
    // Goal hierarchy structures
    std::map<Handle, GoalNode> _goal_nodes;
    std::vector<Handle> _root_goals;
    std::map<Handle, std::vector<Handle>> _dependency_graph;
    
    // AtomSpace contexts
    Handle _goal_hierarchy_context;
    Handle _goal_dependency_context;
    Handle _goal_satisfaction_context;
    
    // Configuration
    int _max_hierarchy_depth;
    int _max_subgoals_per_goal;
    bool _enable_automatic_activation;
    bool _enable_satisfaction_propagation;
    
    // Internal methods
    void initializeGoalHierarchy();
    void createGoalContexts();
    bool validateGoalStructure(const Handle& goal_atom);
    void propagateSatisfaction(const Handle& goal_atom);
    void updateGoalDependencies(const Handle& goal_atom);
    Handle createGoalNodeAtom(const GoalNode& node);

public:
    /**
     * Constructor
     * @param atomspace Shared pointer to the AtomSpace
     */
    GoalHierarchy(AtomSpacePtr atomspace);
    
    /**
     * Destructor
     */
    ~GoalHierarchy();
    
    // Goal management
    /**
     * Add a new goal to the hierarchy
     * @param goal_atom Handle to the goal atom
     * @param parent_goal Handle to parent goal (Handle::UNDEFINED for root)
     * @param priority Goal priority level
     * @return true if goal was added successfully
     */
    bool addGoal(const Handle& goal_atom, 
                const Handle& parent_goal = Handle::UNDEFINED,
                GoalPriority priority = GoalPriority::NORMAL);
    
    /**
     * Remove a goal from the hierarchy
     * @param goal_atom Handle to the goal atom
     * @return true if goal was removed successfully
     */
    bool removeGoal(const Handle& goal_atom);
    
    /**
     * Add a subgoal to an existing goal
     * @param parent_goal Handle to the parent goal
     * @param subgoal Handle to the subgoal
     * @return true if subgoal was added successfully
     */
    bool addSubgoal(const Handle& parent_goal, const Handle& subgoal);
    
    /**
     * Remove a subgoal from a goal
     * @param parent_goal Handle to the parent goal
     * @param subgoal Handle to the subgoal
     * @return true if subgoal was removed successfully
     */
    bool removeSubgoal(const Handle& parent_goal, const Handle& subgoal);
    
    // Goal status management
    /**
     * Set goal status
     * @param goal_atom Handle to the goal atom
     * @param status New goal status
     * @return true if status was updated successfully
     */
    bool setGoalStatus(const Handle& goal_atom, GoalStatus status);
    
    /**
     * Get goal status
     * @param goal_atom Handle to the goal atom
     * @return GoalStatus of the goal
     */
    GoalStatus getGoalStatus(const Handle& goal_atom) const;
    
    /**
     * Set goal satisfaction level
     * @param goal_atom Handle to the goal atom
     * @param satisfaction Satisfaction level (0.0 - 1.0)
     * @return true if satisfaction was updated successfully
     */
    bool setGoalSatisfaction(const Handle& goal_atom, float satisfaction);
    
    /**
     * Get goal satisfaction level
     * @param goal_atom Handle to the goal atom
     * @return Satisfaction level (0.0 - 1.0)
     */
    float getGoalSatisfaction(const Handle& goal_atom) const;
    
    // Goal hierarchy queries
    /**
     * Get all root goals
     * @return vector of root goal handles
     */
    std::vector<Handle> getRootGoals() const { return _root_goals; }
    
    /**
     * Get subgoals of a goal
     * @param goal_atom Handle to the parent goal
     * @return vector of subgoal handles
     */
    std::vector<Handle> getSubgoals(const Handle& goal_atom) const;
    
    /**
     * Get parent goal
     * @param goal_atom Handle to the goal atom
     * @return Handle to parent goal or Handle::UNDEFINED
     */
    Handle getParentGoal(const Handle& goal_atom) const;
    
    /**
     * Get all active goals
     * @return vector of active goal handles
     */
    std::vector<Handle> getActiveGoals() const;
    
    /**
     * Get goals by priority
     * @param priority Goal priority level
     * @return vector of goal handles with specified priority
     */
    std::vector<Handle> getGoalsByPriority(GoalPriority priority) const;
    
    /**
     * Get goals by status
     * @param status Goal status
     * @return vector of goal handles with specified status
     */
    std::vector<Handle> getGoalsByStatus(GoalStatus status) const;
    
    // Goal dependencies
    /**
     * Add dependency between goals
     * @param dependent_goal Goal that depends on another
     * @param prerequisite_goal Goal that must be satisfied first
     * @return true if dependency was added successfully
     */
    bool addGoalDependency(const Handle& dependent_goal, 
                          const Handle& prerequisite_goal);
    
    /**
     * Remove dependency between goals
     * @param dependent_goal Goal that depends on another
     * @param prerequisite_goal Goal that must be satisfied first
     * @return true if dependency was removed successfully
     */
    bool removeGoalDependency(const Handle& dependent_goal,
                             const Handle& prerequisite_goal);
    
    /**
     * Get goal dependencies
     * @param goal_atom Handle to the goal atom
     * @return vector of prerequisite goal handles
     */
    std::vector<Handle> getGoalDependencies(const Handle& goal_atom) const;
    
    /**
     * Check if goal dependencies are satisfied
     * @param goal_atom Handle to the goal atom
     * @return true if all dependencies are satisfied
     */
    bool areGoalDependenciesSatisfied(const Handle& goal_atom) const;
    
    // Goal activation and planning
    /**
     * Activate a goal for planning
     * @param goal_atom Handle to the goal atom
     * @return true if goal was activated successfully
     */
    bool activateGoal(const Handle& goal_atom);
    
    /**
     * Deactivate a goal
     * @param goal_atom Handle to the goal atom
     * @return true if goal was deactivated successfully
     */
    bool deactivateGoal(const Handle& goal_atom);
    
    /**
     * Get next goal to plan for based on priority and dependencies
     * @return Handle to next goal or Handle::UNDEFINED
     */
    Handle getNextGoalToPlan() const;
    
    /**
     * Update goal hierarchy based on current state
     * @return number of goals updated
     */
    int updateGoalHierarchy();
    
    // Configuration
    /**
     * Set maximum hierarchy depth
     * @param max_depth Maximum depth of goal hierarchy
     */
    void setMaxHierarchyDepth(int max_depth) {
        _max_hierarchy_depth = max_depth;
    }
    
    /**
     * Set maximum subgoals per goal
     * @param max_subgoals Maximum number of subgoals
     */
    void setMaxSubgoalsPerGoal(int max_subgoals) {
        _max_subgoals_per_goal = max_subgoals;
    }
    
    /**
     * Configure automatic features
     * @param auto_activation Enable automatic goal activation
     * @param satisfaction_propagation Enable satisfaction propagation
     */
    void configureFeatures(bool auto_activation, bool satisfaction_propagation) {
        _enable_automatic_activation = auto_activation;
        _enable_satisfaction_propagation = satisfaction_propagation;
    }
    
    // AtomSpace integration
    /**
     * Get the goal hierarchy context atom
     * @return Handle to goal hierarchy context
     */
    Handle getGoalHierarchyContext() const { return _goal_hierarchy_context; }
    
    /**
     * Get status information for debugging
     * @return JSON string with hierarchy status details
     */
    std::string getStatusInfo() const;
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_GOAL_HIERARCHY_H