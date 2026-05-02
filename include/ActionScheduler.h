/*
 * opencog/agentzero/ActionScheduler.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Action Scheduler for Temporal Coordination
 * Temporal coordination and scheduling system for Agent-Zero actions
 * Part of AZ-ACTION-001: Implement ActionScheduler for temporal coordination
 */

#ifndef _OPENCOG_AGENTZERO_ACTION_SCHEDULER_H
#define _OPENCOG_AGENTZERO_ACTION_SCHEDULER_H

#include <memory>
#include <vector>
#include <queue>
#include <string>
#include <map>
#include <chrono>
#include <functional>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>
#include <octomap/octomap.h>

namespace opencog {
namespace agentzero {

class AgentZeroCore;
class ActionExecutor;
class SpaceTimeIntegrator;

/**
 * ActionScheduler - Temporal coordination system for Agent-Zero actions
 *
 * This class provides comprehensive temporal scheduling and coordination
 * of actions with AtomSpace integration. It handles timing constraints,
 * dependencies, and resource management for action execution.
 *
 * Key features:
 * - Temporal action scheduling with precise timing
 * - Action dependency management
 * - Resource conflict resolution
 * - Priority-based scheduling
 * - AtomSpace integration for temporal reasoning
 */
class ActionScheduler
{
public:
    // Scheduling constraint types
    enum class ConstraintType {
        TIME_ABSOLUTE,     // Execute at specific time
        TIME_RELATIVE,     // Execute after delay from now
        TIME_DEADLINE,     // Must complete before deadline
        DEPENDENCY,        // Wait for other actions to complete
        RESOURCE,          // Require exclusive resource access
        CONDITION,         // Wait for AtomSpace condition
        PERIODIC           // Execute repeatedly with interval
    };
    
    // Scheduling result
    enum class ScheduleResult {
        SCHEDULED,         // Successfully scheduled
        CONFLICT,          // Resource or time conflict
        DEPENDENCY_UNMET,  // Dependencies not satisfied
        INVALID_CONSTRAINT,// Invalid scheduling constraint
        QUEUE_FULL         // Schedule queue is full
    };
    
    // Scheduled action entry
    struct ScheduledAction {
        Handle action_atom;
        std::chrono::steady_clock::time_point scheduled_time;
        std::chrono::steady_clock::time_point deadline;
        std::vector<Handle> dependencies;
        std::vector<std::string> required_resources;
        int priority;
        bool is_periodic;
        std::chrono::milliseconds period;
        int repeat_count;
        
        // Spatial-temporal extensions for AZ-SPATIAL-001
        bool has_spatial_constraints;
        std::vector<octomap::point3d> required_locations;
        std::vector<octomap::point3d> trajectory_points;
        Handle spatial_context;
        double spatial_tolerance;
        
        ScheduledAction() 
            : priority(5), is_periodic(false), period(0), repeat_count(1),
              has_spatial_constraints(false), spatial_tolerance(0.1) {}
    };

private:
    // Core references
    AgentZeroCore* _agent_core;
    AtomSpacePtr _atomspace;
    std::shared_ptr<ActionExecutor> _executor;
    
    // Scheduling structures
    std::priority_queue<ScheduledAction> _schedule_queue;
    std::map<Handle, ScheduledAction> _scheduled_actions;
    std::map<std::string, Handle> _resource_locks;
    std::map<Handle, std::vector<Handle>> _dependency_graph;
    
    // Current execution state
    std::vector<Handle> _executing_actions;
    std::vector<std::string> _available_resources;
    std::chrono::steady_clock::time_point _last_update;
    
    // AtomSpace handles for scheduling
    Handle _schedule_context;
    Handle _temporal_context;
    Handle _dependency_context;
    Handle _resource_context;
    
    // Configuration
    int _max_scheduled_actions;
    std::chrono::milliseconds _scheduling_resolution;
    bool _enable_resource_management;
    bool _enable_dependency_checking;
    bool _enable_temporal_reasoning;
    bool _enable_spatial_temporal_planning;  // AZ-SPATIAL-001 extension
    
    // Spatial-temporal integration (AZ-SPATIAL-001)
    std::shared_ptr<SpaceTimeIntegrator> _spacetime_integrator;
    
    // Internal methods
    void initializeScheduler();
    bool validateSchedulingConstraints(const ScheduledAction& action);
    bool checkResourceAvailability(const std::vector<std::string>& resources);
    bool checkDependencies(const std::vector<Handle>& dependencies);
    std::vector<ScheduledAction> getReadyActions();
    void updateResourceLocks(const Handle& action_atom, bool acquire);
    void updateDependencyGraph(const Handle& action_atom, const std::vector<Handle>& deps);
    Handle createScheduleAtom(const ScheduledAction& action);
    void recordSchedulingDecision(const Handle& action_atom, ScheduleResult result);
    
