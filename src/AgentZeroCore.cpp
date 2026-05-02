/*
 * src/AgentZeroCore.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Agent-Zero Core Implementation
 * Main cognitive architecture integration with OpenCog
 * Part of the AGENT-ZERO-GENESIS project
 */

#include <sstream>
#include <stdexcept>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#ifdef HAVE_COGSERVER
#include <opencog/cogserver/server/CogServer.h>
#endif

#include "opencog/agentzero/AgentZeroCore.h"
#include "opencog/agentzero/CognitiveLoop.h"
#include "opencog/agentzero/TaskManager.h"
#include "opencog/agentzero/KnowledgeIntegrator.h"
#include "opencog/agentzero/ReasoningEngine.h"

using namespace opencog;
using namespace opencog::agentzero;

// Module registration macro for CogServer
#ifdef HAVE_COGSERVER
DECLARE_MODULE(AgentZeroCore)

AgentZeroCore::AgentZeroCore(CogServer& cogserver, const std::string& agent_name)
    : Module(cogserver)
    , _atomspace(nullptr)
    , _running(false)
    , _initialized(false)
    , _agent_name(agent_name)
    , _agent_self_atom(Handle::UNDEFINED)
    , _current_goal_atom(Handle::UNDEFINED)
    , _working_memory_atom(Handle::UNDEFINED)
    , _enable_cognitive_loop(true)
    , _enable_goal_processing(true)
    , _enable_knowledge_integration(true)
    , _enable_reasoning_engine(true)
{
    logger().info() << "[AgentZeroCore] Constructor: Creating agent '" << _agent_name << "'";
}
#endif // HAVE_COGSERVER

AgentZeroCore::AgentZeroCore(const std::string& agent_name, AtomSpacePtr atomspace)
    : _atomspace(atomspace)
    , _running(false)
    , _initialized(false)
    , _agent_name(agent_name)
    , _agent_self_atom(Handle::UNDEFINED)
    , _current_goal_atom(Handle::UNDEFINED)
    , _working_memory_atom(Handle::UNDEFINED)
    , _enable_cognitive_loop(true)
    , _enable_goal_processing(true)
    , _enable_knowledge_integration(true)
    , _enable_reasoning_engine(true)
{
    logger().info() << "[AgentZeroCore] Constructor: Creating agent '" << _agent_name << "'";
}

AgentZeroCore::~AgentZeroCore()
{
    logger().info() << "[AgentZeroCore] Destructor: Cleaning up agent '" << _agent_name << "'";
    
    // Ensure agent is stopped before destruction
    if (_running.load()) {
        stop();
    }
}

#ifdef HAVE_COGSERVER
void AgentZeroCore::init()
{
    logger().info() << "[AgentZeroCore] Initializing Agent-Zero Core module";
    
    try {
        // Initialize AtomSpace integration
        initializeAtomSpace();
        
        // Create agent self-representation
        createAgentSelfRepresentation();
        
        // Setup core atoms for agent state
        setupCoreAtoms();
        
        // Initialize core components
        if (_enable_cognitive_loop) {
            _cognitive_loop = std::make_unique<CognitiveLoop>(this, _atomspace);
            logger().info() << "[AgentZeroCore] CognitiveLoop component initialized";
        }
        
        if (_enable_goal_processing) {
            _task_manager = std::make_unique<TaskManager>(this, _atomspace);
            logger().info() << "[AgentZeroCore] TaskManager component initialized";
        }
        
        if (_enable_knowledge_integration) {
            _knowledge_integrator = std::make_unique<KnowledgeIntegrator>(this, _atomspace);
            logger().info() << "[AgentZeroCore] KnowledgeIntegrator component initialized";
        }
        
        if (_enable_reasoning_engine) {
            _reasoning_engine = std::make_unique<ReasoningEngine>(this, _atomspace);
            logger().info() << "[AgentZeroCore] ReasoningEngine component initialized";
        }
        
        _initialized = true;
        logger().info() << "[AgentZeroCore] Agent-Zero Core initialization completed successfully";
        
    } catch (const std::exception& e) {
        logger().error() << "[AgentZeroCore] Initialization failed: " << e.what();
        _initialized = false;
        throw;
    }
}

