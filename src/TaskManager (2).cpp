/*
 * src/TaskManager.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Task Manager Implementation
 * Manages goal decomposition and execution with AtomSpace integration
 * Part of the AGENT-ZERO-GENESIS project
 */

#include <sstream>
#include <algorithm>
#include <ctime>
#include <cstdlib>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

#include "opencog/agentzero/TaskManager.h"
#include "opencog/agentzero/AgentZeroCore.h"

using namespace opencog;
using namespace opencog::agentzero;

TaskManager::TaskManager(AgentZeroCore* agent_core, AtomSpacePtr atomspace)
    : _agent_core(agent_core)
    , _atomspace(atomspace)
    , _current_task(Handle::UNDEFINED)
    , _current_goal(Handle::UNDEFINED)
    , _goal_hierarchy_root(Handle::UNDEFINED)
    , _task_context(Handle::UNDEFINED)
    , _goal_context(Handle::UNDEFINED)
    , _execution_context(Handle::UNDEFINED)
    , _max_concurrent_tasks(1)
    , _enable_goal_decomposition(true)
    , _enable_priority_scheduling(true)
{
    logger().info() << "[TaskManager] Constructor: Initializing task management";
    initializeTaskManagement();
}

TaskManager::~TaskManager()
{
    logger().info() << "[TaskManager] Destructor: Cleaning up task management";
}

Handle TaskManager::setGoal(const std::string& goal_description, bool auto_decompose)
{
    if (goal_description.empty()) {
        logger().error() << "[TaskManager] Cannot set goal with empty description";
        return Handle::UNDEFINED;
    }
    
    logger().info() << "[TaskManager] Setting goal: " << goal_description;
    
    try {
        // Create goal atom with enhanced metadata
        Handle goal_atom = createGoalAtom(goal_description);
        if (goal_atom == Handle::UNDEFINED) {
            logger().error() << "[TaskManager] Failed to create goal atom";
            return Handle::UNDEFINED;
        }
        
        // Clear previous goal if exists
        if (_current_goal != Handle::UNDEFINED) {
            logger().info() << "[TaskManager] Replacing previous goal: " << _current_goal;
            
            // Mark previous goal as suspended
            TruthValuePtr suspended_tv = SimpleTruthValue::createTV(0.3, 0.8);
            Handle suspended_pred = _atomspace->add_node(PREDICATE_NODE, "suspended");
            Handle suspended_eval = _atomspace->add_link(EVALUATION_LINK, {suspended_pred, _current_goal});
            suspended_eval->setTruthValue(suspended_tv);
        }
        
        _current_goal = goal_atom;
        
        // Create active goal relationship in AtomSpace
        Handle active_pred = _atomspace->add_node(PREDICATE_NODE, "active_goal");
        Handle active_goal_eval = _atomspace->add_link(EVALUATION_LINK, {active_pred, goal_atom});
        TruthValuePtr active_tv = SimpleTruthValue::createTV(1.0, 0.95);
        active_goal_eval->setTruthValue(active_tv);
        
        // Link goal to agent context
        Handle agent_goal_link = _atomspace->add_link(MEMBER_LINK, {goal_atom, _goal_context});
        
        // Set goal timestamp
        Handle timestamp_pred = _atomspace->add_node(PREDICATE_NODE, "goal_created");
        Handle timestamp_value = _atomspace->add_node(NUMBER_NODE, std::to_string(std::time(nullptr)));
        Handle timestamp_eval = _atomspace->add_link(EVALUATION_LINK, {timestamp_pred, goal_atom, timestamp_value});
        
        // Auto-decompose if requested and enabled
        if (auto_decompose && _enable_goal_decomposition) {
            logger().info() << "[TaskManager] Auto-decomposing goal: " << goal_description;
            if (!decomposeGoal(goal_atom)) {
                logger().warn() << "[TaskManager] Goal decomposition failed, continuing with basic goal";
            }
        } else {
            // Create a single primary task for the goal
            Handle primary_task = createTask("Primary_" + goal_description, Priority::HIGH, goal_atom);
            logger().info() << "[TaskManager] Created primary task for goal: " << primary_task;
        }
        
        logger().info() << "[TaskManager] Goal successfully set and configured: " << goal_atom;
        return goal_atom;
        
    } catch (const std::exception& e) {
        logger().error() << "[TaskManager] Error setting goal: " << e.what();
        return Handle::UNDEFINED;
    }
}

Handle TaskManager::addSubgoal(const Handle& parent_goal, const std::string& subgoal_description)
{
    if (parent_goal == Handle::UNDEFINED) {
        logger().error() << "[TaskManager] Cannot add subgoal to undefined parent goal";
        return Handle::UNDEFINED;
    }
    
    if (subgoal_description.empty()) {
        logger().error() << "[TaskManager] Cannot add subgoal with empty description";
        return Handle::UNDEFINED;
    }
    
    logger().debug() << "[TaskManager] Adding subgoal: " << subgoal_description 
                    << " to parent: " << parent_goal;
    
    try {
        Handle subgoal = createGoalAtom(subgoal_description);
        if (subgoal == Handle::UNDEFINED) {
            logger().error() << "[TaskManager] Failed to create subgoal atom";
            return Handle::UNDEFINED;
        }
        
        // Create hierarchical relationship using INHERITANCE_LINK
        // Parent goal "inherits" or "contains" the subgoal
        HandleSeq subgoal_link;
        subgoal_link.push_back(parent_goal);
        subgoal_link.push_back(subgoal);
        Handle hierarchy_link = _atomspace->add_link(INHERITANCE_LINK, std::move(subgoal_link));
        
        // Add metadata about the subgoal relationship
        Handle subgoal_pred = _atomspace->add_node(PREDICATE_NODE, "subgoal_of");
        Handle subgoal_eval = _atomspace->add_link(EVALUATION_LINK, {subgoal_pred, subgoal, parent_goal});
        TruthValuePtr subgoal_tv = SimpleTruthValue::createTV(1.0, 0.9);
        subgoal_eval->setTruthValue(subgoal_tv);
        
        // Link subgoal to goal context
        Handle subgoal_context_link = _atomspace->add_link(MEMBER_LINK, {subgoal, _goal_context});
        
        // Set initial subgoal achievement to not achieved
        TruthValuePtr initial_tv = SimpleTruthValue::createTV(0.0, 0.8);
        subgoal->setTruthValue(initial_tv);
        
        logger().debug() << "[TaskManager] Successfully added subgoal: " << subgoal 
                        << " with hierarchy link: " << hierarchy_link;
        
        return subgoal;
        
    } catch (const std::exception& e) {
        logger().error() << "[TaskManager] Error adding subgoal: " << e.what();
        return Handle::UNDEFINED;
    }
}