    // Spatial-temporal helper methods (AZ-SPATIAL-001)
    bool validateSpatialConstraints(const ScheduledAction& action);
    bool checkLocationAvailability(const std::vector<octomap::point3d>& locations,
                                  const std::chrono::steady_clock::time_point& start_time,
                                  const std::chrono::steady_clock::time_point& end_time);
    void initializeSpaceTimeIntegration();
    Handle createSpatialScheduleAtom(const ScheduledAction& action);

public:
    /**
     * Constructor
     * @param agent_core Pointer to the parent AgentZeroCore instance
     * @param atomspace Shared pointer to the AtomSpace
     */
    ActionScheduler(AgentZeroCore* agent_core, AtomSpacePtr atomspace);
    
    /**
     * Destructor - ensures cleanup of resources
     */
    ~ActionScheduler();
    
    // Core scheduling interface
    /**
     * Schedule an action with timing constraints
     * @param action_atom Handle to the action atom
     * @param scheduled_time When to execute the action
     * @param priority Priority level (1-20)
     * @return ScheduleResult indicating success or failure reason
     */
    ScheduleResult scheduleAction(const Handle& action_atom,
                                 const std::chrono::steady_clock::time_point& scheduled_time,
                                 int priority = 5);
    
    /**
     * Schedule an action with relative delay
     * @param action_atom Handle to the action atom
     * @param delay_ms Delay in milliseconds from now
     * @param priority Priority level (1-20)
     * @return ScheduleResult indicating success or failure reason
     */
    ScheduleResult scheduleActionAfter(const Handle& action_atom,
                                      int delay_ms,
                                      int priority = 5);
    
    /**
     * Schedule an action with deadline constraint
     * @param action_atom Handle to the action atom
     * @param deadline When action must complete by
     * @param priority Priority level (1-20)
     * @return ScheduleResult indicating success or failure reason
     */
    ScheduleResult scheduleActionBefore(const Handle& action_atom,
                                       const std::chrono::steady_clock::time_point& deadline,
                                       int priority = 5);
    
    /**
     * Schedule a periodic action
     * @param action_atom Handle to the action atom
     * @param period_ms Period between executions in milliseconds
     * @param repeat_count Number of repetitions (-1 for infinite)
     * @param priority Priority level (1-20)
     * @return ScheduleResult indicating success or failure reason
     */
    ScheduleResult schedulePeriodicAction(const Handle& action_atom,
                                         int period_ms,
                                         int repeat_count = -1,
                                         int priority = 5);
    
    // Dependency and resource management
    /**
     * Schedule action with dependencies
     * @param action_atom Handle to the action atom
     * @param dependencies Vector of action atoms that must complete first
     * @param priority Priority level (1-20)
     * @return ScheduleResult indicating success or failure reason
     */
    ScheduleResult scheduleActionWithDependencies(const Handle& action_atom,
                                                 const std::vector<Handle>& dependencies,
                                                 int priority = 5);
    
    /**
     * Schedule action with resource requirements
     * @param action_atom Handle to the action atom
     * @param resources Vector of resource names required
     * @param priority Priority level (1-20)
     * @return ScheduleResult indicating success or failure reason
     */
    ScheduleResult scheduleActionWithResources(const Handle& action_atom,
                                              const std::vector<std::string>& resources,
                                              int priority = 5);
    
    // Spatial-temporal scheduling methods (AZ-SPATIAL-001)
    /**
     * Schedule an action with spatial constraints
     * @param action_atom Handle to the action atom
     * @param locations Required spatial locations for the action
     * @param scheduled_time When to execute the action
     * @param spatial_tolerance Distance tolerance in meters
     * @param priority Priority level (1-20)
     * @return ScheduleResult indicating success or failure reason
     */
    ScheduleResult scheduleActionWithSpatialConstraints(const Handle& action_atom,
                                                       const std::vector<octomap::point3d>& locations,
                                                       const std::chrono::steady_clock::time_point& scheduled_time,
                                                       double spatial_tolerance = 0.1,
                                                       int priority = 5);
    
    /**
     * Schedule an action with a trajectory
     * @param action_atom Handle to the action atom
     * @param trajectory_points Sequence of spatial points to follow
     * @param start_time When to start the trajectory
     * @param priority Priority level (1-20)
     * @return ScheduleResult indicating success or failure reason
     */
    ScheduleResult scheduleActionWithTrajectory(const Handle& action_atom,
                                               const std::vector<octomap::point3d>& trajectory_points,
                                               const std::chrono::steady_clock::time_point& start_time,
                                               int priority = 5);
    
