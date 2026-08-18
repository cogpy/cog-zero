/*
 * tests/mocks/MockObjects.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Mock Objects Implementation for Agent-Zero Testing
 * Part of the AGENT-ZERO-GENESIS project - AZ-TEST-001
 */

#include "MockObjects.h"
#include <iostream>
#include <sstream>
#include <algorithm>

using namespace opencog;
using namespace opencog::agentzero::test::mocks;

// Static member initialization
int TestDataFactory::_next_test_id = 1;

// ===================================================================
// MockAgentZeroCore Implementation
// ===================================================================

MockAgentZeroCore::MockAgentZeroCore(const std::string& agent_name) 
    : _agent_name(agent_name) {
    _atomspace = std::make_shared<AtomSpace>();
}

MockAgentZeroCore::~MockAgentZeroCore() {
    if (_running) {
        stop();
    }
}

bool MockAgentZeroCore::init() {
    if (_initialized) {
        return true;
    }
    
    // Create agent self-representation in AtomSpace
    _agent_self_atom = _atomspace->add_node(CONCEPT_NODE, _agent_name);
    
    // Initialize mock components
    _cognitive_loop = std::make_unique<MockCognitiveLoop>(_atomspace);
    _task_manager = std::make_unique<MockTaskManager>(_atomspace);
    _knowledge_integrator = std::make_unique<MockKnowledgeIntegrator>(_atomspace);
    
    _initialized = true;
    return true;
}

bool MockAgentZeroCore::start() {
    if (!_initialized) {
        return false;
    }
    
    if (_running) {
        return true;
    }
    
    _running = true;
    if (_cognitive_loop) {
        _cognitive_loop->start();
    }
    
    return true;
}

bool MockAgentZeroCore::stop() {
    if (!_running) {
        return true;
    }
    
    _running = false;
    if (_cognitive_loop) {
        _cognitive_loop->stop();
    }
    
    return true;
}

bool MockAgentZeroCore::config(const std::string& config_params) {
    if (config_params.empty()) {
        return true;
    }
    
    _configuration_history.push_back(config_params);
    
    // Simple configuration parsing simulation
    if (config_params.find("cognitive_loop=false") != std::string::npos) {
        if (_cognitive_loop) {
            _cognitive_loop->stop();
        }
    }
    
    return true;
}

std::string MockAgentZeroCore::getStatusInfo() const {
    std::ostringstream status;
    status << "MockAgent: " << _agent_name
           << ", initialized: " << (_initialized ? "true" : "false")
           << ", running: " << (_running ? "true" : "false")
           << ", cognitive_steps: " << _cognitive_steps_executed;
    return status.str();
}

bool MockAgentZeroCore::setGoal(Handle goal_atom) {
    if (goal_atom == Handle::UNDEFINED) {
        return false;
    }
    
    _current_goal = goal_atom;
    if (_task_manager) {
        // Delegate to task manager for goal decomposition
        std::string goal_name = goal_atom->get_name();
        _task_manager->setGoal(goal_name, true);
    }
    
    return true;
}

bool MockAgentZeroCore::processCognitiveStep() {
    if (!_initialized || !_running) {
        return false;
    }
    
    _cognitive_steps_executed++;
    
    if (_cognitive_loop) {
        return _cognitive_loop->executeSingleCycle();
    }
    
    return true;
}

void MockAgentZeroCore::reset() {
    _initialized = false;
    _running = false;
    _current_goal = Handle::UNDEFINED;
    _agent_self_atom = Handle::UNDEFINED;
    _cognitive_steps_executed = 0;
    _configuration_history.clear();
    
    if (_atomspace) {
        _atomspace->clear();
    }
    
    _cognitive_loop.reset();
    _task_manager.reset();
    _knowledge_integrator.reset();
}

void MockAgentZeroCore::enableVerboseLogging(bool enabled) {
    logger().set_level(enabled ? Logger::DEBUG : Logger::INFO);
    logger().set_print_to_stdout_flag(enabled);
}

// ===================================================================
// MockCognitiveLoop Implementation
// ===================================================================

MockCognitiveLoop::MockCognitiveLoop(std::shared_ptr<AtomSpace> atomspace) 
    : _atomspace(atomspace) {
}

bool MockCognitiveLoop::start() {
    _running = true;
    _paused = false;
    return true;
}

bool MockCognitiveLoop::stop() {
    _running = false;
    _paused = false;
    return true;
}

bool MockCognitiveLoop::pause() {
    if (_running) {
        _paused = true;
    }
    return _paused;
}

bool MockCognitiveLoop::resume() {
    if (_running && _paused) {
        _paused = false;
    }
    return !_paused;
}

