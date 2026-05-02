/*
 * opencog/agentzero/TaskManager.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Task Manager Implementation
 * Manages goal decomposition and execution with AtomSpace integration
 * Part of the AGENT-ZERO-GENESIS project
 */

#ifndef _OPENCOG_AGENTZERO_TASK_MANAGER_H
#define _OPENCOG_AGENTZERO_TASK_MANAGER_H

#include <memory>
#include <vector>
#include <queue>
#include <string>
#include <map>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {

class AgentZeroCore;

/**
 * TaskManager - Manages goal decomposition and task execution
 *
 * This class provides hierarchical goal and task management using
 * AtomSpace for representation and tracking. It supports goal
 * decomposition, priority management, and execution monitoring.
 *
 * Key features:
 * - Hierarchical goal representation in AtomSpace
 * - Task decomposition and dependency tracking
 * - Priority-based task scheduling
 * - Execution status monitoring
 * - Goal achievement tracking
 */
class TaskManager
{
public:
    // Task and goal status enumeration
    enum class TaskStatus {
        PENDING,    // Task created but not started
        ACTIVE,     // Task currently being executed
        COMPLETED,  // Task finished successfully
        FAILED,     // Task failed to complete
        CANCELLED,  // Task was cancelled
        SUSPENDED   // Task temporarily suspended
    };
    
    // Goal priority levels
    enum class Priority {
        LOW = 1,
        MEDIUM = 5,
        HIGH = 10,
        CRITICAL = 20
    };

private:
    // Core references
    AgentZeroCore* _agent_core;
    AtomSpacePtr _atomspace;
    
    // Task management structures
    std::queue<Handle> _task_queue;
    std::map<Handle, TaskStatus> _task_status;
    std::map<Handle, Priority> _task_priorities;
    std::map<Handle, std::vector<Handle>> _task_dependencies;
    
    // Current execution state
    Handle _current_task;
    Handle _current_goal;
    Handle _goal_hierarchy_root;
    
    // AtomSpace handles for task management
    Handle _task_context;
    Handle _goal_context;
    Handle _execution_context;
    
    // Configuration
    int _max_concurrent_tasks;
    bool _enable_goal_decomposition;
    bool _enable_priority_scheduling;
    
    // Internal methods
    void initializeTaskManagement();
    Handle createTaskAtom(const std::string& task_description, Priority priority);
    Handle createGoalAtom(const std::string& goal_description);
    bool decomposeGoal(const Handle& goal_atom);
    std::vector<Handle> getReadyTasks();
    bool checkTaskDependencies(const Handle& task_atom);
    void updateTaskStatus(const Handle& task_atom, TaskStatus status);
    TruthValuePtr calculateGoalAchievement(const Handle& goal_atom);
    Handle findTaskForGoal(const Handle& goal_atom);

public:
    /**
     * Constructor
     * @param agent_core Pointer to the parent AgentZeroCore instance
     * @param atomspace Shared pointer to the AtomSpace
     */
    TaskManager(AgentZeroCore* agent_core, AtomSpacePtr atomspace);
    
    /**
     * Destructor - cleans up task management resources
     */
    ~TaskManager();
    
    // Goal management
    /**
     * Set a new primary goal for the agent
     * @param goal_description Human-readable goal description
     * @param auto_decompose Whether to automatically decompose the goal
     * @return Handle to the created goal atom
     */
    Handle setGoal(const std::string& goal_description, bool auto_decompose = true);
    
    /**
     * Add a subgoal to an existing goal
     * @param parent_goal Handle to the parent goal
     * @param subgoal_description Description of the subgoal
     * @return Handle to the created subgoal atom
     */
    Handle addSubgoal(const Handle& parent_goal, const std::string& subgoal_description);
    
    /**
     * Get the current primary goal
     * @return Handle to the current goal atom
     */
    Handle getCurrentGoal() const { return _current_goal; }
    
    /**
     * Check if a goal has been achieved
     * @param goal_atom Handle to the goal to check
     * @return TruthValue representing achievement level
     */
    TruthValuePtr isGoalAchieved(const Handle& goal_atom);
    
    // Task management
    /**
     * Create a new task
     * @param task_description Human-readable task description
     * @param priority Task priority level
     * @param goal_atom Optional associated goal
     * @return Handle to the created task atom
     */
    Handle createTask(const std::string& task_description, 
                     Priority priority = Priority::MEDIUM,
                     const Handle& goal_atom = Handle::UNDEFINED);
    
    /**
     * Add a dependency between tasks
     * @param task_atom The task that depends on another
     * @param dependency_atom The task that must complete first
     * @return true if dependency was added successfully
     */
    bool addTaskDependency(const Handle& task_atom, const Handle& dependency_atom);
    
    /**
     * Get the next task to execute based on priority and dependencies
     * @return Handle to the next task, or Handle::UNDEFINED if none available
     */
    Handle getNextTask();
    
    /**
     * Mark a task as completed
     * @param task_atom Handle to the completed task
     * @param success Whether the task completed successfully
     * @return true if status was updated successfully
     */
    bool completeTask(const Handle& task_atom, bool success = true);
    