bool AgentZeroCore::config(const char* config_string)
{
    if (!config_string) {
        logger().warn() << "[AgentZeroCore] NULL config string provided";
        return false;
    }
    
    logger().info() << "[AgentZeroCore] Processing configuration: " << config_string;
    
    // Parse configuration string
    std::string config(config_string);
    
    // Simple key=value configuration parsing
    if (config.find("cognitive_loop=false") != std::string::npos) {
        _enable_cognitive_loop = false;
        logger().info() << "[AgentZeroCore] Cognitive loop disabled via config";
    }
    
    if (config.find("goal_processing=false") != std::string::npos) {
        _enable_goal_processing = false;
        logger().info() << "[AgentZeroCore] Goal processing disabled via config";
    }
    
    if (config.find("knowledge_integration=false") != std::string::npos) {
        _enable_knowledge_integration = false;
        logger().info() << "[AgentZeroCore] Knowledge integration disabled via config";
    }
    
    return true;
}

const char* AgentZeroCore::id()
{
    static const char* module_id = "opencog::agentzero::AgentZeroCore";
    return module_id;
}
#endif // HAVE_COGSERVER

bool AgentZeroCore::initialize(const std::string& agent_name, AtomSpacePtr atomspace)
{
    // Allow re-initialization with a different name or AtomSpace;
    // empty/null arguments preserve the values set at construction.
    if (!agent_name.empty()) {
        _agent_name = agent_name;
    }
    if (atomspace) {
        _atomspace = atomspace;
    }

    logger().info() << "[AgentZeroCore] Initializing Agent-Zero Core (standalone mode)";

    try {
        initializeAtomSpace();
        createAgentSelfRepresentation();
        setupCoreAtoms();

        if (_enable_cognitive_loop) {
            _cognitive_loop = std::make_unique<CognitiveLoop>(this, _atomspace);
            logger().info() << "[AgentZeroCore] CognitiveLoop component initialized";
        }

        if (_enable_goal_processing) {
            _task_manager = std::make_unique<TaskManager>(this, _atomspace);
            logger().info() << "[AgentZeroCore] TaskManager component initialized";
        }

        if (_enable_knowledge_integration) {
            _knowledge_integrator = std::make_unique<KnowledgeIntegrator>(this, _atomspace);
            logger().info() << "[AgentZeroCore] KnowledgeIntegrator component initialized";
        }

        if (_enable_reasoning_engine) {
            _reasoning_engine = std::make_unique<ReasoningEngine>(this, _atomspace);
            logger().info() << "[AgentZeroCore] ReasoningEngine component initialized";
        }

        _initialized = true;
        logger().info() << "[AgentZeroCore] Agent-Zero Core initialization completed successfully";
        return true;

    } catch (const std::exception& e) {
        logger().error() << "[AgentZeroCore] Initialization failed: " << e.what();
        _initialized = false;
        return false;
    }
}

bool AgentZeroCore::start()
{
    if (!_initialized.load()) {
        logger().error() << "[AgentZeroCore] Cannot start: Agent not initialized";
        return false;
    }
    
    if (_running.load()) {
        logger().warn() << "[AgentZeroCore] Agent already running";
        return true;
    }
    
    logger().info() << "[AgentZeroCore] Starting Agent-Zero cognitive processing";
    
    try {
        // Start cognitive loop if enabled
        if (_enable_cognitive_loop && _cognitive_loop) {
            startCognitiveLoop();
        }
        
        _running = true;
        logger().info() << "[AgentZeroCore] Agent-Zero started successfully";
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[AgentZeroCore] Failed to start agent: " << e.what();
        return false;
    }
}

