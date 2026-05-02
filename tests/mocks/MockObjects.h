/*
 * tests/mocks/MockObjects.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Mock Objects for Agent-Zero Testing
 * Provides lightweight mock implementations of OpenCog components
 * Part of the AGENT-ZERO-GENESIS project - AZ-TEST-001
 */

#ifndef AGENTZERO_MOCK_OBJECTS_H
#define AGENTZERO_MOCK_OBJECTS_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/cogserver/server/CogServer.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {

// Forward declarations
class AgentZeroCore;
class CognitiveLoop;
class TaskManager;
class KnowledgeIntegrator;

namespace test {
namespace mocks {

/**
 * Mock AgentZeroCore for testing
 * Provides simplified implementation for unit testing
 */
class MockAgentZeroCore {
private:
    std::string _agent_name;
    std::shared_ptr<AtomSpace> _atomspace;
    bool _initialized{false};
    bool _running{false};
    Handle _agent_self_atom{Handle::UNDEFINED};
    Handle _current_goal{Handle::UNDEFINED};
    
    // Mock component instances
    std::unique_ptr<CognitiveLoop> _cognitive_loop;
    std::unique_ptr<TaskManager> _task_manager;
    std::unique_ptr<KnowledgeIntegrator> _knowledge_integrator;
    
    // Execution tracking
    int _cognitive_steps_executed{0};
    std::vector<std::string> _configuration_history;
    
public:
    explicit MockAgentZeroCore(const std::string& agent_name = "MockAgent");
    virtual ~MockAgentZeroCore();
    
    // Core interface methods
    virtual bool init();
    virtual bool start();
    virtual bool stop();
    virtual bool config(const std::string& config_params);
    
    // Status and identification
    std::string getAgentName() const { return _agent_name; }
    bool isInitialized() const { return _initialized; }
    bool isRunning() const { return _running; }
    std::string getStatusInfo() const;
    const char* id() const { return "MockAgentZeroCore"; }
    
    // AtomSpace integration
    std::shared_ptr<AtomSpace> getAtomSpace() const { return _atomspace; }
    Handle getAgentSelfAtom() const { return _agent_self_atom; }
    
    // Goal management
    bool setGoal(Handle goal_atom);
    Handle getCurrentGoal() const { return _current_goal; }
    
    // Component access
    CognitiveLoop* getCognitiveLoop() const { return _cognitive_loop.get(); }
    TaskManager* getTaskManager() const { return _task_manager.get(); }
    KnowledgeIntegrator* getKnowledgeIntegrator() const { return _knowledge_integrator.get(); }
    
    // Cognitive processing
    bool processCognitiveStep();
    
    // Test utilities
    int getCognitiveStepsExecuted() const { return _cognitive_steps_executed; }
    const std::vector<std::string>& getConfigurationHistory() const { return _configuration_history; }
    void reset();
    
    // Mock-specific configuration
    void setAtomSpace(std::shared_ptr<AtomSpace> atomspace) { _atomspace = atomspace; }
    void enableVerboseLogging(bool enabled = true);
};

/**
 * Mock CognitiveLoop for testing
 * Simulates cognitive processing cycles
 */
class MockCognitiveLoop {
private:
    bool _running{false};
    bool _paused{false};
    int _cycle_count{0};
    std::shared_ptr<AtomSpace> _atomspace;
    
    // Phase configuration
    bool _perception_enabled{true};
    bool _reasoning_enabled{true};
    bool _planning_enabled{true};
    bool _action_enabled{true};
    
    // Execution tracking
    std::vector<std::string> _executed_phases;
    std::function<void()> _custom_cycle_callback;
    
public:
    explicit MockCognitiveLoop(std::shared_ptr<AtomSpace> atomspace);
    
    // Lifecycle management
    bool start();
    bool stop();
    bool pause();
    bool resume();
    
    // Status
    bool isRunning() const { return _running; }
    bool isPaused() const { return _paused; }
    int getCycleCount() const { return _cycle_count; }
    
    // Execution
    bool executeSingleCycle();
    bool executeMultipleCycles(int count);
    
