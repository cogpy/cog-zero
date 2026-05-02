/*
 * opencog/agentzero/ActionExecutor.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Action Execution Framework
 * Core action execution system for Agent-Zero with AtomSpace integration
 * Part of AZ-ACTION-002: Create action execution framework
 */

#ifndef _OPENCOG_AGENTZERO_ACTION_EXECUTOR_H
#define _OPENCOG_AGENTZERO_ACTION_EXECUTOR_H

#include <memory>
#include <vector>
#include <queue>
#include <string>
#include <map>
#include <functional>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {

class AgentZeroCore;
class ActionScheduler;

/**
 * ActionExecutor - Core action execution framework for Agent-Zero
 *
 * This class provides comprehensive action execution capabilities with
 * AtomSpace integration, status tracking, and error handling. It works
 * in conjunction with ActionScheduler for temporal coordination.
 *
 * Key features:
 * - AtomSpace-based action representation
 * - Asynchronous action execution
 * - Status monitoring and error handling  
 * - Action outcome tracking
 * - Integration with cognitive loop
 */
class ActionExecutor
{
public:
    // Action execution states
    enum class ActionStatus {
        PENDING,      // Action created but not started
        EXECUTING,    // Action currently being executed
        COMPLETED,    // Action finished successfully
        FAILED,       // Action failed to execute
        CANCELLED,    // Action was cancelled
        TIMEOUT,      // Action exceeded time limit
        SUSPENDED     // Action temporarily suspended
    };
    
    // Action priority levels
    enum class Priority {
        LOW = 1,
        MEDIUM = 5,
        HIGH = 10,
        CRITICAL = 20
    };
    
    // Action execution result
    struct ActionResult {
        ActionStatus status;
        std::string message;
        Handle outcome_atom;
        std::chrono::milliseconds duration;
        double success_probability;
        
        ActionResult() 
            : status(ActionStatus::PENDING)
            , duration(0)
            , success_probability(0.0) {}
    };
    
    // Action execution callback function type
    using ActionCallback = std::function<ActionResult(const Handle& action_atom, const std::map<std::string, Handle>& parameters)>;

private:
    // Core references
    AgentZeroCore* _agent_core;
    AtomSpacePtr _atomspace;
    std::shared_ptr<ActionScheduler> _scheduler;
    
    // Action management
    std::queue<Handle> _action_queue;
    std::map<Handle, ActionStatus> _action_status;
    std::map<Handle, Priority> _action_priorities;
    std::map<Handle, ActionResult> _action_results;
    std::map<Handle, std::chrono::steady_clock::time_point> _action_start_times;
    
    // Action registry and callbacks
    std::map<std::string, ActionCallback> _action_registry;
    std::map<Handle, ActionCallback> _active_actions;
    
    // AtomSpace handles for action management
    Handle _action_context;
    Handle _execution_context;
    Handle _outcome_context;
    Handle _error_context;
    
    // Configuration
    int _max_concurrent_actions;
    std::chrono::milliseconds _default_timeout;
    bool _enable_async_execution;
    bool _enable_action_monitoring;
    
    // Internal methods
    void initializeActionSystem();
    Handle createActionAtom(const std::string& action_type, 
                           const std::map<std::string, Handle>& parameters);
    bool validateActionAtom(const Handle& action_atom);
    ActionResult executeActionInternal(const Handle& action_atom);
    void updateActionStatus(const Handle& action_atom, ActionStatus status);
    void recordActionOutcome(const Handle& action_atom, const ActionResult& result);
    void handleActionError(const Handle& action_atom, const std::string& error_message);
    void cleanupCompletedActions();
    
    // Built-in action implementations
    ActionResult executeBasicAction(const Handle& action_atom, const std::map<std::string, Handle>& parameters);
    ActionResult executeAtomSpaceAction(const Handle& action_atom, const std::map<std::string, Handle>& parameters);
    ActionResult executeCompositeAction(const Handle& action_atom, const std::map<std::string, Handle>& parameters);

public:
    /**
     * Constructor
     * @param agent_core Pointer to the parent AgentZeroCore instance
     * @param atomspace Shared pointer to the AtomSpace
     */
    ActionExecutor(AgentZeroCore* agent_core, AtomSpacePtr atomspace);
    