bool MockCognitiveLoop::executeSingleCycle() {
    if (!_running || _paused) {
        return false;
    }
    
    _cycle_count++;
    _executed_phases.clear();
    
    if (_perception_enabled) {
        executePerceptionPhase();
    }
    
    if (_reasoning_enabled) {
        executeReasoningPhase();
    }
    
    if (_planning_enabled) {
        executePlanningPhase();
    }
    
    if (_action_enabled) {
        executeActionPhase();
    }
    
    if (_custom_cycle_callback) {
        _custom_cycle_callback();
    }
    
    return true;
}

bool MockCognitiveLoop::executeMultipleCycles(int count) {
    for (int i = 0; i < count; ++i) {
        if (!executeSingleCycle()) {
            return false;
        }
    }
    return true;
}

void MockCognitiveLoop::configurePhases(bool perception, bool reasoning, bool planning, bool action) {
    _perception_enabled = perception;
    _reasoning_enabled = reasoning;
    _planning_enabled = planning;
    _action_enabled = action;
}

void MockCognitiveLoop::reset() {
    _running = false;
    _paused = false;
    _cycle_count = 0;
    _executed_phases.clear();
    _custom_cycle_callback = nullptr;
}

void MockCognitiveLoop::executePerceptionPhase() {
    _executed_phases.push_back("perception");
    // Simulate perception processing
}

void MockCognitiveLoop::executeReasoningPhase() {
    _executed_phases.push_back("reasoning");
    // Simulate reasoning processing
}

void MockCognitiveLoop::executePlanningPhase() {
    _executed_phases.push_back("planning");
    // Simulate planning processing
}

void MockCognitiveLoop::executeActionPhase() {
    _executed_phases.push_back("action");
    // Simulate action execution
}

// ===================================================================
// MockTaskManager Implementation
// ===================================================================

MockTaskManager::MockTaskManager(std::shared_ptr<AtomSpace> atomspace) 
    : _atomspace(atomspace) {
}

Handle MockTaskManager::setGoal(const std::string& description, bool decompose) {
    Handle goal = _atomspace->add_node(CONCEPT_NODE, "Goal_" + description);
    _current_goal = goal;
    
    if (decompose) {
        // Create some mock subtasks
        std::vector<std::string> subtask_names = {
            "analyze_" + description,
            "plan_" + description,
            "execute_" + description
        };
        
        for (const auto& subtask_name : subtask_names) {
            Handle subtask = createTask(subtask_name, Priority::MEDIUM);
            addSubtask(goal, subtask);
        }
    }
    
    return goal;
}

Handle MockTaskManager::createTask(const std::string& description, Priority priority) {
    Handle task = createTaskAtom(description, priority);
    updateTaskStatus(task, TaskStatus::PENDING);
    _task_priorities[task] = priority;
    _task_descriptions[task] = description;
    
    return task;
}

bool MockTaskManager::completeTask(Handle task, bool success) {
    auto it = _task_statuses.find(task);
    if (it == _task_statuses.end()) {
        return false;
    }
    
    TaskStatus new_status = success ? TaskStatus::COMPLETED : TaskStatus::FAILED;
    updateTaskStatus(task, new_status);
    _task_execution_order.push_back(task);
    
    return true;
}

bool MockTaskManager::cancelTask(Handle task) {
    auto it = _task_statuses.find(task);
    if (it == _task_statuses.end()) {
        return false;
    }
    
    updateTaskStatus(task, TaskStatus::CANCELLED);
    return true;
}

MockTaskManager::TaskStatus MockTaskManager::getTaskStatus(Handle task) const {
    auto it = _task_statuses.find(task);
    return (it != _task_statuses.end()) ? it->second : TaskStatus::PENDING;
}

MockTaskManager::Priority MockTaskManager::getTaskPriority(Handle task) const {
    auto it = _task_priorities.find(task);
    return (it != _task_priorities.end()) ? it->second : Priority::MEDIUM;
}

std::vector<Handle> MockTaskManager::getSubtasks(Handle goal) const {
    auto it = _goal_subtasks.find(goal);
    return (it != _goal_subtasks.end()) ? it->second : std::vector<Handle>();
}

bool MockTaskManager::addSubtask(Handle goal, Handle task) {
    _goal_subtasks[goal].push_back(task);
    return true;
}

bool MockTaskManager::executeNextTask() {
    auto pending_tasks = getPendingTasks();
    if (pending_tasks.empty()) {
        return false;
    }
    
    // Execute highest priority task
    auto highest_priority_task = *std::max_element(pending_tasks.begin(), pending_tasks.end(),
        [this](Handle a, Handle b) {
            return static_cast<int>(getTaskPriority(a)) < static_cast<int>(getTaskPriority(b));
        });
    
    updateTaskStatus(highest_priority_task, TaskStatus::RUNNING);
    // Simulate task execution
    completeTask(highest_priority_task, true);
    
    return true;
}