bool AgentZeroCore::stop()
{
    if (!_running.load()) {
        logger().warn() << "[AgentZeroCore] Agent not running";
        return true;
    }
    
    logger().info() << "[AgentZeroCore] Stopping Agent-Zero cognitive processing";
    
    try {
        // Stop cognitive loop
        if (_cognitive_loop) {
            stopCognitiveLoop();
        }
        
        _running = false;
        logger().info() << "[AgentZeroCore] Agent-Zero stopped successfully";
        return true;
        
    } catch (const std::exception& e) {
        logger().error() << "[AgentZeroCore] Error during stop: " << e.what();
        return false;
    }
}

bool AgentZeroCore::setGoal(const Handle& goal_atom)
{
    if (goal_atom == Handle::UNDEFINED) {
        logger().error() << "[AgentZeroCore] Cannot set undefined goal";
        return false;
    }
    
// <<<<<<< copilot/fix-27
    logger().info() << "[AgentZeroCore] Setting new goal: " << goal_atom->to_short_string();
// =======
//    logger().info() << "[AgentZeroCore] Setting new goal: " << goal_atom->to_string();
// >>>>>>> main
    
    _current_goal_atom = goal_atom;
    
    // Update goal in TaskManager if available
    if (_task_manager) {
        // TaskManager will handle goal decomposition internally
        logger().debug() << "[AgentZeroCore] Goal forwarded to TaskManager";
    }
    
    return true;
}

std::string AgentZeroCore::getStatusInfo() const
{
    std::ostringstream status;
    status << "{";
    status << "\"agent_name\":\"" << _agent_name << "\",";
    status << "\"running\":" << (_running.load() ? "true" : "false") << ",";
    status << "\"initialized\":" << (_initialized.load() ? "true" : "false") << ",";
    status << "\"atomspace_atoms\":" << (_atomspace ? _atomspace->get_size() : 0) << ",";
    status << "\"current_goal\":\"" << _current_goal_atom << "\",";
    status << "\"cognitive_loop_enabled\":" << (_enable_cognitive_loop ? "true" : "false") << ",";
    status << "\"goal_processing_enabled\":" << (_enable_goal_processing ? "true" : "false") << ",";
    status << "\"knowledge_integration_enabled\":" << (_enable_knowledge_integration ? "true" : "false");
    
    // Add component status if available
    if (_cognitive_loop) {
        status << ",\"cognitive_loop_cycles\":" << _cognitive_loop->getCycleCount();
    }
    
    if (_task_manager) {
        status << ",\"pending_tasks\":" << _task_manager->getPendingTaskCount();
    }
    
    status << "}";
    return status.str();
}

void AgentZeroCore::setAtomSpace(AtomSpacePtr atomspace)
{
    logger().info() << "[AgentZeroCore] Setting AtomSpace for agent '" << _agent_name << "'";
    
    _atomspace = atomspace;
    
    // Re-initialize core atoms with new atomspace
    if (_atomspace) {
        setupCoreAtoms();
        
        // Re-initialize cognitive loop with new atomspace
        if (_cognitive_loop) {
            _cognitive_loop.reset();
            _cognitive_loop = std::make_unique<CognitiveLoop>(this, _atomspace);
        }
        
        // Re-initialize other components if needed
        if (_task_manager) {
            _task_manager.reset();
            _task_manager = std::make_unique<TaskManager>(this, _atomspace);
        }
        
        if (_knowledge_integrator) {
            _knowledge_integrator.reset();
            _knowledge_integrator = std::make_unique<KnowledgeIntegrator>(this, _atomspace);
        }
        
        logger().debug() << "[AgentZeroCore] AtomSpace and components re-initialized";
    }
}