TruthValuePtr TaskManager::isGoalAchieved(const Handle& goal_atom)
{
    return calculateGoalAchievement(goal_atom);
}

Handle TaskManager::createTask(const std::string& task_description, Priority priority, const Handle& goal_atom)
{
    logger().debug() << "[TaskManager] Creating task: " << task_description;
    
    Handle task_atom = createTaskAtom(task_description, priority);
    
    // Associate with goal if provided
    if (goal_atom != Handle::UNDEFINED) {
        HandleSeq task_goal_link;
        task_goal_link.push_back(task_atom);
        task_goal_link.push_back(goal_atom);
        _atomspace->add_link(EVALUATION_LINK, std::move(task_goal_link));
    }
    
    // Add to task queue and set initial status
    _task_queue.push(task_atom);
    _task_status[task_atom] = TaskStatus::PENDING;
    _task_priorities[task_atom] = priority;
    
    return task_atom;
}

bool TaskManager::addTaskDependency(const Handle& task_atom, const Handle& dependency_atom)
{
    logger().debug() << "[TaskManager] Adding task dependency";
    
    _task_dependencies[task_atom].push_back(dependency_atom);
    
    // Create dependency link in AtomSpace
    HandleSeq dependency_link;
    dependency_link.push_back(task_atom);
    dependency_link.push_back(dependency_atom);
    _atomspace->add_link(SEQUENTIAL_AND_LINK, std::move(dependency_link));
    
    return true;
}

Handle TaskManager::getNextTask()
{
    std::vector<Handle> ready_tasks = getReadyTasks();
    
    if (ready_tasks.empty()) {
        return Handle::UNDEFINED;
    }
    
    if (!_enable_priority_scheduling) {
        return ready_tasks.front();
    }
    
    // Find highest priority task
    Handle best_task = ready_tasks.front();
    Priority best_priority = _task_priorities[best_task];
    
    for (const Handle& task : ready_tasks) {
        Priority task_priority = _task_priorities[task];
        if (static_cast<int>(task_priority) > static_cast<int>(best_priority)) {
            best_task = task;
            best_priority = task_priority;
        }
    }
    
    return best_task;
}

bool TaskManager::completeTask(const Handle& task_atom, bool success)
{
    logger().debug() << "[TaskManager] Completing task: " << task_atom;
    
    TaskStatus new_status = success ? TaskStatus::COMPLETED : TaskStatus::FAILED;
    updateTaskStatus(task_atom, new_status);
    
    if (task_atom == _current_task) {
        _current_task = Handle::UNDEFINED;
    }
    
    return true;
}

bool TaskManager::cancelTask(const Handle& task_atom)
{
    logger().debug() << "[TaskManager] Cancelling task: " << task_atom;
    
    updateTaskStatus(task_atom, TaskStatus::CANCELLED);
    
    if (task_atom == _current_task) {
        _current_task = Handle::UNDEFINED;
    }
    
    return true;
}

TaskManager::TaskStatus TaskManager::getTaskStatus(const Handle& task_atom) const
{
    auto it = _task_status.find(task_atom);
    return (it != _task_status.end()) ? it->second : TaskStatus::PENDING;
}

std::vector<Handle> TaskManager::getTasksByStatus(TaskStatus status) const
{
    std::vector<Handle> result;
    
    for (const auto& pair : _task_status) {
        if (pair.second == status) {
            result.push_back(pair.first);
        }
    }
    
    return result;
}

size_t TaskManager::clearPendingTasks()
{
    logger().info() << "[TaskManager] Clearing pending tasks";
    
    size_t cleared_count = 0;
    while (!_task_queue.empty()) {
        _task_queue.pop();
        cleared_count++;
    }
    
    return cleared_count;
}

