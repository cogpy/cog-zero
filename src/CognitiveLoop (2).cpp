/*
 * src/CognitiveLoop.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Cognitive Loop Implementation
 * Implements perception-action-reflection cycle with AtomSpace integration
 * Part of the AGENT-ZERO-GENESIS project
 */

#include <sstream>
#include <thread>
#include <chrono>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

// Attention system includes (conditionally available)
#ifdef HAVE_ATTENTION_BANK
#include <opencog/attentionbank/bank/AttentionBank.h>
#include <opencog/attentionbank/avalue/AttentionValue.h>
#include <opencog/attentionbank/bank/AVUtils.h>
// Function to get attention bank
namespace opencog { 
    AttentionBank& attentionbank(AtomSpace* as);
}
#endif

#include "opencog/agentzero/CognitiveLoop.h"
#include "opencog/agentzero/AgentZeroCore.h"
#include "opencog/agentzero/TaskManager.h"
#include "opencog/agentzero/KnowledgeIntegrator.h"
#include "opencog/agentzero/ActionExecutor.h"
#include "opencog/agentzero/ActionScheduler.h"

using namespace opencog;
using namespace opencog::agentzero;

CognitiveLoop::CognitiveLoop(AgentZeroCore* agent_core, AtomSpacePtr atomspace)
    : _agent_core(agent_core)
    , _atomspace(atomspace)
    , _action_executor(nullptr)
    , _action_scheduler(nullptr)
    , _running(false)
    , _paused(false)
    , _cycle_interval(std::chrono::milliseconds(1000)) // Default 1 second
    , _cycle_count(0)
    , _last_cycle_duration_ms(0)
    , _perception_context(Handle::UNDEFINED)
    , _planning_context(Handle::UNDEFINED)
    , _action_context(Handle::UNDEFINED)
    , _reflection_context(Handle::UNDEFINED)
    , _enable_perception(true)
    , _enable_planning(true)
    , _enable_action(true)
    , _enable_reflection(true)
    , _enable_attention_allocation(false)
    , _attention_bank(nullptr)
    , _attention_context(Handle::UNDEFINED)
    , _perception_importance_threshold(0.5)
    , _attention_spreading_factor(0.1)
    , _action_scheduler(nullptr)
{
    logger().info() << "[CognitiveLoop] Constructor: Initializing cognitive loop";
    
    // Create context atoms for each cognitive phase
    std::string agent_name = _agent_core->getAgentName();
    _perception_context = _atomspace->add_node(CONCEPT_NODE, agent_name + "_Perception");
    _planning_context = _atomspace->add_node(CONCEPT_NODE, agent_name + "_Planning");
    _action_context = _atomspace->add_node(CONCEPT_NODE, agent_name + "_Action");
    _reflection_context = _atomspace->add_node(CONCEPT_NODE, agent_name + "_Reflection");
    _attention_context = _atomspace->add_node(CONCEPT_NODE, agent_name + "_Attention");
    
    // Initialize attention system if available
    initializeAttentionSystem();
    
    // Initialize action execution components
    _action_executor = std::make_shared<ActionExecutor>(_agent_core, _atomspace);
    _action_scheduler = std::make_shared<ActionScheduler>(_agent_core, _atomspace);
    
    // Cross-reference the components
    _action_executor->setScheduler(_action_scheduler);
    _action_scheduler->setExecutor(_action_executor);
    
    logger().debug() << "[CognitiveLoop] Context atoms and action components created";
}

CognitiveLoop::~CognitiveLoop()
{
    logger().info() << "[CognitiveLoop] Destructor: Cleaning up cognitive loop";
    
    // Ensure loop is stopped before destruction
    if (_running.load()) {
        stop();
    }
}