std::vector<Handle> MockTaskManager::getPendingTasks() const {
    std::vector<Handle> pending_tasks;
    for (const auto& [task, status] : _task_statuses) {
        if (status == TaskStatus::PENDING) {
            pending_tasks.push_back(task);
        }
    }
    return pending_tasks;
}

size_t MockTaskManager::getCompletedTaskCount() const {
    return std::count_if(_task_statuses.begin(), _task_statuses.end(),
        [](const auto& pair) { return pair.second == TaskStatus::COMPLETED; });
}

size_t MockTaskManager::getFailedTaskCount() const {
    return std::count_if(_task_statuses.begin(), _task_statuses.end(),
        [](const auto& pair) { return pair.second == TaskStatus::FAILED; });
}

void MockTaskManager::reset() {
    _current_goal = Handle::UNDEFINED;
    _task_statuses.clear();
    _task_priorities.clear();
    _task_descriptions.clear();
    _task_execution_order.clear();
    _goal_subtasks.clear();
    _next_task_id = 1;
}

Handle MockTaskManager::createTaskAtom(const std::string& description, Priority priority) {
    std::string task_name = "Task_" + std::to_string(_next_task_id++) + "_" + description;
    return _atomspace->add_node(CONCEPT_NODE, task_name);
}

void MockTaskManager::updateTaskStatus(Handle task, TaskStatus status) {
    _task_statuses[task] = status;
}

// ===================================================================
// MockKnowledgeIntegrator Implementation
// ===================================================================

MockKnowledgeIntegrator::MockKnowledgeIntegrator(std::shared_ptr<AtomSpace> atomspace) 
    : _atomspace(atomspace) {
    _knowledge_base = _atomspace->add_node(CONCEPT_NODE, "MockKnowledgeBase");
    _semantic_network = _atomspace->add_node(CONCEPT_NODE, "MockSemanticNetwork");
}

Handle MockKnowledgeIntegrator::addFact(const std::string& fact_description, ConfidenceLevel confidence) {
    Handle fact = createFactAtom(fact_description, confidence);
    _facts.push_back(fact);
    categorizeKnowledge(fact, KnowledgeType::FACTUAL, confidence);
    return fact;
}

Handle MockKnowledgeIntegrator::addProcedure(const std::string& procedure_description, 
                                           const std::vector<std::string>& steps, 
                                           ConfidenceLevel confidence) {
    Handle procedure = createProcedureAtom(procedure_description, steps, confidence);
    _procedures.push_back(procedure);
    categorizeKnowledge(procedure, KnowledgeType::PROCEDURAL, confidence);
    return procedure;
}

Handle MockKnowledgeIntegrator::addEpisode(const std::string& experience_description, 
                                         const std::vector<Handle>& context_atoms,
                                         ConfidenceLevel confidence) {
    Handle episode = createEpisodeAtom(experience_description, context_atoms, confidence);
    _episodes.push_back(episode);
    categorizeKnowledge(episode, KnowledgeType::EPISODIC, confidence);
    return episode;
}

Handle MockKnowledgeIntegrator::registerConcept(const std::string& concept_name, const std::string& description) {
    auto it = _concepts.find(concept_name);
    if (it != _concepts.end()) {
        return it->second;
    }
    
    Handle concept = _atomspace->add_node(CONCEPT_NODE, concept_name);
    _concepts[concept_name] = concept;
    
    if (!description.empty()) {
        categorizeKnowledge(concept, KnowledgeType::SEMANTIC, ConfidenceLevel::MEDIUM);
    }
    
    return concept;
}

bool MockKnowledgeIntegrator::hasKnowledgeAbout(const std::string& concept_name) const {
    return _concepts.find(concept_name) != _concepts.end();
}

Handle MockKnowledgeIntegrator::getConcept(const std::string& concept_name) const {
    auto it = _concepts.find(concept_name);
    return (it != _concepts.end()) ? it->second : Handle::UNDEFINED;
}

Handle MockKnowledgeIntegrator::addSemanticRelation(const std::string& subject, 
                                                   const std::string& predicate,
                                                   const std::string& object, 
                                                   ConfidenceLevel confidence) {
    Handle subject_atom = registerConcept(subject);
    Handle object_atom = registerConcept(object);
    Handle predicate_atom = registerConcept(predicate);
    
    // Create a relation link
    HandleSeq relation_args = {subject_atom, predicate_atom, object_atom};
    Handle relation = _atomspace->add_link(LIST_LINK, relation_args);
    
    categorizeKnowledge(relation, KnowledgeType::SEMANTIC, confidence);
    return relation;
}