    /**
     * Schedule an action with optimal spatial-temporal planning
     * Uses SpaceTimeIntegrator to find optimal time and location
     * @param action_atom Handle to the action atom
     * @param preferred_locations Preferred spatial locations (in order of preference)
     * @param earliest_start Earliest allowable start time
     * @param latest_end Latest allowable end time
     * @param priority Priority level (1-20)
     * @return ScheduleResult indicating success or failure reason
     */
    ScheduleResult scheduleActionWithOptimalPlanning(const Handle& action_atom,
                                                    const std::vector<octomap::point3d>& preferred_locations,
                                                    const std::chrono::steady_clock::time_point& earliest_start,
                                                    const std::chrono::steady_clock::time_point& latest_end,
                                                    int priority = 5);
    
    /**
     * Cancel a scheduled action
     * @param action_atom Handle to the action atom to cancel
     * @return true if action was cancelled successfully
     */
    bool cancelScheduledAction(const Handle& action_atom);
    
    /**
     * Reschedule an existing action
     * @param action_atom Handle to the action atom
     * @param new_time New scheduled time
     * @return ScheduleResult indicating success or failure reason
     */
    ScheduleResult rescheduleAction(const Handle& action_atom,
                                   const std::chrono::steady_clock::time_point& new_time);
    
    // Processing and monitoring
    /**
     * Process the schedule queue and dispatch ready actions
     * @return number of actions dispatched
     */
    int processScheduleQueue();
    
    /**
     * Update scheduler state and check for ready actions
     * @return number of actions that became ready
     */
    int updateScheduler();
    
    /**
     * Get list of currently scheduled actions
     * @return vector of scheduled action entries
     */
    std::vector<ScheduledAction> getScheduledActions() const;
    
    /**
     * Get next scheduled action time
     * @return time point of next action, or time_point::max() if none
     */
    std::chrono::steady_clock::time_point getNextActionTime() const;
    
    // Resource management
    /**
     * Register a resource for management
     * @param resource_name Name of the resource
     * @return true if resource was registered successfully
     */
    bool registerResource(const std::string& resource_name);
    
    /**
     * Unregister a resource
     * @param resource_name Name of the resource
     * @return true if resource was unregistered successfully
     */
    bool unregisterResource(const std::string& resource_name);
    
    /**
     * Check if a resource is currently available
     * @param resource_name Name of the resource
     * @return true if resource is available
     */
    bool isResourceAvailable(const std::string& resource_name) const;
    
    /**
     * Get list of available resources
     * @return vector of resource names
     */
    std::vector<std::string> getAvailableResources() const;
    
    // Configuration
    /**
     * Set maximum number of scheduled actions
     * @param max_actions Maximum actions in schedule queue
     */
    void setMaxScheduledActions(int max_actions) { 
        _max_scheduled_actions = max_actions; 
    }
    
    /**
     * Set scheduling time resolution
     * @param resolution_ms Time resolution in milliseconds
     */
    void setSchedulingResolution(int resolution_ms) { 
        _scheduling_resolution = std::chrono::milliseconds(resolution_ms); 
    }
    
    /**
     * Configure scheduler features
     * @param resources Enable resource management
     * @param dependencies Enable dependency checking
     * @param temporal Enable temporal reasoning
     */
    void configureFeatures(bool resources, bool dependencies, bool temporal);
    
    /**
     * Configure spatial-temporal planning features (AZ-SPATIAL-001)
     * @param spatial_temporal Enable spatial-temporal planning
     * @param spatial_resolution Spatial resolution in meters
     * @param time_resolution Time resolution in milliseconds
     */
    void configureSpatialTemporalFeatures(bool spatial_temporal, 
                                         double spatial_resolution = 0.1,
                                         int time_resolution_ms = 100);
    
    /**
     * Get the SpaceTimeIntegrator instance
     * @return Shared pointer to SpaceTimeIntegrator or nullptr if not enabled
     */
    std::shared_ptr<SpaceTimeIntegrator> getSpaceTimeIntegrator() const { return _spacetime_integrator; }
    
    // AtomSpace integration
    /**
     * Get the schedule context atom
     * @return Handle to schedule context
     */
    Handle getScheduleContext() const { return _schedule_context; }
    
    /**
     * Get the temporal context atom
     * @return Handle to temporal context
     */
    Handle getTemporalContext() const { return _temporal_context; }
    
    /**
     * Get status information for debugging
     * @return JSON string with status details
     */
    std::string getStatusInfo() const;
    
    // Executor integration
    /**
     * Set the action executor instance
     * @param executor Shared pointer to ActionExecutor
     */
    void setExecutor(std::shared_ptr<ActionExecutor> executor) { 
        _executor = executor; 
    }
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_ACTION_SCHEDULER_H