    // Configuration
    void configurePhases(bool perception, bool reasoning, bool planning, bool action);
    void setCustomCycleCallback(std::function<void()> callback) { _custom_cycle_callback = callback; }
    
    // Test utilities
    const std::vector<std::string>& getExecutedPhases() const { return _executed_phases; }
    void reset();
    
private:
    void executePerceptionPhase();
    void executeReasoningPhase();
    void executePlanningPhase();
    void executeActionPhase();
};

/**
 * Mock TaskManager for testing
 * Simulates task and goal management
 */
class MockTaskManager {
public:
    enum class Priority { LOW, MEDIUM, HIGH, CRITICAL };
    enum class TaskStatus { PENDING, RUNNING, COMPLETED, FAILED, CANCELLED };
    
private:
    std::shared_ptr<AtomSpace> _atomspace;
    Handle _current_goal{Handle::UNDEFINED};
    
    // Task tracking
    std::unordered_map<Handle, TaskStatus> _task_statuses;
    std::unordered_map<Handle, Priority> _task_priorities;
    std::unordered_map<Handle, std::string> _task_descriptions;
    std::vector<Handle> _task_execution_order;
    
    // Goal decomposition tracking
    std::unordered_map<Handle, std::vector<Handle>> _goal_subtasks;
    int _next_task_id{1};
    
public:
    explicit MockTaskManager(std::shared_ptr<AtomSpace> atomspace);
    
    // Goal management
    Handle setGoal(const std::string& description, bool decompose = false);
    Handle getCurrentGoal() const { return _current_goal; }
    bool hasActiveGoal() const { return _current_goal != Handle::UNDEFINED; }
    
    // Task management
    Handle createTask(const std::string& description, Priority priority = Priority::MEDIUM);
    bool completeTask(Handle task, bool success = true);
    bool cancelTask(Handle task);
    TaskStatus getTaskStatus(Handle task) const;
    Priority getTaskPriority(Handle task) const;
    
    // Task hierarchy
    std::vector<Handle> getSubtasks(Handle goal) const;
    bool addSubtask(Handle goal, Handle task);
    
    // Execution
    bool executeNextTask();
    std::vector<Handle> getPendingTasks() const;
    
    // Statistics
    size_t getTaskCount() const { return _task_statuses.size(); }
    size_t getCompletedTaskCount() const;
    size_t getFailedTaskCount() const;
    
    // Test utilities
    const std::vector<Handle>& getTaskExecutionOrder() const { return _task_execution_order; }
    void reset();
    
private:
    Handle createTaskAtom(const std::string& description, Priority priority);
    void updateTaskStatus(Handle task, TaskStatus status);
};

/**
 * Mock KnowledgeIntegrator for testing
 * Simulates knowledge management and integration
 */
class MockKnowledgeIntegrator {
public:
    enum class ConfidenceLevel { LOW, MEDIUM, HIGH, CERTAIN };
    enum class KnowledgeType { FACTUAL, SEMANTIC, EPISODIC, PROCEDURAL };
    
private:
    std::shared_ptr<AtomSpace> _atomspace;
    Handle _knowledge_base{Handle::UNDEFINED};
    Handle _semantic_network{Handle::UNDEFINED};
    
    // Knowledge tracking
    std::unordered_map<std::string, Handle> _concepts;
    std::unordered_map<Handle, KnowledgeType> _knowledge_types;
    std::unordered_map<Handle, ConfidenceLevel> _confidence_levels;
    std::vector<Handle> _facts;
    std::vector<Handle> _procedures;
    std::vector<Handle> _episodes;
    
    // Configuration
    bool _enable_concept_formation{true};
    bool _enable_auto_classification{true};
    
public:
    explicit MockKnowledgeIntegrator(std::shared_ptr<AtomSpace> atomspace);
    
    // Knowledge addition
    Handle addFact(const std::string& fact_description, ConfidenceLevel confidence = ConfidenceLevel::MEDIUM);
    Handle addProcedure(const std::string& procedure_description, const std::vector<std::string>& steps, 
                       ConfidenceLevel confidence = ConfidenceLevel::MEDIUM);
    Handle addEpisode(const std::string& experience_description, const std::vector<Handle>& context_atoms,
                     ConfidenceLevel confidence = ConfidenceLevel::MEDIUM);
    