std::vector<Handle> MockKnowledgeIntegrator::queryKnowledge(const std::string& query_term, size_t max_results) const {
    std::vector<Handle> results;
    
    // Simple substring matching simulation
    for (const auto& [concept_name, concept_handle] : _concepts) {
        if (concept_name.find(query_term) != std::string::npos) {
            results.push_back(concept_handle);
            if (results.size() >= max_results) {
                break;
            }
        }
    }
    
    return results;
}

std::vector<Handle> MockKnowledgeIntegrator::getRelatedConcepts(const std::string& concept_name) const {
    std::vector<Handle> related;
    
    Handle concept = getConcept(concept_name);
    if (concept == Handle::UNDEFINED) {
        return related;
    }
    
    // Mock implementation - return some concepts
    for (const auto& [name, handle] : _concepts) {
        if (name != concept_name && name.length() > 3) {
            related.push_back(handle);
            if (related.size() >= 5) {
                break;
            }
        }
    }
    
    return related;
}

bool MockKnowledgeIntegrator::integrateExternalKnowledge(const std::vector<Handle>& external_atoms) {
    for (Handle atom : external_atoms) {
        if (_atomspace->is_valid_handle(atom)) {
            categorizeKnowledge(atom, KnowledgeType::FACTUAL, ConfidenceLevel::MEDIUM);
        }
    }
    return true;
}

bool MockKnowledgeIntegrator::consolidateKnowledge() {
    // Mock consolidation process
    return true;
}

MockKnowledgeIntegrator::KnowledgeType MockKnowledgeIntegrator::getKnowledgeType(Handle atom) const {
    auto it = _knowledge_types.find(atom);
    return (it != _knowledge_types.end()) ? it->second : KnowledgeType::FACTUAL;
}

MockKnowledgeIntegrator::ConfidenceLevel MockKnowledgeIntegrator::getConfidenceLevel(Handle atom) const {
    auto it = _confidence_levels.find(atom);
    return (it != _confidence_levels.end()) ? it->second : ConfidenceLevel::MEDIUM;
}

void MockKnowledgeIntegrator::reset() {
    _concepts.clear();
    _knowledge_types.clear();
    _confidence_levels.clear();
    _facts.clear();
    _procedures.clear();
    _episodes.clear();
    
    if (_atomspace) {
        _atomspace->clear();
        _knowledge_base = _atomspace->add_node(CONCEPT_NODE, "MockKnowledgeBase");
        _semantic_network = _atomspace->add_node(CONCEPT_NODE, "MockSemanticNetwork");
    }
}

Handle MockKnowledgeIntegrator::createFactAtom(const std::string& description, ConfidenceLevel confidence) {
    return _atomspace->add_node(CONCEPT_NODE, "Fact_" + description);
}

Handle MockKnowledgeIntegrator::createProcedureAtom(const std::string& description, 
                                                   const std::vector<std::string>& steps, 
                                                   ConfidenceLevel confidence) {
    return _atomspace->add_node(CONCEPT_NODE, "Procedure_" + description);
}

Handle MockKnowledgeIntegrator::createEpisodeAtom(const std::string& description, 
                                                 const std::vector<Handle>& context, 
                                                 ConfidenceLevel confidence) {
    return _atomspace->add_node(CONCEPT_NODE, "Episode_" + description);
}

void MockKnowledgeIntegrator::categorizeKnowledge(Handle atom, KnowledgeType type, ConfidenceLevel confidence) {
    _knowledge_types[atom] = type;
    _confidence_levels[atom] = confidence;
}

// ===================================================================
// MockCogServer Implementation
// ===================================================================

MockCogServer::MockCogServer() {
    _atomspace = std::make_shared<AtomSpace>();
}

bool MockCogServer::loadModules() {
    if (_modules_loaded) {
        return true;
    }
    
    // Simulate loading common modules
    _loaded_modules = {"atomspace", "cogutil", "cogserver"};
    _modules_loaded = true;
    
    return true;
}

bool MockCogServer::loadModule(const std::string& module_name) {
    auto it = std::find(_loaded_modules.begin(), _loaded_modules.end(), module_name);
    if (it == _loaded_modules.end()) {
        _loaded_modules.push_back(module_name);
    }
    return true;
}

void MockCogServer::reset() {
    _modules_loaded = false;
    _loaded_modules.clear();
    if (_atomspace) {
        _atomspace->clear();
    }
}