std::string TaskManager::getStatusInfo() const
{
    std::ostringstream status;
    status << "{";
    
    // Basic statistics
    status << "\"pending_tasks\":" << _task_queue.size() << ",";
    status << "\"total_tasks\":" << _task_status.size() << ",";
    status << "\"current_task\":\"" << _current_task << "\",";
    status << "\"current_goal\":\"" << _current_goal << "\",";
    
    // Task status breakdown
    int pending = 0, active = 0, completed = 0, failed = 0, cancelled = 0, suspended = 0;
    for (const auto& pair : _task_status) {
        switch (pair.second) {
            case TaskStatus::PENDING: pending++; break;
            case TaskStatus::ACTIVE: active++; break;
            case TaskStatus::COMPLETED: completed++; break;
            case TaskStatus::FAILED: failed++; break;
            case TaskStatus::CANCELLED: cancelled++; break;
            case TaskStatus::SUSPENDED: suspended++; break;
        }
    }
    
    status << "\"task_status_breakdown\":{";
    status << "\"pending\":" << pending << ",";
    status << "\"active\":" << active << ",";
    status << "\"completed\":" << completed << ",";
    status << "\"failed\":" << failed << ",";
    status << "\"cancelled\":" << cancelled << ",";
    status << "\"suspended\":" << suspended;
    status << "},";
    
    // Priority distribution
    int low = 0, medium = 0, high = 0, critical = 0;
    for (const auto& pair : _task_priorities) {
        switch (pair.second) {
            case Priority::LOW: low++; break;
            case Priority::MEDIUM: medium++; break;
            case Priority::HIGH: high++; break;
            case Priority::CRITICAL: critical++; break;
        }
    }
    
    status << "\"priority_distribution\":{";
    status << "\"low\":" << low << ",";
    status << "\"medium\":" << medium << ",";
    status << "\"high\":" << high << ",";
    status << "\"critical\":" << critical;
    status << "},";
    
    // Configuration
    status << "\"configuration\":{";
    status << "\"max_concurrent_tasks\":" << _max_concurrent_tasks << ",";
    status << "\"goal_decomposition_enabled\":" << (_enable_goal_decomposition ? "true" : "false") << ",";
    status << "\"priority_scheduling_enabled\":" << (_enable_priority_scheduling ? "true" : "false");
    status << "},";
    
    // Context atoms
    status << "\"atomspace_context\":{";
    status << "\"task_context\":\"" << _task_context << "\",";
    status << "\"goal_context\":\"" << _goal_context << "\",";
    status << "\"execution_context\":\"" << _execution_context << "\",";
    status << "\"goal_hierarchy_root\":\"" << _goal_hierarchy_root << "\"";
    status << "},";
    
    // Goal achievement if available
    if (_current_goal != Handle::UNDEFINED) {
        // We can't call calculateGoalAchievement from const method, so provide basic info
        status << "\"current_goal_info\":{";
        status << "\"has_active_goal\":true,";
        status << "\"goal_atom\":\"" << _current_goal << "\"";
        status << "}";
    } else {
        status << "\"current_goal_info\":{\"has_active_goal\":false}";
    }
    
    status << "}";
    return status.str();
}

std::string TaskManager::getGoalHierarchyInfo(const Handle& goal_atom) const
{
    Handle target_goal = (goal_atom != Handle::UNDEFINED) ? goal_atom : _current_goal;
    
    std::ostringstream info;
    info << "{";
    
    if (target_goal == Handle::UNDEFINED) {
        info << "\"error\":\"No goal specified or current goal is undefined\"";
        info << "}";
        return info.str();
    }
    
    try {
        info << "\"goal\":\"" << target_goal << "\",";
        info << "\"goal_name\":\"" << target_goal->get_name() << "\",";
        
        // Find subgoals
        HandleSeq subgoals;
        HandleSeq all_links;
        _atomspace->get_handles_by_type(all_links, INHERITANCE_LINK);
        
        for (const Handle& link : all_links) {
            HandleSeq link_outgoing = link->getOutgoingSet();
            // Check if this inheritance link involves our target goal
            if (link_outgoing.size() == 2) {
                if (link_outgoing[1] == target_goal) {
                    // link_outgoing[0] is a subgoal of target_goal
                    subgoals.push_back(link_outgoing[0]);
                }
            }
        }
        
        info << "\"subgoals\":[";
        for (size_t i = 0; i < subgoals.size(); ++i) {
            if (i > 0) info << ",";
            info << "{";
            info << "\"atom\":\"" << subgoals[i] << "\",";
            info << "\"name\":\"" << subgoals[i]->get_name() << "\"";
            
            // Check if subgoal has associated tasks
            HandleSeq eval_links;
            _atomspace->get_handles_by_type(eval_links, EVALUATION_LINK);
            Handle associated_task = Handle::UNDEFINED;
            for (const Handle& eval : eval_links) {
                HandleSeq eval_outgoing = eval->getOutgoingSet();
                if (eval_outgoing.size() == 2 && eval_outgoing[1] == subgoals[i]) {
                    associated_task = eval_outgoing[0];
                    break;
                }
            }
            
            if (associated_task != Handle::UNDEFINED) {
                info << ",\"associated_task\":\"" << associated_task << "\"";
                auto status_it = _task_status.find(associated_task);
                if (status_it != _task_status.end()) {
                    info << ",\"task_status\":\"";
                    switch (status_it->second) {
                        case TaskStatus::PENDING: info << "PENDING"; break;
                        case TaskStatus::ACTIVE: info << "ACTIVE"; break;
                        case TaskStatus::COMPLETED: info << "COMPLETED"; break;
                        case TaskStatus::FAILED: info << "FAILED"; break;
                        case TaskStatus::CANCELLED: info << "CANCELLED"; break;
                        case TaskStatus::SUSPENDED: info << "SUSPENDED"; break;
                    }
                    info << "\"";
                }
            }
            
            info << "}";
        }
        info << "],";
        
        info << "\"subgoal_count\":" << subgoals.size() << ",";
        
        // Goal achievement (simplified since we can't call non-const method)
        TruthValuePtr goal_tv = target_goal->getTruthValue();
        if (goal_tv) {
            info << "\"current_truth_value\":{";
            info << "\"strength\":" << goal_tv->get_mean() << ",";
            info << "\"confidence\":" << goal_tv->get_confidence();
            info << "}";
        } else {
            info << "\"current_truth_value\":null";
        }
        
    } catch (const std::exception& e) {
        info << "\"error\":\"" << e.what() << "\"";
    }
    
    info << "}";
    return info.str();
}

