/*
 * src/ActionExecutor.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Action Execution Framework Implementation
 * Core action execution system for Agent-Zero with AtomSpace integration
 * Part of AZ-ACTION-002: Create action execution framework
 */

#include <sstream>
#include <algorithm>
#include <ctime>
#include <thread>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

#include "opencog/agentzero/ActionExecutor.h"
#include "opencog/agentzero/ActionScheduler.h"
#include "opencog/agentzero/AgentZeroCore.h"

using namespace opencog;
using namespace opencog::agentzero;

ActionExecutor::ActionExecutor(AgentZeroCore* agent_core, AtomSpacePtr atomspace)
    : _agent_core(agent_core)
    , _atomspace(atomspace)
    , _scheduler(nullptr)
    , _action_context(Handle::UNDEFINED)
    , _execution_context(Handle::UNDEFINED)
    , _outcome_context(Handle::UNDEFINED)
    , _error_context(Handle::UNDEFINED)
    , _max_concurrent_actions(5)
    , _default_timeout(std::chrono::milliseconds(30000))  // 30 second default
    , _enable_async_execution(true)
    , _enable_action_monitoring(true)
{
    logger().info() << "[ActionExecutor] Constructor: Initializing action execution framework";
    initializeActionSystem();
}

ActionExecutor::~ActionExecutor()
{
    logger().info() << "[ActionExecutor] Destructor: Cleaning up action execution framework";
    
    // Cancel all pending actions
    std::queue<Handle> empty_queue;
    std::swap(_action_queue, empty_queue);
    
    // Clear action status tracking
    _action_status.clear();
    _action_results.clear();
    _active_actions.clear();
    _action_registry.clear();
}

void ActionExecutor::initializeActionSystem()
{
    logger().debug() << "[ActionExecutor] Initializing action system";
    
    try {
        // Create context atoms in AtomSpace
        _action_context = _atomspace->add_node(CONCEPT_NODE, std::string("ActionContext"));
        _execution_context = _atomspace->add_node(CONCEPT_NODE, std::string("ExecutionContext"));
        _outcome_context = _atomspace->add_node(CONCEPT_NODE, std::string("OutcomeContext"));
        _error_context = _atomspace->add_node(CONCEPT_NODE, std::string("ErrorContext"));
        
        // Set initial truth values
        TruthValuePtr action_tv = SimpleTruthValue::createTV(0.8, 0.9);
        _action_context->setTruthValue(action_tv);
        _execution_context->setTruthValue(action_tv);
        _outcome_context->setTruthValue(action_tv);
        _error_context->setTruthValue(action_tv);
        
        // Register built-in action types
        registerAction("basic", [this](const Handle& action_atom, const std::map<std::string, Handle>& params) {
            return executeBasicAction(action_atom, params);
        });
        
        registerAction("atomspace", [this](const Handle& action_atom, const std::map<std::string, Handle>& params) {
            return executeAtomSpaceAction(action_atom, params);
        });
        
        registerAction("composite", [this](const Handle& action_atom, const std::map<std::string, Handle>& params) {
            return executeCompositeAction(action_atom, params);
        });
        
        logger().info() << "[ActionExecutor] Action system initialized successfully";
        
    } catch (const std::exception& e) {
        logger().error() << "[ActionExecutor] Failed to initialize action system: " << e.what();
        throw;
    }
}

bool ActionExecutor::executeAction(const Handle& action_atom, Priority priority)
{
    logger().debug() << "[ActionExecutor] Queueing action for execution: " << action_atom;
    
    if (!validateActionAtom(action_atom)) {
        logger().error() << "[ActionExecutor] Invalid action atom: " << action_atom;
        return false;
    }
    
    // Check if we've reached maximum queue size
    if (_action_queue.size() >= static_cast<size_t>(_max_concurrent_actions * 2)) {
        logger().warn() << "[ActionExecutor] Action queue is full, rejecting new action";
        return false;
    }
    
    // Add to queue and set initial status
    _action_queue.push(action_atom);
    _action_priorities[action_atom] = priority;
    updateActionStatus(action_atom, ActionStatus::PENDING);
    
    // Create action tracking atom in AtomSpace
    HandleSeq action_link;
    action_link.push_back(action_atom);
    action_link.push_back(_action_context);
    Handle tracking_atom = _atomspace->add_link(EVALUATION_LINK, std::move(action_link));
    TruthValuePtr pending_tv = SimpleTruthValue::createTV(0.1, 0.8);  // Low strength = pending
    tracking_atom->setTruthValue(pending_tv);
    
    logger().info() << "[ActionExecutor] Action queued successfully: " << action_atom;
    return true;
}