    /**
     * Destructor - ensures cleanup of resources
     */
    ~ActionExecutor();
    
    // Action execution interface
    /**
     * Execute an action asynchronously
     * @param action_atom Handle to the action atom
     * @param priority Priority level for execution
     * @return true if action was queued successfully
     */
    bool executeAction(const Handle& action_atom, Priority priority = Priority::MEDIUM);
    
    /**
     * Execute an action synchronously
     * @param action_atom Handle to the action atom  
     * @param timeout_ms Maximum execution time in milliseconds
     * @return ActionResult with execution outcome
     */
    ActionResult executeActionSync(const Handle& action_atom, int timeout_ms = 5000);
    
    /**
     * Create and execute a simple action
     * @param action_type Type of action to execute
     * @param parameters Action parameters as Handle map
     * @param priority Priority level for execution
     * @return Handle to the created action atom
     */
    Handle executeSimpleAction(const std::string& action_type,
                              const std::map<std::string, Handle>& parameters = {},
                              Priority priority = Priority::MEDIUM);
    
    /**
     * Cancel a pending or executing action
     * @param action_atom Handle to the action atom to cancel
     * @return true if action was cancelled successfully
     */
    bool cancelAction(const Handle& action_atom);
    
    /**
     * Get the current status of an action
     * @param action_atom Handle to the action atom
     * @return ActionStatus enum value
     */
    ActionStatus getActionStatus(const Handle& action_atom) const;
    
    /**
     * Get the result of a completed action
     * @param action_atom Handle to the action atom
     * @return ActionResult struct with outcome details
     */
    ActionResult getActionResult(const Handle& action_atom) const;
    
    // Action registry management
    /**
     * Register a custom action callback
     * @param action_type String identifier for the action type
     * @param callback Function to execute for this action type
     * @return true if registration was successful
     */
    bool registerAction(const std::string& action_type, ActionCallback callback);
    
    /**
     * Unregister an action callback
     * @param action_type String identifier for the action type
     * @return true if unregistration was successful
     */
    bool unregisterAction(const std::string& action_type);
    
    /**
     * Check if an action type is registered
     * @param action_type String identifier for the action type
     * @return true if action type is registered
     */
    bool isActionRegistered(const std::string& action_type) const;
    
    // Processing and monitoring
    /**
     * Process the action queue (called by cognitive loop)
     * @return number of actions processed
     */
    int processActionQueue();
    
    /**
     * Monitor executing actions for completion/timeout
     * @return number of actions that changed status
     */
    int monitorExecutingActions();
    
    /**
     * Get list of currently executing actions
     * @return vector of action atom handles
     */
    std::vector<Handle> getExecutingActions() const;
    
    /**
     * Get list of pending actions in queue
     * @return vector of action atom handles
     */
    std::vector<Handle> getPendingActions() const;
    
    // Configuration
    /**
     * Set maximum number of concurrent actions
     * @param max_concurrent Maximum concurrent actions allowed
     */
    void setMaxConcurrentActions(int max_concurrent) { 
        _max_concurrent_actions = max_concurrent; 
    }
    
    /**
     * Set default action timeout
     * @param timeout_ms Default timeout in milliseconds
     */
    void setDefaultTimeout(int timeout_ms) { 
        _default_timeout = std::chrono::milliseconds(timeout_ms); 
    }
    
    /**
     * Enable or disable asynchronous action execution
     * @param enable true to enable async execution
     */
    void setAsyncExecution(bool enable) { 
        _enable_async_execution = enable; 
    }
    
    // AtomSpace integration
    /**
     * Get the action context atom
     * @return Handle to action context
     */
    Handle getActionContext() const { return _action_context; }
    
    /**
     * Get the execution context atom
     * @return Handle to execution context
     */
    Handle getExecutionContext() const { return _execution_context; }
    
    /**
     * Get status information for debugging
     * @return JSON string with status details
     */
    std::string getStatusInfo() const;
    
    // Scheduler integration
    /**
     * Set the action scheduler instance
     * @param scheduler Shared pointer to ActionScheduler
     */
    void setScheduler(std::shared_ptr<ActionScheduler> scheduler) { 
        _scheduler = scheduler; 
    }
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_ACTION_EXECUTOR_H