bool CognitiveLoop::start()
{
    if (_running.load()) {
        logger().warn() << "[CognitiveLoop] Already running";
        return true;
    }
    
    logger().info() << "[CognitiveLoop] Starting cognitive loop";
    
    try {
        _running = true;
        _paused = false;
        
        // Start the main loop in a separate thread
        _loop_thread = std::make_unique<std::thread>(&CognitiveLoop::runMainLoop, this);
        
        logger().info() << "[CognitiveLoop] Cognitive loop started successfully";
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[CognitiveLoop] Failed to start: " << e.what();
        _running = false;
        return false;
    }
}

bool CognitiveLoop::stop()
{
    if (!_running.load()) {
        logger().warn() << "[CognitiveLoop] Not running";
        return true;
    }
    
    logger().info() << "[CognitiveLoop] Stopping cognitive loop";
    
    _running = false;
    _paused = false;
    
    // Wait for thread to complete
    if (_loop_thread && _loop_thread->joinable()) {
        _loop_thread->join();
        _loop_thread.reset();
    }
    
    logger().info() << "[CognitiveLoop] Cognitive loop stopped successfully";
    return true;
}

bool CognitiveLoop::pause()
{
    if (!_running.load()) {
        logger().warn() << "[CognitiveLoop] Cannot pause: not running";
        return false;
    }
    
    if (_paused.load()) {
        logger().warn() << "[CognitiveLoop] Already paused";
        return true;
    }
    
    logger().info() << "[CognitiveLoop] Pausing cognitive loop";
    _paused = true;
    return true;
}

bool CognitiveLoop::resume()
{
    if (!_running.load()) {
        logger().warn() << "[CognitiveLoop] Cannot resume: not running";
        return false;
    }
    
    if (!_paused.load()) {
        logger().warn() << "[CognitiveLoop] Not paused";
        return true;
    }
    
    logger().info() << "[CognitiveLoop] Resuming cognitive loop";
    _paused = false;
    return true;
}

bool CognitiveLoop::executeSingleCycle()
{
    logger().debug() << "[CognitiveLoop] Executing single cognitive cycle";
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        bool cycle_success = true;
        
        // Execute each cognitive phase if enabled
        if (_enable_perception) {
            cycle_success &= executePerceptionPhase();
        }
        
        if (_enable_planning) {
            cycle_success &= executePlanningPhase();
        }
        
        if (_enable_action) {
            cycle_success &= executeActionPhase();
        }
        
        if (_enable_reflection) {
            cycle_success &= executeReflectionPhase();
        }
        
        // Update cycle metrics
        updateCycleMetrics(start_time);
        
        // Increment cycle count
        _cycle_count++;
        
        logger().debug() << "[CognitiveLoop] Cycle " << _cycle_count << " completed in " 
                        << _last_cycle_duration_ms << "ms";
        
        return cycle_success;
        
    } catch (const std::exception& e) {
        handleLoopException(e);
        return false;
    }
}

void CognitiveLoop::configurePhases(bool perception, bool planning, bool action, bool reflection)
{
    logger().info() << "[CognitiveLoop] Configuring phases: perception=" << perception 
                   << ", planning=" << planning << ", action=" << action 
                   << ", reflection=" << reflection;
    
    _enable_perception = perception;
    _enable_planning = planning;
    _enable_action = action;
    _enable_reflection = reflection;
}

void CognitiveLoop::configureAttention(bool enable, double importance_threshold, double spreading_factor)
{
    logger().info() << "[CognitiveLoop] Configuring attention: enable=" << enable
                   << ", importance_threshold=" << importance_threshold
                   << ", spreading_factor=" << spreading_factor;
    
    _enable_attention_allocation = enable;
    _perception_importance_threshold = importance_threshold;
    _attention_spreading_factor = spreading_factor;
    
    if (enable && !_attention_bank) {
        initializeAttentionSystem();
    }
}