ActionExecutor::ActionResult ActionExecutor::executeActionSync(const Handle& action_atom, int timeout_ms)
{
    logger().debug() << "[ActionExecutor] Executing action synchronously: " << action_atom;
    
    ActionResult result;
    auto start_time = std::chrono::steady_clock::now();
    
    if (!validateActionAtom(action_atom)) {
        result.status = ActionStatus::FAILED;
        result.message = "Invalid action atom";
        return result;
    }
    
    // Execute the action directly
    updateActionStatus(action_atom, ActionStatus::EXECUTING);
    result = executeActionInternal(action_atom);
    
    // Calculate duration
    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Record outcome
    recordActionOutcome(action_atom, result);
    
    logger().info() << "[ActionExecutor] Synchronous action completed: " 
                   << action_atom << " Status: " << static_cast<int>(result.status);
    
    return result;
}

Handle ActionExecutor::executeSimpleAction(const std::string& action_type,
                                          const std::map<std::string, Handle>& parameters,
                                          Priority priority)
{
    logger().debug() << "[ActionExecutor] Creating simple action: " << action_type;
    
    // Create action atom
    Handle action_atom = createActionAtom(action_type, parameters);
    
    if (action_atom == Handle::UNDEFINED) {
        logger().error() << "[ActionExecutor] Failed to create action atom for type: " << action_type;
        return Handle::UNDEFINED;
    }
    
    // Execute the action
    if (executeAction(action_atom, priority)) {
        return action_atom;
    } else {
        logger().error() << "[ActionExecutor] Failed to execute simple action: " << action_type;
        return Handle::UNDEFINED;
    }
}

bool ActionExecutor::cancelAction(const Handle& action_atom)
{
    logger().debug() << "[ActionExecutor] Cancelling action: " << action_atom;
    
    auto status_iter = _action_status.find(action_atom);
    if (status_iter == _action_status.end()) {
        logger().warn() << "[ActionExecutor] Action not found for cancellation: " << action_atom;
        return false;
    }
    
    ActionStatus current_status = status_iter->second;
    
    // Can only cancel pending or executing actions
    if (current_status != ActionStatus::PENDING && current_status != ActionStatus::EXECUTING) {
        logger().warn() << "[ActionExecutor] Cannot cancel action in status: " << static_cast<int>(current_status);
        return false;
    }
    
    // Update status to cancelled
    updateActionStatus(action_atom, ActionStatus::CANCELLED);
    
    // Remove from active actions if executing
    _active_actions.erase(action_atom);
    
    // Record cancellation outcome
    ActionResult result;
    result.status = ActionStatus::CANCELLED;
    result.message = "Action cancelled by user request";
    recordActionOutcome(action_atom, result);
    
    logger().info() << "[ActionExecutor] Action cancelled successfully: " << action_atom;
    return true;
}

ActionExecutor::ActionStatus ActionExecutor::getActionStatus(const Handle& action_atom) const
{
    auto iter = _action_status.find(action_atom);
    if (iter != _action_status.end()) {
        return iter->second;
    }
    return ActionStatus::PENDING;  // Default status
}

ActionExecutor::ActionResult ActionExecutor::getActionResult(const Handle& action_atom) const
{
    auto iter = _action_results.find(action_atom);
    if (iter != _action_results.end()) {
        return iter->second;
    }
    return ActionResult();  // Empty result
}

bool ActionExecutor::registerAction(const std::string& action_type, ActionCallback callback)
{
    logger().debug() << "[ActionExecutor] Registering action type: " << action_type;
    
    if (callback == nullptr) {
        logger().error() << "[ActionExecutor] Invalid callback for action type: " << action_type;
        return false;
    }
    
    _action_registry[action_type] = callback;
    
    logger().info() << "[ActionExecutor] Action type registered: " << action_type;
    return true;
}

bool ActionExecutor::unregisterAction(const std::string& action_type)
{
    logger().debug() << "[ActionExecutor] Unregistering action type: " << action_type;
    
    auto iter = _action_registry.find(action_type);
    if (iter == _action_registry.end()) {
        logger().warn() << "[ActionExecutor] Action type not found: " << action_type;
        return false;
    }
    
    _action_registry.erase(iter);
    
    logger().info() << "[ActionExecutor] Action type unregistered: " << action_type;
    return true;
}

bool ActionExecutor::isActionRegistered(const std::string& action_type) const
{
    return _action_registry.find(action_type) != _action_registry.end();
}