    /**
     * Cancel a pending or active task
     * @param task_atom Handle to the task to cancel
     * @return true if task was cancelled successfully
     */
    bool cancelTask(const Handle& task_atom);
    
    /**
     * Get the current active task
     * @return Handle to the current task, or Handle::UNDEFINED if none active
     */
    Handle getCurrentTask() const { return _current_task; }
    
    /**
     * Get the status of a task
     * @param task_atom Handle to the task
     * @return TaskStatus of the specified task
     */
    TaskStatus getTaskStatus(const Handle& task_atom) const;
    
    // Queue management
    /**
     * Get the number of pending tasks
     * @return number of tasks in the queue
     */
    size_t getPendingTaskCount() const { return _task_queue.size(); }
    
    /**
     * Get all tasks with a specific status
     * @param status The status to filter by
     * @return vector of task handles with the specified status
     */
    std::vector<Handle> getTasksByStatus(TaskStatus status) const;
    
    /**
     * Clear all pending tasks
     * @return number of tasks that were cleared
     */
    size_t clearPendingTasks();
    
    // Configuration
    /**
     * Set the maximum number of concurrent tasks
     * @param max_tasks Maximum concurrent tasks (default: 1)
     */
    void setMaxConcurrentTasks(int max_tasks) { _max_concurrent_tasks = max_tasks; }
    
    /**
     * Enable or disable goal decomposition
     * @param enable Whether to enable automatic goal decomposition
     */
    void setGoalDecompositionEnabled(bool enable) { _enable_goal_decomposition = enable; }
    
    /**
     * Enable or disable priority-based scheduling
     * @param enable Whether to use priority-based task scheduling
     */
    void setPrioritySchedulingEnabled(bool enable) { _enable_priority_scheduling = enable; }
    
    // AtomSpace integration
    /**
     * Get the task management context atom
     * @return Handle to task context
     */
    Handle getTaskContext() const { return _task_context; }
    
    /**
     * Get the goal management context atom
     * @return Handle to goal context
     */
    Handle getGoalContext() const { return _goal_context; }
    
    /**
     * Get status information for debugging
     * @return JSON string with status details
     */
    std::string getStatusInfo() const;
    
    /**
     * Get goal hierarchy information
     * @param goal_atom The goal to analyze (defaults to current goal)
     * @return JSON string with goal hierarchy details
     */
    std::string getGoalHierarchyInfo(const Handle& goal_atom = Handle::UNDEFINED) const;
    
    // Enhanced GoalHierarchy management methods
    /**
     * Get all subgoals of a given goal
     * @param parent_goal Handle to the parent goal
     * @return vector of subgoal handles
     */
    std::vector<Handle> getSubgoals(const Handle& parent_goal) const;
    
    /**
     * Get the parent goal of a given goal
     * @param subgoal Handle to the subgoal
     * @return Handle to parent goal, or Handle::UNDEFINED if top-level
     */
    Handle getParentGoal(const Handle& subgoal) const;
    
    /**
     * Get all ancestors of a goal in the hierarchy
     * @param goal Handle to the goal
     * @return vector of ancestor goals from immediate parent to root
     */
    std::vector<Handle> getGoalAncestors(const Handle& goal) const;
    
    /**
     * Calculate hierarchical goal achievement
     * Considers achievement of all subgoals when calculating parent goal achievement
     * @param goal Handle to the goal
     * @return TruthValue representing hierarchical achievement
     */
    TruthValuePtr calculateHierarchicalGoalAchievement(const Handle& goal);
    
    /**
     * Propagate goal priority through hierarchy
     * High priority parent goals boost priority of subgoals
     * @param goal Handle to the goal to propagate from
     * @param priority Priority to propagate
     * @return true if propagation was successful
     */
    bool propagateGoalPriority(const Handle& goal, Priority priority);
    
    /**
     * Synchronize goal status across hierarchy
     * Updates parent and child goal statuses based on completion states
     * @param goal Handle to the goal to synchronize from
     * @return true if synchronization was successful
     */
    bool synchronizeGoalHierarchy(const Handle& goal);
    
    /**
     * Find all leaf goals (goals with no subgoals) in hierarchy
     * @param root_goal Handle to start search from (defaults to current goal)
     * @return vector of leaf goal handles
     */
    std::vector<Handle> getLeafGoals(const Handle& root_goal = Handle::UNDEFINED) const;
    
    /**
     * Get goal hierarchy depth
     * @param goal Handle to the goal (defaults to current goal)
     * @return depth of the goal hierarchy starting from this goal
     */
    int getGoalHierarchyDepth(const Handle& goal = Handle::UNDEFINED) const;
    
    /**
     * Remove a goal and all its subgoals from hierarchy
     * @param goal Handle to the goal to remove
     * @param preserve_orphans Whether to preserve orphaned subgoals
     * @return true if removal was successful
     */
    bool removeGoalFromHierarchy(const Handle& goal, bool preserve_orphans = false);
    
    /**
     * Process task management for one cycle
     * Called by the cognitive loop
     * @return true if processing completed successfully
     */
    bool processTaskManagement();
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_TASK_MANAGER_H