bool TaskManager::processTaskManagement()
{
    logger().debug() << "[TaskManager] Processing task management cycle";
    
    try {
        // Get next task if none currently active
        if (_current_task == Handle::UNDEFINED) {
            _current_task = getNextTask();
            
            if (_current_task != Handle::UNDEFINED) {
                updateTaskStatus(_current_task, TaskStatus::ACTIVE);
                logger().debug() << "[TaskManager] Started task: " << _current_task;
                
                // Add task start timestamp
                Handle start_time_pred = _atomspace->add_node(PREDICATE_NODE, "task_started");
                Handle start_time_value = _atomspace->add_node(NUMBER_NODE, std::to_string(std::time(nullptr)));
                Handle start_time_eval = _atomspace->add_link(EVALUATION_LINK, {start_time_pred, _current_task, start_time_value});
                
                return true; // Task started, let it process in next cycle
            }
        }
        
        // Process current task (enhanced task processing logic)
        if (_current_task != Handle::UNDEFINED) {
            logger().debug() << "[TaskManager] Processing active task: " << _current_task;
            
            // Simulate task processing - in a real implementation, this would:
            // 1. Execute the actual task logic
            // 2. Check for completion conditions
            // 3. Handle task-specific errors
            // 4. Update progress indicators
            
            // For now, we'll complete tasks based on their type/complexity
            std::string task_name = _current_task->get_name();
            bool should_complete = true;
            bool success = true;
            
            // Simulate different task processing behaviors
            if (task_name.find("Analyze") != std::string::npos) {
                // Analysis tasks take time but usually succeed
                logger().debug() << "[TaskManager] Processing analysis task";
            } else if (task_name.find("Plan") != std::string::npos) {
                // Planning tasks require careful consideration
                logger().debug() << "[TaskManager] Processing planning task";
            } else if (task_name.find("Execute") != std::string::npos) {
                // Execution tasks might fail sometimes
                logger().debug() << "[TaskManager] Processing execution task";
                // Simulate occasional failure (10% chance)
                if (std::rand() % 10 == 0) {
                    success = false;
                    logger().warn() << "[TaskManager] Task execution simulation failed";
                }
            }
            
            if (should_complete) {
                // Add task completion timestamp
                Handle end_time_pred = _atomspace->add_node(PREDICATE_NODE, "task_completed");
                Handle end_time_value = _atomspace->add_node(NUMBER_NODE, std::to_string(std::time(nullptr)));
                Handle end_time_eval = _atomspace->add_link(EVALUATION_LINK, {end_time_pred, _current_task, end_time_value});
                
                completeTask(_current_task, success);
                logger().debug() << "[TaskManager] Completed task: " << _current_task->to_string() 
                                << " (success: " << (success ? "true" : "false") << ")";
                
                // If task failed, may need to retry or create alternative tasks
                if (!success) {
                    logger().info() << "[TaskManager] Task failed, considering recovery options";
                    // In a real implementation, this might:
                    // - Retry the task
                    // - Create alternative approach tasks
                    // - Escalate to goal replanning
                }
            }
        }
        
        // Update goal achievement if we have an active goal
        if (_current_goal != Handle::UNDEFINED) {
            TruthValuePtr achievement = calculateGoalAchievement(_current_goal);
            if (achievement && achievement->get_mean() > 0.9) {
                logger().info() << "[TaskManager] Goal nearly achieved: " << _current_goal->to_string() 
                               << " (achievement: " << achievement->get_mean() << ")";
                
                // Mark goal as achieved
                Handle achieved_pred = _atomspace->add_node(PREDICATE_NODE, "goal_achieved");
                Handle achieved_eval = _atomspace->add_link(EVALUATION_LINK, {achieved_pred, _current_goal});
                achieved_eval->setTruthValue(achievement);
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[TaskManager] Error in task processing: " << e.what();
        
        // Reset current task on error to avoid getting stuck
        if (_current_task != Handle::UNDEFINED) {
            updateTaskStatus(_current_task, TaskStatus::FAILED);
            _current_task = Handle::UNDEFINED;
        }
        
        return false;
    }
}

// Private implementation methods

void TaskManager::initializeTaskManagement()
{
    logger().debug() << "[TaskManager] Initializing task management structures";
    
    std::string agent_name = _agent_core->getAgentName();
    _task_context = _atomspace->add_node(CONCEPT_NODE, agent_name + "_TaskContext");
    _goal_context = _atomspace->add_node(CONCEPT_NODE, agent_name + "_GoalContext");
    _execution_context = _atomspace->add_node(CONCEPT_NODE, agent_name + "_ExecutionContext");
    _goal_hierarchy_root = _atomspace->add_node(CONCEPT_NODE, agent_name + "_GoalHierarchy");
}

Handle TaskManager::createTaskAtom(const std::string& task_description, Priority priority)
{
    Handle task_atom = _atomspace->add_node(CONCEPT_NODE, "Task_" + task_description);
    
    // Set truth value based on priority
    double priority_strength = static_cast<double>(priority) / 20.0; // Normalize to 0-1
    TruthValuePtr task_tv = SimpleTruthValue::createTV(priority_strength, 0.9);
    task_atom->setTruthValue(task_tv);
    
    return task_atom;
}

Handle TaskManager::createGoalAtom(const std::string& goal_description)
{
    Handle goal_atom = _atomspace->add_node(CONCEPT_NODE, "Goal_" + goal_description);
    
    // Initial goal truth value
    TruthValuePtr goal_tv = SimpleTruthValue::createTV(0.0, 0.9); // Not achieved yet
    goal_atom->setTruthValue(goal_tv);
    
    return goal_atom;
}

bool TaskManager::decomposeGoal(const Handle& goal_atom)
{
    logger().debug() << "[TaskManager] Decomposing goal: " << goal_atom->to_string();
    
    if (goal_atom == Handle::UNDEFINED) {
        logger().error() << "[TaskManager] Cannot decompose undefined goal";
        return false;
    }
    
    // Get goal description from atom name
    std::string goal_name = goal_atom->get_name();
    logger().info() << "[TaskManager] Analyzing goal for decomposition: " << goal_name;
    
    try {
        // Create goal hierarchy in AtomSpace
        Handle goal_hierarchy_link = _atomspace->add_link(LIST_LINK, {_goal_hierarchy_root, goal_atom});
        
        // Enhanced goal decomposition based on goal analysis
        std::vector<std::string> subgoals;
        
        // Analyze goal type and create appropriate subgoals
        if (goal_name.find("learn") != std::string::npos || goal_name.find("study") != std::string::npos) {
            // Learning-oriented goals
            subgoals = {"Identify_Learning_Objectives", "Gather_Resources", "Acquire_Knowledge", "Practice_Skills", "Validate_Understanding"};
        } else if (goal_name.find("solve") != std::string::npos || goal_name.find("problem") != std::string::npos) {
            // Problem-solving goals
            subgoals = {"Define_Problem", "Analyze_Constraints", "Generate_Solutions", "Evaluate_Options", "Implement_Solution", "Test_Result"};
        } else if (goal_name.find("create") != std::string::npos || goal_name.find("build") != std::string::npos) {
            // Creative/construction goals
            subgoals = {"Conceptualize_Design", "Plan_Implementation", "Gather_Resources", "Execute_Construction", "Test_Quality", "Refine_Output"};
        } else if (goal_name.find("communicate") != std::string::npos || goal_name.find("interact") != std::string::npos) {
            // Communication goals
            subgoals = {"Understand_Context", "Plan_Message", "Select_Medium", "Deliver_Communication", "Verify_Understanding"};
        } else {
            // Generic goal decomposition with enhanced structure
            subgoals = {"Analyze_Goal_Context", "Plan_Approach", "Identify_Resources", "Execute_Actions", "Monitor_Progress", "Verify_Achievement"};
        }
        
        // Create subgoals and establish dependencies
        std::vector<Handle> subgoal_atoms;
        for (size_t i = 0; i < subgoals.size(); ++i) {
            Handle subgoal = addSubgoal(goal_atom, subgoals[i]);
            subgoal_atoms.push_back(subgoal);
            
            // Create tasks for each subgoal
            Priority task_priority = (i == 0) ? Priority::HIGH : Priority::MEDIUM;
            Handle task = createTask("Task_" + subgoals[i], task_priority, subgoal);
            
            // Set up sequential dependencies (each task depends on the previous one)
            if (i > 0 && !subgoal_atoms.empty()) {
                Handle prev_task_atom = findTaskForGoal(subgoal_atoms[i-1]);
                if (prev_task_atom != Handle::UNDEFINED) {
                    addTaskDependency(task, prev_task_atom);
                }
            }
        }
        
        // Mark goal as decomposed in AtomSpace
        TruthValuePtr decomposed_tv = SimpleTruthValue::createTV(1.0, 0.9);
        Handle decomposed_pred = _atomspace->add_node(PREDICATE_NODE, "decomposed");
        Handle decomposed_eval = _atomspace->add_link(EVALUATION_LINK, {decomposed_pred, goal_atom});
        decomposed_eval->setTruthValue(decomposed_tv);
        
        logger().info() << "[TaskManager] Successfully decomposed goal into " << subgoals.size() << " subgoals";
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[TaskManager] Error during goal decomposition: " << e.what();
        return false;
    }
}

std::vector<Handle> TaskManager::getReadyTasks()
{
    std::vector<Handle> ready_tasks;
    
    for (const auto& pair : _task_status) {
        if (pair.second == TaskStatus::PENDING && checkTaskDependencies(pair.first)) {
            ready_tasks.push_back(pair.first);
        }
    }
    
    return ready_tasks;
}

bool TaskManager::checkTaskDependencies(const Handle& task_atom)
{
    auto dep_it = _task_dependencies.find(task_atom);
    if (dep_it == _task_dependencies.end()) {
        return true; // No dependencies
    }
    
    for (const Handle& dependency : dep_it->second) {
        TaskStatus dep_status = getTaskStatus(dependency);
        if (dep_status != TaskStatus::COMPLETED) {
            return false; // Dependency not satisfied
        }
    }
    
    return true; // All dependencies satisfied
}

void TaskManager::updateTaskStatus(const Handle& task_atom, TaskStatus status)
{
    _task_status[task_atom] = status;
    
    // Update truth value based on status
    double strength = 0.0;
    switch (status) {
        case TaskStatus::PENDING: strength = 0.2; break;
        case TaskStatus::ACTIVE: strength = 0.5; break;
        case TaskStatus::COMPLETED: strength = 1.0; break;
        case TaskStatus::FAILED: strength = 0.0; break;
        case TaskStatus::CANCELLED: strength = 0.1; break;
        case TaskStatus::SUSPENDED: strength = 0.3; break;
    }
    
    TruthValuePtr status_tv = SimpleTruthValue::createTV(strength, 0.9);
    task_atom->setTruthValue(status_tv);
}

TruthValuePtr TaskManager::calculateGoalAchievement(const Handle& goal_atom)
{
    if (goal_atom == Handle::UNDEFINED) {
        return SimpleTruthValue::createTV(0.0, 0.9);
    }
    
    try {
        logger().debug() << "[TaskManager] Calculating achievement for goal: " << goal_atom->to_string();
        
        // Find all subgoals for this goal using INHERITANCE_LINK
        HandleSeq subgoals;
        HandleSeq all_links = goal_atom->getIncomingSet();
        
        for (const Handle& link : all_links) {
            HandleSeq link_outgoing = link->getOutgoingSet();
            if (link_outgoing.size() == 2 && link_outgoing[0] == goal_atom) {
                subgoals.push_back(link_outgoing[1]);
            }
        }
        
        if (subgoals.empty()) {
            // No subgoals - check if goal has associated completed tasks
            Handle task = findTaskForGoal(goal_atom);
            if (task != Handle::UNDEFINED) {
                TaskStatus status = getTaskStatus(task);
                double achievement = (status == TaskStatus::COMPLETED) ? 1.0 : 
                                   (status == TaskStatus::ACTIVE) ? 0.5 : 0.0;
                return SimpleTruthValue::createTV(achievement, 0.9);
            }
            
            // No tasks either - check goal's current truth value
            TruthValuePtr current_tv = goal_atom->getTruthValue();
            return current_tv ? current_tv : SimpleTruthValue::createTV(0.0, 0.5);
        }
        
        // Calculate achievement based on subgoal completion
        double total_achievement = 0.0;
        double total_confidence = 0.0;
        int completed_subgoals = 0;
        
        for (const Handle& subgoal : subgoals) {
            TruthValuePtr subgoal_achievement = calculateGoalAchievement(subgoal);
            if (subgoal_achievement) {
                double strength = subgoal_achievement->get_mean();
                double confidence = subgoal_achievement->get_confidence();
                
                total_achievement += strength * confidence;
                total_confidence += confidence;
                
                if (strength > 0.8) { // Consider subgoal achieved if > 80%
                    completed_subgoals++;
                }
            }
        }
        
        // Weighted average of subgoal achievements
        double final_achievement = (total_confidence > 0) ? (total_achievement / total_confidence) : 0.0;
        double final_confidence = std::min(0.9, total_confidence / subgoals.size());
        
        // Bonus for having all subgoals completed
        if (completed_subgoals == static_cast<int>(subgoals.size()) && !subgoals.empty()) {
            final_achievement = std::min(1.0, final_achievement + 0.1);
            final_confidence = std::min(0.95, final_confidence + 0.05);
        }
        
        logger().debug() << "[TaskManager] Goal achievement calculated: " << final_achievement 
                        << " (confidence: " << final_confidence << ", " << completed_subgoals 
                        << "/" << subgoals.size() << " subgoals completed)";
        
        return SimpleTruthValue::createTV(final_achievement, final_confidence);
        
    } catch (const std::exception& e) {
        logger().error() << "[TaskManager] Error calculating goal achievement: " << e.what();
        return SimpleTruthValue::createTV(0.0, 0.1);
    }
}

Handle TaskManager::findTaskForGoal(const Handle& goal_atom)
{
    if (goal_atom == Handle::UNDEFINED) {
        return Handle::UNDEFINED;
    }
    
    try {
        // Look for EVALUATION_LINK connecting task to goal  
        HandleSeq all_eval_links;
        _atomspace->get_handles_by_type(all_eval_links, EVALUATION_LINK, true, false, _atomspace.get());
        
        for (const Handle& eval_link : all_eval_links) {
            HandleSeq outgoing = eval_link->getOutgoingSet();
            if (outgoing.size() == 2 && outgoing[1] == goal_atom) {
                // Found a task associated with this goal
                return outgoing[0];
            }
        }
        
        // Alternative: check if goal name corresponds to a task
        std::string goal_name = goal_atom->get_name();
        if (goal_name.find("Goal_") == 0) {
            std::string task_name = "Task_" + goal_name.substr(5); // Remove "Goal_" prefix
            HandleSeq task_candidates;
            Handle task_node = _atomspace->get_node(CONCEPT_NODE, std::move(task_name));
            if (task_node != Handle::UNDEFINED) {
                task_candidates.push_back(task_node);
            }
            if (!task_candidates.empty()) {
                return task_candidates[0];
            }
        }
        
        return Handle::UNDEFINED;
        
    } catch (const std::exception& e) {
        logger().error() << "[TaskManager] Error finding task for goal: " << e.what();
        return Handle::UNDEFINED;
    }
}

// Enhanced GoalHierarchy management methods implementation

std::vector<Handle> TaskManager::getSubgoals(const Handle& parent_goal) const
{
    std::vector<Handle> subgoals;
    
    if (parent_goal == Handle::UNDEFINED) {
        logger().error() << "[TaskManager] Cannot get subgoals of undefined parent goal";
        return subgoals;
    }
    
    try {
        HandleSeq all_links;
        _atomspace->get_handles_by_type(all_links, INHERITANCE_LINK);
        
        for (const Handle& link : all_links) {
            HandleSeq link_outgoing = link->getOutgoingSet();
            if (link_outgoing.size() == 2 && link_outgoing[1] == parent_goal) {
                subgoals.push_back(link_outgoing[0]);
            }
        }
        
        logger().debug() << "[TaskManager] Found " << subgoals.size() 
                        << " subgoals for parent: " << parent_goal->get_name();
        
    } catch (const std::exception& e) {
        logger().error() << "[TaskManager] Error getting subgoals: " << e.what();
    }
    
    return subgoals;
}

Handle TaskManager::getParentGoal(const Handle& subgoal) const
{
    if (subgoal == Handle::UNDEFINED) {
        logger().error() << "[TaskManager] Cannot get parent of undefined subgoal";
        return Handle::UNDEFINED;
    }
    
    try {
        HandleSeq all_links;
        _atomspace->get_handles_by_type(all_links, INHERITANCE_LINK);
        
        for (const Handle& link : all_links) {
            HandleSeq link_outgoing = link->getOutgoingSet();
            if (link_outgoing.size() == 2 && link_outgoing[0] == subgoal) {
                logger().debug() << "[TaskManager] Found parent goal: " 
                                << link_outgoing[1]->get_name() 
                                << " for subgoal: " << subgoal->get_name();
                return link_outgoing[1];
            }
        }
        
        logger().debug() << "[TaskManager] No parent goal found for: " << subgoal->get_name();
        
    } catch (const std::exception& e) {
        logger().error() << "[TaskManager] Error getting parent goal: " << e.what();
    }
    
    return Handle::UNDEFINED;
}

std::vector<Handle> TaskManager::getGoalAncestors(const Handle& goal) const
{
    std::vector<Handle> ancestors;
    
    if (goal == Handle::UNDEFINED) {
        return ancestors;
    }
    
    try {
        Handle current = goal;
        Handle parent = getParentGoal(current);
        
        while (parent != Handle::UNDEFINED) {
            ancestors.push_back(parent);
            current = parent;
            parent = getParentGoal(current);
            
            // Prevent infinite loops in case of circular hierarchies
            if (ancestors.size() > 50) {
                logger().warn() << "[TaskManager] Goal hierarchy too deep, stopping ancestor search";
                break;
            }
        }
        
        logger().debug() << "[TaskManager] Found " << ancestors.size() 
                        << " ancestors for goal: " << goal->get_name();
        
    } catch (const std::exception& e) {
        logger().error() << "[TaskManager] Error getting goal ancestors: " << e.what();
    }
    
    return ancestors;
}

TruthValuePtr TaskManager::calculateHierarchicalGoalAchievement(const Handle& goal)
{
    if (goal == Handle::UNDEFINED) {
        logger().error() << "[TaskManager] Cannot calculate achievement for undefined goal";
        return SimpleTruthValue::createTV(0.0, 0.0);
    }
    
    try {
        // Get the goal's own achievement level
        TruthValuePtr goal_tv = goal->getTruthValue();
        double goal_achievement = goal_tv ? goal_tv->get_mean() : 0.0;
        double goal_confidence = goal_tv ? goal_tv->get_confidence() : 0.1;
        
        // Get all subgoals
        std::vector<Handle> subgoals = getSubgoals(goal);
        
        if (subgoals.empty()) {
            // Leaf goal - return its own achievement
            logger().debug() << "[TaskManager] Leaf goal achievement: " 
                            << goal_achievement << " for " << goal->get_name();
            return SimpleTruthValue::createTV(goal_achievement, goal_confidence);
        }
        
        // Calculate weighted average of subgoal achievements
        double total_subgoal_achievement = 0.0;
        double total_weight = 0.0;
        double min_confidence = 1.0;
        
        for (const Handle& subgoal : subgoals) {
            TruthValuePtr subgoal_tv = calculateHierarchicalGoalAchievement(subgoal);
            double subgoal_achievement = subgoal_tv->get_mean();
            double subgoal_confidence = subgoal_tv->get_confidence();
            
            // Weight by confidence
            total_subgoal_achievement += subgoal_achievement * subgoal_confidence;
            total_weight += subgoal_confidence;
            min_confidence = std::min(min_confidence, subgoal_confidence);
        }
        
        double hierarchical_achievement = total_weight > 0 ? 
            total_subgoal_achievement / total_weight : 0.0;
        
        // Combine goal's own achievement (30%) with subgoal achievement (70%)
        double final_achievement = 0.3 * goal_achievement + 0.7 * hierarchical_achievement;
        double final_confidence = std::min(goal_confidence, min_confidence * 0.9);
        
        logger().debug() << "[TaskManager] Hierarchical achievement for " << goal->get_name() 
                        << ": " << final_achievement << " (confidence: " << final_confidence << ")";
        
        // Update the goal's truth value with hierarchical achievement
        TruthValuePtr hierarchical_tv = SimpleTruthValue::createTV(final_achievement, final_confidence);
        goal->setTruthValue(hierarchical_tv);
        
        return hierarchical_tv;
        
    } catch (const std::exception& e) {
        logger().error() << "[TaskManager] Error calculating hierarchical goal achievement: " << e.what();
        return SimpleTruthValue::createTV(0.0, 0.1);
    }
}

bool TaskManager::propagateGoalPriority(const Handle& goal, Priority priority)
{
    if (goal == Handle::UNDEFINED) {
        logger().error() << "[TaskManager] Cannot propagate priority for undefined goal";
        return false;
    }
    
    try {
        logger().debug() << "[TaskManager] Propagating priority " << static_cast<int>(priority) 
                        << " from goal: " << goal->get_name();
        
        // Get all subgoals
        std::vector<Handle> subgoals = getSubgoals(goal);
        
        for (const Handle& subgoal : subgoals) {
            // Propagate priority to subgoal (but reduce it slightly)
            Priority subgoal_priority = static_cast<Priority>(
                std::max(1, static_cast<int>(priority) - 1)
            );
            
            // Update priority metadata in AtomSpace
            Handle priority_pred = _atomspace->add_node(PREDICATE_NODE, "goal_priority");
            Handle priority_value = _atomspace->add_node(NUMBER_NODE, 
                std::to_string(static_cast<int>(subgoal_priority)));
            Handle priority_eval = _atomspace->add_link(EVALUATION_LINK, 
                {priority_pred, subgoal, priority_value});
            
            TruthValuePtr priority_tv = SimpleTruthValue::createTV(
                static_cast<double>(subgoal_priority) / 20.0, 0.9);
            priority_eval->setTruthValue(priority_tv);
            
            // Recursively propagate to deeper levels
            propagateGoalPriority(subgoal, subgoal_priority);
            
            logger().debug() << "[TaskManager] Set priority " << static_cast<int>(subgoal_priority) 
                            << " for subgoal: " << subgoal->get_name();
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[TaskManager] Error propagating goal priority: " << e.what();
        return false;
    }
}

bool TaskManager::synchronizeGoalHierarchy(const Handle& goal)
{
    if (goal == Handle::UNDEFINED) {
        logger().error() << "[TaskManager] Cannot synchronize undefined goal";
        return false;
    }
    
    try {
        logger().debug() << "[TaskManager] Synchronizing goal hierarchy from: " << goal->get_name();
        
        // Get current goal achievement
        TruthValuePtr goal_tv = calculateHierarchicalGoalAchievement(goal);
        double goal_achievement = goal_tv->get_mean();
        
        // Synchronize parent goals if this goal is highly achieved
        if (goal_achievement > 0.8) {
            Handle parent = getParentGoal(goal);
            if (parent != Handle::UNDEFINED) {
                synchronizeGoalHierarchy(parent);
            }
        }
        
        // Synchronize subgoals
        std::vector<Handle> subgoals = getSubgoals(goal);
        for (const Handle& subgoal : subgoals) {
            synchronizeGoalHierarchy(subgoal);
        }
        
        // Add synchronization timestamp
        Handle sync_pred = _atomspace->add_node(PREDICATE_NODE, "hierarchy_synchronized");
        Handle sync_time = _atomspace->add_node(NUMBER_NODE, std::to_string(std::time(nullptr)));
        Handle sync_eval = _atomspace->add_link(EVALUATION_LINK, {sync_pred, goal, sync_time});
        sync_eval->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));
        
        logger().debug() << "[TaskManager] Synchronized goal hierarchy for: " << goal->get_name();
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[TaskManager] Error synchronizing goal hierarchy: " << e.what();
        return false;
    }
}

std::vector<Handle> TaskManager::getLeafGoals(const Handle& root_goal) const
{
    std::vector<Handle> leaf_goals;
    Handle target_goal = (root_goal != Handle::UNDEFINED) ? root_goal : _current_goal;
    
    if (target_goal == Handle::UNDEFINED) {
        logger().warn() << "[TaskManager] No root goal specified and no current goal set";
        return leaf_goals;
    }
    
    try {
        std::vector<Handle> subgoals = getSubgoals(target_goal);
        
        if (subgoals.empty()) {
            // This is a leaf goal
            leaf_goals.push_back(target_goal);
        } else {
            // Recursively find leaf goals in subgoals
            for (const Handle& subgoal : subgoals) {
                std::vector<Handle> subgoal_leaves = getLeafGoals(subgoal);
                leaf_goals.insert(leaf_goals.end(), subgoal_leaves.begin(), subgoal_leaves.end());
            }
        }
        
        logger().debug() << "[TaskManager] Found " << leaf_goals.size() 
                        << " leaf goals under: " << target_goal->get_name();
        
    } catch (const std::exception& e) {
        logger().error() << "[TaskManager] Error getting leaf goals: " << e.what();
    }
    
    return leaf_goals;
}

int TaskManager::getGoalHierarchyDepth(const Handle& goal) const
{
    Handle target_goal = (goal != Handle::UNDEFINED) ? goal : _current_goal;
    
    if (target_goal == Handle::UNDEFINED) {
        return 0;
    }
    
    try {
        std::vector<Handle> subgoals = getSubgoals(target_goal);
        
        if (subgoals.empty()) {
            return 1; // Leaf goal has depth 1
        }
        
        int max_subgoal_depth = 0;
        for (const Handle& subgoal : subgoals) {
            int subgoal_depth = getGoalHierarchyDepth(subgoal);
            max_subgoal_depth = std::max(max_subgoal_depth, subgoal_depth);
        }
        
        int total_depth = 1 + max_subgoal_depth;
        logger().debug() << "[TaskManager] Goal hierarchy depth for " << target_goal->get_name() 
                        << ": " << total_depth;
        
        return total_depth;
        
    } catch (const std::exception& e) {
        logger().error() << "[TaskManager] Error calculating goal hierarchy depth: " << e.what();
        return 0;
    }
}

bool TaskManager::removeGoalFromHierarchy(const Handle& goal, bool preserve_orphans)
{
    if (goal == Handle::UNDEFINED) {
        logger().error() << "[TaskManager] Cannot remove undefined goal from hierarchy";
        return false;
    }
    
    try {
        logger().info() << "[TaskManager] Removing goal from hierarchy: " << goal->get_name() 
                       << " (preserve_orphans: " << (preserve_orphans ? "true" : "false") << ")";
        
        // Get subgoals before removing the goal
        std::vector<Handle> subgoals = getSubgoals(goal);
        Handle parent = getParentGoal(goal);
        
        // Handle orphaned subgoals
        if (preserve_orphans && parent != Handle::UNDEFINED) {
            // Reconnect subgoals to parent
            for (const Handle& subgoal : subgoals) {
                addSubgoal(parent, subgoal->get_name());
            }
            logger().debug() << "[TaskManager] Reconnected " << subgoals.size() 
                            << " orphaned subgoals to parent";
        } else if (!preserve_orphans) {
            // Recursively remove all subgoals
            for (const Handle& subgoal : subgoals) {
                removeGoalFromHierarchy(subgoal, false);
            }
            logger().debug() << "[TaskManager] Removed " << subgoals.size() << " subgoals";
        }
        
        // Remove all links involving this goal
        HandleSeq all_links;
        _atomspace->get_handles_by_type(all_links, INHERITANCE_LINK);
        
        for (const Handle& link : all_links) {
            HandleSeq link_outgoing = link->getOutgoingSet();
            if (link_outgoing.size() == 2 && 
                (link_outgoing[0] == goal || link_outgoing[1] == goal)) {
                _atomspace->remove_atom(link);
            }
        }
        
        // Remove goal metadata
        HandleSeq eval_links;
        _atomspace->get_handles_by_type(eval_links, EVALUATION_LINK);
        
        for (const Handle& eval : eval_links) {
            HandleSeq eval_outgoing = eval->getOutgoingSet();
            if (eval_outgoing.size() >= 2) {
                for (size_t i = 1; i < eval_outgoing.size(); ++i) {
                    if (eval_outgoing[i] == goal) {
                        _atomspace->remove_atom(eval);
                        break;
                    }
                }
            }
        }
        
        // Clear from current goal if it matches
        if (_current_goal == goal) {
            _current_goal = Handle::UNDEFINED;
            logger().info() << "[TaskManager] Cleared current goal reference";
        }
        
        logger().info() << "[TaskManager] Successfully removed goal from hierarchy: " 
                       << goal->get_name();
                return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[TaskManager] Error removing goal from hierarchy: " << e.what();
        return false;
    }
}