int ActionExecutor::processActionQueue()
{
    if (!_enable_async_execution) {
        return 0;
    }
    
    int actions_processed = 0;
    int current_executing = 0;
    
    // Count currently executing actions
    for (const auto& pair : _action_status) {
        if (pair.second == ActionStatus::EXECUTING) {
            current_executing++;
        }
    }
    
    // Process queue while we have capacity
    while (!_action_queue.empty() && current_executing < _max_concurrent_actions) {
        Handle action_atom = _action_queue.front();
        _action_queue.pop();
        
        // Skip if action was cancelled while in queue
        if (getActionStatus(action_atom) == ActionStatus::CANCELLED) {
            continue;
        }
        
        logger().debug() << "[ActionExecutor] Starting async execution of: " << action_atom;
        
        // Start async execution
        updateActionStatus(action_atom, ActionStatus::EXECUTING);
        _action_start_times[action_atom] = std::chrono::steady_clock::now();
        
        // Execute in separate thread (simplified for this implementation)
        std::thread([this, action_atom]() {
            try {
                ActionResult result = executeActionInternal(action_atom);
                recordActionOutcome(action_atom, result);
                _active_actions.erase(action_atom);
            } catch (const std::exception& e) {
                handleActionError(action_atom, e.what());
                _active_actions.erase(action_atom);
            }
        }).detach();
        
        current_executing++;
        actions_processed++;
    }
    
    return actions_processed;
}