// ===================================================================
// TestDataFactory Implementation
// ===================================================================

std::vector<Handle> TestDataFactory::createTestAtoms(AtomSpace* atomspace, size_t count) {
    std::vector<Handle> atoms;
    
    for (size_t i = 0; i < count; ++i) {
        std::string atom_name = "TestAtom_" + std::to_string(_next_test_id++);
        Handle atom = atomspace->add_node(CONCEPT_NODE, atom_name);
        atoms.push_back(atom);
    }
    
    return atoms;
}

Handle TestDataFactory::createTestGoal(AtomSpace* atomspace, const std::string& goal_name) {
    std::string full_goal_name = "TestGoal_" + goal_name + "_" + std::to_string(_next_test_id++);
    return atomspace->add_node(CONCEPT_NODE, full_goal_name);
}

std::vector<Handle> TestDataFactory::createTestKnowledge(AtomSpace* atomspace, size_t fact_count) {
    std::vector<Handle> knowledge_atoms;
    
    for (size_t i = 0; i < fact_count; ++i) {
        std::string fact_name = "TestFact_" + std::to_string(_next_test_id++);
        Handle fact = atomspace->add_node(CONCEPT_NODE, fact_name);
        knowledge_atoms.push_back(fact);
    }
    
    return knowledge_atoms;
}

void TestDataFactory::setupBasicCognitiveScenario(MockAgentZeroCore* agent) {
    if (!agent) return;
    
    agent->init();
    
    // Create a basic goal
    auto atomspace = agent->getAtomSpace();
    Handle goal = createTestGoal(atomspace.get(), "BasicCognition");
    agent->setGoal(goal);
    
    // Add some test knowledge
    auto knowledge = agent->getKnowledgeIntegrator();
    if (knowledge) {
        knowledge->addFact("Test fact for basic cognition");
        knowledge->registerConcept("TestConcept", "A concept for testing");
    }
}

void TestDataFactory::setupKnowledgeIntegrationScenario(MockKnowledgeIntegrator* knowledge) {
    if (!knowledge) return;
    
    // Add various types of knowledge
    knowledge->addFact("The sky is blue");
    knowledge->addFact("Water boils at 100 degrees Celsius");
    
    knowledge->registerConcept("Sky", "The atmosphere above");
    knowledge->registerConcept("Water", "H2O liquid");
    knowledge->registerConcept("Temperature", "Measure of heat");
    
    knowledge->addSemanticRelation("Sky", "has-color", "Blue");
    knowledge->addSemanticRelation("Water", "boils-at", "100C");
    
    std::vector<std::string> procedure_steps = {"Heat water", "Wait for bubbles", "Observe boiling"};
    knowledge->addProcedure("Boil water", procedure_steps);
}

void TestDataFactory::setupTaskManagementScenario(MockTaskManager* task_manager) {
    if (!task_manager) return;
    
    // Create a hierarchical goal structure
    Handle main_goal = task_manager->setGoal("CompleteProject", true);
    
    // Add additional tasks
    task_manager->createTask("GatherRequirements", MockTaskManager::Priority::HIGH);
    task_manager->createTask("DesignSolution", MockTaskManager::Priority::MEDIUM);
    task_manager->createTask("ImplementSolution", MockTaskManager::Priority::HIGH);
    task_manager->createTask("TestSolution", MockTaskManager::Priority::CRITICAL);
}

std::vector<Handle> TestDataFactory::createLargeAtomSet(AtomSpace* atomspace, size_t count) {
    std::vector<Handle> atoms;
    atoms.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        std::string atom_name = "LargeSet_" + std::to_string(i) + "_" + std::to_string(_next_test_id);
        Handle atom = atomspace->add_node(CONCEPT_NODE, atom_name);
        atoms.push_back(atom);
        
        // Create some links between atoms for complexity
        if (i > 0 && i % 10 == 0) {
            HandleSeq link_args = {atoms[i-1], atom};
            atomspace->add_link(LIST_LINK, link_args);
        }
    }
    
    _next_test_id++;
    return atoms;
}

void TestDataFactory::createMemoryIntensiveStructure(AtomSpace* atomspace) {
    // Create a complex nested structure
    std::vector<Handle> base_atoms = createLargeAtomSet(atomspace, 100);
    
    // Create links between atoms in a tree-like structure
    for (size_t i = 0; i < base_atoms.size(); ++i) {
        for (size_t j = i + 1; j < std::min(i + 10, base_atoms.size()); ++j) {
            HandleSeq link_args = {base_atoms[i], base_atoms[j]};
            atomspace->add_link(LIST_LINK, link_args);
        }
    }
}