    // Concept management
    Handle registerConcept(const std::string& concept_name, const std::string& description = "");
    bool hasKnowledgeAbout(const std::string& concept_name) const;
    Handle getConcept(const std::string& concept_name) const;
    
    // Semantic relations
    Handle addSemanticRelation(const std::string& subject, const std::string& predicate, 
                              const std::string& object, ConfidenceLevel confidence = ConfidenceLevel::MEDIUM);
    
    // Knowledge queries
    std::vector<Handle> queryKnowledge(const std::string& query_term, size_t max_results = 10) const;
    std::vector<Handle> getRelatedConcepts(const std::string& concept_name) const;
    
    // Knowledge integration
    bool integrateExternalKnowledge(const std::vector<Handle>& external_atoms);
    bool consolidateKnowledge();
    
    // Statistics
    size_t getFactCount() const { return _facts.size(); }
    size_t getConceptCount() const { return _concepts.size(); }
    size_t getProcedureCount() const { return _procedures.size(); }
    size_t getEpisodeCount() const { return _episodes.size(); }
    
    // Configuration
    void enableConceptFormation(bool enabled = true) { _enable_concept_formation = enabled; }
    void enableAutoClassification(bool enabled = true) { _enable_auto_classification = enabled; }
    
    // Test utilities
    const std::vector<Handle>& getFacts() const { return _facts; }
    const std::vector<Handle>& getProcedures() const { return _procedures; }
    const std::vector<Handle>& getEpisodes() const { return _episodes; }
    KnowledgeType getKnowledgeType(Handle atom) const;
    ConfidenceLevel getConfidenceLevel(Handle atom) const;
    void reset();
    
private:
    Handle createFactAtom(const std::string& description, ConfidenceLevel confidence);
    Handle createProcedureAtom(const std::string& description, const std::vector<std::string>& steps, ConfidenceLevel confidence);
    Handle createEpisodeAtom(const std::string& description, const std::vector<Handle>& context, ConfidenceLevel confidence);
    void categorizeKnowledge(Handle atom, KnowledgeType type, ConfidenceLevel confidence);
};

/**
 * Mock CogServer for testing
 * Provides minimal CogServer functionality for testing
 */
class MockCogServer {
private:
    std::shared_ptr<AtomSpace> _atomspace;
    bool _modules_loaded{false};
    std::vector<std::string> _loaded_modules;
    
public:
    MockCogServer();
    
    // Module management
    bool loadModules();
    bool loadModule(const std::string& module_name);
    const std::vector<std::string>& getLoadedModules() const { return _loaded_modules; }
    
    // AtomSpace access
    std::shared_ptr<AtomSpace> getAtomSpace() const { return _atomspace; }
    void setAtomSpace(std::shared_ptr<AtomSpace> atomspace) { _atomspace = atomspace; }
    
    // Status
    bool areModulesLoaded() const { return _modules_loaded; }
    
    // Test utilities
    void reset();
};

/**
 * Test data factory for creating test scenarios
 */
class TestDataFactory {
public:
    // AtomSpace test data
    static std::vector<Handle> createTestAtoms(AtomSpace* atomspace, size_t count = 10);
    static Handle createTestGoal(AtomSpace* atomspace, const std::string& goal_name);
    static std::vector<Handle> createTestKnowledge(AtomSpace* atomspace, size_t fact_count = 5);
    
    // Test scenarios
    static void setupBasicCognitiveScenario(MockAgentZeroCore* agent);
    static void setupKnowledgeIntegrationScenario(MockKnowledgeIntegrator* knowledge);
    static void setupTaskManagementScenario(MockTaskManager* task_manager);
    
    // Performance test data
    static std::vector<Handle> createLargeAtomSet(AtomSpace* atomspace, size_t count);
    static void createMemoryIntensiveStructure(AtomSpace* atomspace);
    
private:
    static int _next_test_id;
};

} // namespace mocks
} // namespace test
} // namespace agentzero
} // namespace opencog

#endif // AGENTZERO_MOCK_OBJECTS_H