bool AgentZeroCore::processCognitiveStep()
{
    if (!_initialized.load()) {
        logger().error() << "[AgentZeroCore] Cannot process cognitive step: Not initialized";
        return false;
    }
    
    logger().debug() << "[AgentZeroCore] Processing cognitive step";
    
    try {
        bool step_success = true;
        
        // Process task management
        if (_task_manager) {
            step_success &= _task_manager->processTaskManagement();
        }
        
        // Process knowledge integration
        if (_knowledge_integrator) {
            step_success &= _knowledge_integrator->processKnowledgeIntegration();
        }
        
        // Process reasoning
        if (_reasoning_engine) {
            step_success &= _reasoning_engine->processReasoningCycle();
        }
        
        return step_success;
        
    } catch (const std::exception& e) {
        logger().error() << "[AgentZeroCore] Error in cognitive step: " << e.what();
        return false;
    }
}

// Private implementation methods

void AgentZeroCore::initializeAtomSpace()
{
    logger().debug() << "[AgentZeroCore] Initializing AtomSpace integration";

#ifdef HAVE_COGSERVER
    if (!_atomspace) {
        _atomspace = _cogserver.getAtomSpace();
        if (!_atomspace) {
            throw std::runtime_error("Failed to get AtomSpace from CogServer");
        }
    }
#else
    if (!_atomspace) {
        _atomspace = std::make_shared<AtomSpace>();
    }
#endif

    logger().debug() << "[AgentZeroCore] AtomSpace integration established";
}

void AgentZeroCore::createAgentSelfRepresentation()
{
    logger().debug() << "[AgentZeroCore] Creating agent self-representation";
    
    // Create agent self atom
    _agent_self_atom = _atomspace->add_node(CONCEPT_NODE, std::string(_agent_name));
    
    // Set initial truth value to indicate agent exists and is active
    TruthValuePtr agent_tv = SimpleTruthValue::createTV(1.0, 1.0);
    _agent_self_atom->setTruthValue(agent_tv);
    
// <<<<<<< copilot/fix-27
    logger().debug() << "[AgentZeroCore] Agent self-representation created: " << _agent_self_atom->to_short_string();
// =======
//    logger().debug() << "[AgentZeroCore] Agent self-representation created: " << _agent_self_atom->to_string();
// >>>>>>> main
}

void AgentZeroCore::setupCoreAtoms()
{
    logger().debug() << "[AgentZeroCore] Setting up core atoms";
    
    // Create working memory atom
    _working_memory_atom = _atomspace->add_node(CONCEPT_NODE, _agent_name + "_WorkingMemory");
    
    // Create initial goal atom (empty until set)
    _current_goal_atom = _atomspace->add_node(CONCEPT_NODE, _agent_name + "_CurrentGoal");
    
    // Link agent self to working memory
    HandleSeq agent_memory_link;
    agent_memory_link.push_back(_agent_self_atom);
    agent_memory_link.push_back(_working_memory_atom);
    _atomspace->add_link(EVALUATION_LINK, std::move(agent_memory_link));
    
    logger().debug() << "[AgentZeroCore] Core atoms setup completed";
}

void AgentZeroCore::startCognitiveLoop()
{
    logger().debug() << "[AgentZeroCore] Starting cognitive loop";
    
    if (_cognitive_loop) {
        bool started = _cognitive_loop->start();
        if (!started) {
            throw std::runtime_error("Failed to start cognitive loop");
        }
        logger().info() << "[AgentZeroCore] Cognitive loop started successfully";
    }
}

void AgentZeroCore::stopCognitiveLoop()
{
    logger().debug() << "[AgentZeroCore] Stopping cognitive loop";
    
    if (_cognitive_loop) {
        bool stopped = _cognitive_loop->stop();
        if (!stopped) {
            logger().warn() << "[AgentZeroCore] Cognitive loop did not stop cleanly";
        } else {
            logger().info() << "[AgentZeroCore] Cognitive loop stopped successfully";
        }
    }
}