std::string CognitiveLoop::getStatusInfo() const
{
    std::ostringstream status;
    status << "{";
    status << "\"running\":" << (_running.load() ? "true" : "false") << ",";
    status << "\"paused\":" << (_paused.load() ? "true" : "false") << ",";
    status << "\"cycle_count\":" << _cycle_count.load() << ",";
    status << "\"last_cycle_duration_ms\":" << _last_cycle_duration_ms.load() << ",";
    status << "\"cycle_interval_ms\":" << _cycle_interval.count() << ",";
    status << "\"perception_enabled\":" << (_enable_perception ? "true" : "false") << ",";
    status << "\"planning_enabled\":" << (_enable_planning ? "true" : "false") << ",";
    status << "\"action_enabled\":" << (_enable_action ? "true" : "false") << ",";
    status << "\"reflection_enabled\":" << (_enable_reflection ? "true" : "false") << ",";
    status << "\"attention_allocation_enabled\":" << (_enable_attention_allocation ? "true" : "false") << ",";
    status << "\"attention_bank_available\":" << (_attention_bank ? "true" : "false");
    status << "}";
    return status.str();
}

// Private implementation methods

void CognitiveLoop::runMainLoop()
{
    logger().info() << "[CognitiveLoop] Main loop thread started";
    
    while (_running.load()) {
        try {
            // Check if paused
            if (_paused.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            // Execute one cognitive cycle
            executeSingleCycle();
            
            // Sleep for the specified interval
            std::this_thread::sleep_for(_cycle_interval);
            
        } catch (const std::exception& e) {
            handleLoopException(e);
            
            // Brief pause before retrying
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    logger().info() << "[CognitiveLoop] Main loop thread terminated";
}

bool CognitiveLoop::executePerceptionPhase()
{
    logger().debug() << "[CognitiveLoop] Executing perception phase";
    
    try {
        // Update perception context with current timestamp
        TruthValuePtr perception_tv = SimpleTruthValue::createTV(0.8, 0.9);
        _perception_context->setTruthValue(perception_tv);
        
        // Basic perception processing - in a full implementation this would:
        // - Process sensory inputs
        // - Update world model in AtomSpace
        // - Detect changes and events
        // - Update attention allocation
        
        // Create some example percepts for demonstration
        HandleSeq percepts;
        
        // Record that perception occurred
        HandleSeq perception_link;
        perception_link.push_back(_agent_core->getAgentSelfAtom());
        perception_link.push_back(_perception_context);
        Handle perception_event = _atomspace->add_link(EVALUATION_LINK, std::move(perception_link));
        percepts.push_back(perception_event);
        
        // Add perception context to percepts for attention allocation
        percepts.push_back(_perception_context);
        
        // Integrate ECAN attention allocation for perceived items
        if (_enable_attention_allocation) {
            logger().debug() << "[CognitiveLoop] Allocating attention to " << percepts.size() << " percepts";
            allocateAttentionToPercepts(percepts);
            
            // Get high-importance atoms to focus cognitive processing
            HandleSeq important_atoms = getHighImportanceAtoms();
            if (!important_atoms.empty()) {
                logger().debug() << "[CognitiveLoop] Found " << important_atoms.size() 
                               << " high-importance atoms for focused processing";
                
                // In a full implementation, these important atoms would guide
                // further perception processing and attention spreading
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[CognitiveLoop] Perception phase error: " << e.what();
        return false;
    }
}

bool CognitiveLoop::executePlanningPhase()
{
    logger().debug() << "[CognitiveLoop] Executing planning phase";
    
    try {
        // Update planning context
        TruthValuePtr planning_tv = SimpleTruthValue::createTV(0.7, 0.8);
        _planning_context->setTruthValue(planning_tv);
        
        // Basic planning processing - in a full implementation this would:
        // - Analyze current goals
        // - Generate action plans
        // - Evaluate plan feasibility
        // - Select optimal actions
        
        // Delegate to TaskManager if available
        if (_agent_core->getTaskManager()) {
            return _agent_core->getTaskManager()->processTaskManagement();
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[CognitiveLoop] Planning phase error: " << e.what();
        return false;
    }
}

bool CognitiveLoop::executeActionPhase()
{
    logger().debug() << "[CognitiveLoop] Executing action phase";
    
    try {
        // Update action context
        TruthValuePtr action_tv = SimpleTruthValue::createTV(0.6, 0.7);
        _action_context->setTruthValue(action_tv);
        
        // Process action execution through the ActionExecutor and ActionScheduler
        if (_action_executor) {
            // Process scheduled actions
            int scheduled_actions = 0;
            if (_action_scheduler) {
                scheduled_actions = _action_scheduler->processScheduleQueue();
                _action_scheduler->updateScheduler();
            }
            
            // Process action queue
            int queued_actions = _action_executor->processActionQueue();
            
            // Monitor executing actions
            int status_changes = _action_executor->monitorExecutingActions();
            
            logger().debug() << "[CognitiveLoop] Action phase processed: " 
                           << scheduled_actions << " scheduled, "
                           << queued_actions << " queued, "
                           << status_changes << " status changes";
        } else if (_action_scheduler && _action_scheduler->isEnabled()) {
            // Fallback to ActionScheduler-only mode for backward compatibility
            bool action_success = _action_scheduler->processScheduleQueue() > 0;
            _action_scheduler->updateScheduler();
            
            logger().debug() << "[CognitiveLoop] ActionScheduler processed actions";
        }
        
        // Delegate to TaskManager for additional processing
        if (_agent_core->getTaskManager()) {
            _agent_core->getTaskManager()->processTaskManagement();
        }
        
        // Record that action phase occurred in AtomSpace
        HandleSeq action_link;
        action_link.push_back(_agent_core->getAgentSelfAtom());
        action_link.push_back(_action_context);
        _atomspace->add_link(EVALUATION_LINK, std::move(action_link));
        
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[CognitiveLoop] Action phase error: " << e.what();
        return false;
    }
}

bool CognitiveLoop::executeReflectionPhase()
{
    logger().debug() << "[CognitiveLoop] Executing reflection phase";
    
    try {
        // Update reflection context
        TruthValuePtr reflection_tv = SimpleTruthValue::createTV(0.5, 0.6);
        _reflection_context->setTruthValue(reflection_tv);
        
        // Basic reflection processing - in a full implementation this would:
        // - Analyze action outcomes
        // - Update knowledge base
        // - Learn from experience
        // - Adjust future behavior
        
        // Delegate to KnowledgeIntegrator if available
        if (_agent_core->getKnowledgeIntegrator()) {
            return _agent_core->getKnowledgeIntegrator()->processKnowledgeIntegration();
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[CognitiveLoop] Reflection phase error: " << e.what();
        return false;
    }
}

void CognitiveLoop::updateCycleMetrics(const std::chrono::steady_clock::time_point& start_time)
{
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    _last_cycle_duration_ms = duration.count();
}

void CognitiveLoop::handleLoopException(const std::exception& e)
{
    logger().error() << "[CognitiveLoop] Exception in main loop: " << e.what();
    
    // In a production system, might want to:
    // - Record error statistics
    // - Attempt recovery actions
    // - Notify monitoring systems
    // - Adjust loop parameters
}

// Attention allocation implementation

void CognitiveLoop::initializeAttentionSystem()
{
    logger().debug() << "[CognitiveLoop] Initializing attention system";
    
    try {
#ifdef HAVE_ATTENTION_BANK
        // Try to get the attention bank for this atomspace
        _attention_bank = &opencog::attentionbank(_atomspace.get());
        if (_attention_bank) {
            logger().info() << "[CognitiveLoop] Attention bank initialized successfully";
            _enable_attention_allocation = true;
        }
#else
        logger().warn() << "[CognitiveLoop] Attention system not available - compiled without attention support";
#endif
    } catch (const std::exception& e) {
        logger().warn() << "[CognitiveLoop] Failed to initialize attention system: " << e.what();
        _attention_bank = nullptr;
        _enable_attention_allocation = false;
    }
}

void CognitiveLoop::allocateAttentionToPercepts(const HandleSeq& percepts)
{
    if (!_enable_attention_allocation || !_attention_bank || percepts.empty()) {
        return;
    }
    
    logger().debug() << "[CognitiveLoop] Allocating attention to " << percepts.size() << " percepts";
    
    try {
#ifdef HAVE_ATTENTION_BANK
        // Calculate attention values for percepts based on novelty and importance
        for (const Handle& percept : percepts) {
            if (percept == Handle::UNDEFINED) continue;
            
            // Get current attention value using AVUtils
            AttentionValuePtr current_av = get_av(percept);
            
            // Calculate new importance based on perception factors
            // This is a simplified heuristic - in a full system this would be more sophisticated
            double base_importance = current_av ? current_av->getSTI() : 0.0;
            double novelty_boost = 10.0; // New percepts get importance boost
            double perception_importance = base_importance + novelty_boost;
            
            // Ensure we're above the threshold
            if (perception_importance < _perception_importance_threshold * 100.0) {
                perception_importance = _perception_importance_threshold * 100.0;
            }
            
            // Create new attention value
            AttentionValuePtr new_av = AttentionValue::createAV(
                static_cast<AttentionValue::sti_t>(perception_importance),  // STI (short-term importance)
                current_av ? current_av->getLTI() : 0,                      // LTI (long-term importance)  
                current_av ? current_av->getVLTI() : 0                      // VLTI (very long-term importance)
            );
            
            // Update the atom's attention value using AVUtils
            set_av(_atomspace.get(), percept, new_av);
            
            logger().debug() << "[CognitiveLoop] Set attention for percept: " 
                           << percept->to_short_string() 
                           << " STI=" << perception_importance;
        }
        
        // Update attention context
        updateAttentionContext();
#endif
        
    } catch (const std::exception& e) {
        logger().error() << "[CognitiveLoop] Error in attention allocation: " << e.what();
    }
}

void CognitiveLoop::updateAttentionContext()
{
    if (!_enable_attention_allocation || _attention_context == Handle::UNDEFINED) {
        return;
    }
    
    try {
        // Update the attention context with current attention metrics
        TruthValuePtr attention_tv = SimpleTruthValue::createTV(0.9, 0.95);
        _attention_context->setTruthValue(attention_tv);
        
        // Record that attention allocation occurred
        HandleSeq attention_link;
        attention_link.push_back(_agent_core->getAgentSelfAtom());
        attention_link.push_back(_attention_context);
        _atomspace->add_link(EVALUATION_LINK, std::move(attention_link));
        
    } catch (const std::exception& e) {
        logger().error() << "[CognitiveLoop] Error updating attention context: " << e.what();
    }
}

HandleSeq CognitiveLoop::getHighImportanceAtoms(double threshold) const
{
    HandleSeq high_importance_atoms;
    
    if (!_enable_attention_allocation || !_attention_bank) {
        return high_importance_atoms;
    }
    
    if (threshold < 0.0) {
        threshold = _perception_importance_threshold;
    }
    
    try {
#ifdef HAVE_ATTENTION_BANK
        // Get atoms from attention bank above threshold
        HandleSeq all_atoms;
        _atomspace->get_handles_by_type(all_atoms, ATOM, true);
        
        for (const Handle& atom : all_atoms) {
            AttentionValuePtr av = get_av(atom);
            if (av && av->getSTI() >= threshold * 100.0) {
                high_importance_atoms.push_back(atom);
            }
        }
        
        logger().debug() << "[CognitiveLoop] Found " << high_importance_atoms.size() 
                        << " high importance atoms above threshold " << threshold;
#endif
    } catch (const std::exception& e) {
        logger().error() << "[CognitiveLoop] Error getting high importance atoms: " << e.what();
    }
    
    return high_importance_atoms;
}