int ActionExecutor::monitorExecutingActions()
{
    if (!_enable_action_monitoring) {
        return 0;
    }
    
    int status_changes = 0;
    auto now = std::chrono::steady_clock::now();
    
    // Check for timeouts
    for (auto iter = _action_start_times.begin(); iter != _action_start_times.end(); ) {
        Handle action_atom = iter->first;
        auto start_time = iter->second;
        
        if (getActionStatus(action_atom) != ActionStatus::EXECUTING) {
            iter = _action_start_times.erase(iter);
            continue;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
        if (elapsed > _default_timeout) {
            logger().warn() << "[ActionExecutor] Action timeout: " << action_atom;
            
            updateActionStatus(action_atom, ActionStatus::TIMEOUT);
            _active_actions.erase(action_atom);
            
            ActionResult result;
            result.status = ActionStatus::TIMEOUT;
            result.message = "Action exceeded timeout limit";
            result.duration = elapsed;
            recordActionOutcome(action_atom, result);
            
            status_changes++;
            iter = _action_start_times.erase(iter);
        } else {
            ++iter;
        }
    }
    
    return status_changes;
}

std::vector<Handle> ActionExecutor::getExecutingActions() const
{
    std::vector<Handle> executing;
    for (const auto& pair : _action_status) {
        if (pair.second == ActionStatus::EXECUTING) {
            executing.push_back(pair.first);
        }
    }
    return executing;
}

std::vector<Handle> ActionExecutor::getPendingActions() const
{
    std::vector<Handle> pending;
    for (const auto& pair : _action_status) {
        if (pair.second == ActionStatus::PENDING) {
            pending.push_back(pair.first);
        }
    }
    return pending;
}

std::string ActionExecutor::getStatusInfo() const
{
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"action_queue_size\": " << _action_queue.size() << ",\n";
    oss << "  \"total_tracked_actions\": " << _action_status.size() << ",\n";
    oss << "  \"max_concurrent_actions\": " << _max_concurrent_actions << ",\n";
    oss << "  \"async_execution_enabled\": " << (_enable_async_execution ? "true" : "false") << ",\n";
    oss << "  \"monitoring_enabled\": " << (_enable_action_monitoring ? "true" : "false") << ",\n";
    oss << "  \"registered_action_types\": " << _action_registry.size() << ",\n";
    oss << "  \"executing_actions\": " << getExecutingActions().size() << ",\n";
    oss << "  \"pending_actions\": " << getPendingActions().size() << "\n";
    oss << "}";
    return oss.str();
}

// Private helper methods

Handle ActionExecutor::createActionAtom(const std::string& action_type, 
                                       const std::map<std::string, Handle>& parameters)
{
    try {
        // Create action type node
        Handle action_type_node = _atomspace->add_node(CONCEPT_NODE, std::string(action_type));
        
        // Create parameter nodes and links
        HandleSeq param_links;
        for (const auto& param : parameters) {
            Handle param_name = _atomspace->add_node(CONCEPT_NODE, std::string(param.first));
            HandleSeq param_pair;
            param_pair.push_back(param_name);
            param_pair.push_back(param.second);
            Handle param_link = _atomspace->add_link(LIST_LINK, std::move(param_pair));
            param_links.push_back(param_link);
        }
        
        // Create main action atom
        HandleSeq action_content;
        action_content.push_back(action_type_node);
        if (!param_links.empty()) {
            Handle params_list = _atomspace->add_link(LIST_LINK, std::move(param_links));
            action_content.push_back(params_list);
        }
        
        Handle action_atom = _atomspace->add_link(EXECUTION_LINK, std::move(action_content));
        
        // Set initial truth value
        TruthValuePtr action_tv = SimpleTruthValue::createTV(0.7, 0.8);
        action_atom->setTruthValue(action_tv);
        
        return action_atom;
        
    } catch (const std::exception& e) {
        logger().error() << "[ActionExecutor] Failed to create action atom: " << e.what();
        return Handle::UNDEFINED;
    }
}

bool ActionExecutor::validateActionAtom(const Handle& action_atom)
{
    if (action_atom == Handle::UNDEFINED) {
        return false;
    }
    
    if (!_atomspace->is_valid_handle(action_atom)) {
        return false;
    }
    
    // Additional validation could be added here
    // For now, just check that it's a valid atom in our atomspace
    
    return true;
}

ActionExecutor::ActionResult ActionExecutor::executeActionInternal(const Handle& action_atom)
{
    logger().debug() << "[ActionExecutor] Executing action internally: " << action_atom;
    
    ActionResult result;
    
    try {
        // Extract action type and parameters from atom
        std::string action_type = "basic";  // Default type
        std::map<std::string, Handle> parameters;
        
        // Try to extract action type from the atom structure
        if (action_atom->is_link()) {
            const HandleSeq& outgoing = action_atom->getOutgoingSet();
            if (!outgoing.empty() && outgoing[0]->is_node()) {
                action_type = outgoing[0]->get_name();
            }
        }
        
        // Find registered action callback
        auto callback_iter = _action_registry.find(action_type);
        if (callback_iter != _action_registry.end()) {
            result = callback_iter->second(action_atom, parameters);
        } else {
            // Use basic action as fallback
            result = executeBasicAction(action_atom, parameters);
        }
        
    } catch (const std::exception& e) {
        result.status = ActionStatus::FAILED;
        result.message = std::string("Action execution error: ") + e.what();
        logger().error() << "[ActionExecutor] Action execution failed: " << e.what();
    }
    
    return result;
}

void ActionExecutor::updateActionStatus(const Handle& action_atom, ActionStatus status)
{
    _action_status[action_atom] = status;
    
    // Update AtomSpace representation
    try {
        HandleSeq status_link;
        status_link.push_back(action_atom);
        status_link.push_back(_atomspace->add_node(CONCEPT_NODE, std::string("Status" + std::to_string(static_cast<int>(status)))));
        Handle status_atom = _atomspace->add_link(EVALUATION_LINK, std::move(status_link));
        
        // Set truth value based on status
        double strength = 0.5;
        switch (status) {
            case ActionStatus::COMPLETED:
                strength = 1.0;
                break;
            case ActionStatus::FAILED:
            case ActionStatus::CANCELLED:
            case ActionStatus::TIMEOUT:
                strength = 0.0;
                break;
            case ActionStatus::EXECUTING:
                strength = 0.7;
                break;
            case ActionStatus::PENDING:
                strength = 0.3;
                break;
            default:
                strength = 0.5;
        }
        
        TruthValuePtr status_tv = SimpleTruthValue::createTV(strength, 0.9);
        status_atom->setTruthValue(status_tv);
        
    } catch (const std::exception& e) {
        logger().warn() << "[ActionExecutor] Failed to update AtomSpace status: " << e.what();
    }
}

void ActionExecutor::recordActionOutcome(const Handle& action_atom, const ActionResult& result)
{
    _action_results[action_atom] = result;
    
    // Record in AtomSpace
    try {
        HandleSeq outcome_link;
        outcome_link.push_back(action_atom);
        outcome_link.push_back(_outcome_context);
        Handle outcome_atom = _atomspace->add_link(EVALUATION_LINK, std::move(outcome_link));
        
        TruthValuePtr outcome_tv = SimpleTruthValue::createTV(result.success_probability, 0.9);
        outcome_atom->setTruthValue(outcome_tv);
        
    } catch (const std::exception& e) {
        logger().warn() << "[ActionExecutor] Failed to record outcome: " << e.what();
    }
}

void ActionExecutor::handleActionError(const Handle& action_atom, const std::string& error_message)
{
    logger().error() << "[ActionExecutor] Action error: " << action_atom << " - " << error_message;
    
    updateActionStatus(action_atom, ActionStatus::FAILED);
    
    ActionResult result;
    result.status = ActionStatus::FAILED;
    result.message = error_message;
    recordActionOutcome(action_atom, result);
    
    // Record error in AtomSpace
    try {
        Handle error_node = _atomspace->add_node(CONCEPT_NODE, std::string(error_message));
        HandleSeq error_link;
        error_link.push_back(action_atom);
        error_link.push_back(error_node);
        Handle error_atom = _atomspace->add_link(EVALUATION_LINK, std::move(error_link));
        
        TruthValuePtr error_tv = SimpleTruthValue::createTV(0.0, 0.9);  // Low strength indicates error
        error_atom->setTruthValue(error_tv);
        
    } catch (const std::exception& e) {
        logger().warn() << "[ActionExecutor] Failed to record error: " << e.what();
    }
}

void ActionExecutor::cleanupCompletedActions()
{
    // Remove completed actions older than a threshold to prevent memory leaks
    // This is a simplified implementation - could be more sophisticated
    
    auto now = std::chrono::steady_clock::now();
    const auto cleanup_threshold = std::chrono::minutes(10);
    
    for (auto iter = _action_results.begin(); iter != _action_results.end(); ) {
        const ActionResult& result = iter->second;
        
        if (result.status == ActionStatus::COMPLETED || 
            result.status == ActionStatus::FAILED ||
            result.status == ActionStatus::CANCELLED) {
            
            // Simple cleanup logic - remove after threshold
            // In a real implementation, this would check actual completion time
            ++iter;  // Keep for now - implement proper timestamp tracking later
        } else {
            ++iter;
        }
    }
}

// Built-in action implementations

ActionExecutor::ActionResult ActionExecutor::executeBasicAction(const Handle& action_atom, 
                                                              const std::map<std::string, Handle>& parameters)
{
    logger().debug() << "[ActionExecutor] Executing basic action: " << action_atom;
    
    ActionResult result;
    
    // Basic action just succeeds after a short delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    result.status = ActionStatus::COMPLETED;
    result.message = "Basic action completed successfully";
    result.success_probability = 0.9;
    result.outcome_atom = _atomspace->add_node(CONCEPT_NODE, std::string("BasicActionSuccess"));
    
    return result;
}

ActionExecutor::ActionResult ActionExecutor::executeAtomSpaceAction(const Handle& action_atom, 
                                                                   const std::map<std::string, Handle>& parameters)
{
    logger().debug() << "[ActionExecutor] Executing AtomSpace action: " << action_atom;
    
    ActionResult result;
    
    try {
        // Example AtomSpace operation - create a new concept
        Handle concept = _atomspace->add_node(CONCEPT_NODE, std::string("ActionGeneratedConcept"));
        TruthValuePtr concept_tv = SimpleTruthValue::createTV(0.8, 0.7);
        concept->setTruthValue(concept_tv);
        
        result.status = ActionStatus::COMPLETED;
        result.message = "AtomSpace action completed successfully";
        result.success_probability = 0.85;
        result.outcome_atom = concept;
        
    } catch (const std::exception& e) {
        result.status = ActionStatus::FAILED;
        result.message = std::string("AtomSpace action failed: ") + e.what();
        result.success_probability = 0.0;
    }
    
    return result;
}

ActionExecutor::ActionResult ActionExecutor::executeCompositeAction(const Handle& action_atom, 
                                                                   const std::map<std::string, Handle>& parameters)
{
    logger().debug() << "[ActionExecutor] Executing composite action: " << action_atom;
    
    ActionResult result;
    
    // Composite action executes multiple sub-actions
    // This is a simplified implementation
    
    try {
        // Execute sub-actions in sequence
        for (int i = 0; i < 3; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            // Simulate sub-action execution
            Handle sub_action = _atomspace->add_node(CONCEPT_NODE, std::string("SubAction" + std::to_string(i)));
            TruthValuePtr sub_tv = SimpleTruthValue::createTV(0.7, 0.8);
            sub_action->setTruthValue(sub_tv);
        }
        
        result.status = ActionStatus::COMPLETED;
        result.message = "Composite action completed successfully";
        result.success_probability = 0.8;
        result.outcome_atom = _atomspace->add_node(CONCEPT_NODE, std::string("CompositeActionSuccess"));
        
    } catch (const std::exception& e) {
        result.status = ActionStatus::FAILED;
        result.message = std::string("Composite action failed: ") + e.what();
        result.success_probability = 0.0;
    }
    
    return result;
}