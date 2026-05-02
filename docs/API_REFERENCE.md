# Agent-Zero API Reference

Complete API documentation for Agent-Zero C++ implementation.

## Table of Contents

1. [Core API](#core-api)
2. [Perception API](#perception-api)
3. [Knowledge API](#knowledge-api)
4. [Planning API](#planning-api)
5. [Learning API](#learning-api)
6. [Communication API](#communication-api)
7. [Memory API](#memory-api)
8. [Tools API](#tools-api)

## Core API

### AgentZeroCore

Main orchestration class for Agent-Zero cognitive architecture.

#### Header
```cpp
#include <agentzero/AgentZeroCore.h>
```

#### Class Definition
```cpp
namespace agentzero {

class AgentZeroCore {
public:
    // Constructor
    explicit AgentZeroCore(opencog::AtomSpace& atomspace);
    
    // Destructor
    ~AgentZeroCore();
    
    // Lifecycle
    void initialize();
    void shutdown();
    bool isInitialized() const;
    
    // Cognitive Loop
    void cognitiveStep();
    void runCognitiveLoop(int iterations);
    void pauseCognitiveLoop();
    void resumeCognitiveLoop();
    
    // Goal Management
    opencog::Handle addGoal(const std::string& description, 
                           float priority = 0.5f);
    opencog::Handle addSubGoal(opencog::Handle parent,
                              const std::string& description);
    void removeGoal(opencog::Handle goal);
    GoalStatus getGoalStatus(opencog::Handle goal) const;
    float getGoalPriority(opencog::Handle goal) const;
    void setGoalPriority(opencog::Handle goal, float priority);
    
    // Knowledge Integration
    void integrateKnowledge(const opencog::HandleSeq& atoms);
    opencog::HandleSeq queryKnowledge(opencog::Handle pattern);
    void updateBelief(opencog::Handle atom, opencog::TruthValuePtr tv);
    
    // Component Access
    template<typename T>
    std::shared_ptr<T> getComponent(const std::string& name);
    
    void registerComponent(const std::string& name,
                          std::shared_ptr<Component> component);
    
    // State Query
    CognitiveState getCurrentState() const;
    Statistics getStatistics() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace agentzero
```

#### Usage Example
```cpp
#include <opencog/atomspace/AtomSpace.h>
#include <agentzero/AgentZeroCore.h>

using namespace opencog;
using namespace agentzero;

// Create AtomSpace
AtomSpace atomspace;

// Create and initialize agent
AgentZeroCore agent(atomspace);
agent.initialize();

// Add goals
Handle goal = agent.addGoal("Learn new skill", 0.8f);

// Run cognitive loop
agent.runCognitiveLoop(100);

// Query goal status
GoalStatus status = agent.getGoalStatus(goal);
```

### CognitiveLoop

Implements the perception-action-reflection cycle.

#### Header
```cpp
#include <agentzero/CognitiveLoop.h>
```

#### Class Definition
```cpp
class CognitiveLoop {
public:
    explicit CognitiveLoop(opencog::AtomSpace& as);
    
    // Execution
    void executeStep();
    void configure(const CognitiveConfig& config);
    
    // Phase Access
    void setPerceptionHandler(PerceptionHandler handler);
    void setActionHandler(ActionHandler handler);
    void setLearningHandler(LearningHandler handler);
    
    // Monitoring
    PhaseMetrics getPhaseMetrics(Phase phase) const;
    std::vector<PhaseEvent> getPhaseHistory() const;
};
```

### TaskManager

Manages goal decomposition and task execution.

#### Class Definition
```cpp
class TaskManager {
public:
    explicit TaskManager(opencog::AtomSpace& as);
    
    // Task Management
    TaskId createTask(const TaskDefinition& def);
    void decomposeTask(TaskId task);
    void scheduleTask(TaskId task, TimePoint when);
    void executeTask(TaskId task);
    
    // Status
    TaskStatus getTaskStatus(TaskId task) const;
    std::vector<TaskId> getActiveTasks() const;
    std::vector<TaskId> getSubTasks(TaskId parent) const;
    
    // Priorities
    void setTaskPriority(TaskId task, float priority);
    TaskId selectNextTask();
};
```

## Perception API

### PerceptualProcessor

Multi-modal sensory input processing.

#### Header
```cpp
#include <agentzero/PerceptualProcessor.h>
```

#### Class Definition
```cpp
class PerceptualProcessor {
public:
    explicit PerceptualProcessor(opencog::AtomSpace& as);
    
    // Input Processing
    void process(const SensoryData& input);
    void processAsync(const SensoryData& input,
                     std::function<void(Result)> callback);
    
    // Multi-modal Fusion
    void fuseSensoryModalities();
    void configureFusion(const FusionConfig& config);
    
    // Attention
    void allocateAttention();
    opencog::HandleSeq getAttendedPercepts() const;
    
    // Output
    opencog::HandleSeq getPercepts() const;
    Percept getMostSalient() const;
};

// Sensory data structure
struct SensoryData {
    Modality modality;
    TimePoint timestamp;
    std::vector<float> data;
    std::map<std::string, std::string> metadata;
};

enum class Modality {
    Visual,
    Auditory,
    Tactile,
    Proprioceptive,
    Custom
};
```

### MultiModalSensor

Unified interface for various sensor types.

#### Class Definition
```cpp
class MultiModalSensor {
public:
    // Registration
    void registerSensor(const std::string& name,
                       std::shared_ptr<Sensor> sensor);
    
    // Data Collection
    SensoryData readSensor(const std::string& name);
    std::map<std::string, SensoryData> readAllSensors();
    
    // Configuration
    void configureSensor(const std::string& name,
                        const SensorConfig& config);
    
    // Callbacks
    void setSensorCallback(const std::string& name,
                          SensorCallback callback);
};
```

## Knowledge API

### ReasoningEngine

PLN-based inference and reasoning.

#### Header
```cpp
#include <agentzero/ReasoningEngine.h>
```

#### Class Definition
```cpp
class ReasoningEngine {
public:
    explicit ReasoningEngine(opencog::AtomSpace& as);
    
    // Inference
    opencog::Handle infer(opencog::Handle premise, int maxSteps = 100);
    InferenceResult inferWithProof(opencog::Handle premise);
    
    // Query Answering
    opencog::TruthValuePtr answerQuery(opencog::Handle question);
    std::vector<Answer> findAnswers(opencog::Handle question,
                                   int maxAnswers = 10);
    
    // Pattern Discovery
    opencog::HandleSeq minePatterns(const opencog::HandleSeq& data,
                                   PatternType type = PatternType::Any);
    
    // Concept Formation
    opencog::Handle formConcept(const opencog::HandleSeq& instances,
                               const std::string& name = "");
    
    // Rule Management
    void addRule(opencog::Handle rule, float weight = 1.0f);
    void removeRule(opencog::Handle rule);
    std::vector<opencog::Handle> getRules() const;
    
    // Configuration
    void setMaxInferenceSteps(int steps);
    void setMinTruthValue(float threshold);
};

struct InferenceResult {
    opencog::Handle conclusion;
    opencog::TruthValuePtr truthValue;
    std::vector<opencog::Handle> proof;
    int stepsUsed;
};
```

### KnowledgeBase

Extended AtomSpace operations.

#### Class Definition
```cpp
class KnowledgeBase {
public:
    explicit KnowledgeBase(opencog::AtomSpace& as);
    
    // Basic Operations
    void add(opencog::Handle atom);
    void remove(opencog::Handle atom);
    void update(opencog::Handle atom, opencog::TruthValuePtr tv);
    
    // Query
    opencog::HandleSeq query(opencog::Handle pattern);
    opencog::HandleSeq queryWithVariables(
        const std::string& pattern_str);
    
    // Semantic Operations
    opencog::HandleSeq findSimilar(opencog::Handle concept,
                                   float threshold = 0.7f);
    float computeSimilarity(opencog::Handle a, opencog::Handle b);
    
    // Statistics
    size_t getAtomCount() const;
    size_t getLinkCount() const;
    std::map<opencog::Type, size_t> getTypeDistribution() const;
};
```

## Planning API

### PlanningEngine

Hierarchical planning with temporal reasoning.

#### Header
```cpp
#include <agentzero/PlanningEngine.h>
```

#### Class Definition
```cpp
class PlanningEngine {
public:
    explicit PlanningEngine(opencog::AtomSpace& as);
    
    // Plan Generation
    Plan generatePlan(opencog::Handle goal);
    Plan generatePlan(opencog::Handle goal,
                     const PlanningConstraints& constraints);
    
    // Plan Execution
    void executePlan(const Plan& plan);
    void executePlanAsync(const Plan& plan,
                         std::function<void(Result)> callback);
    
    // Plan Monitoring
    PlanStatus checkPlanStatus(const Plan& plan) const;
    float getPlanProgress(const Plan& plan) const;
    
    // Replanning
    Plan replan(const Plan& failed_plan, opencog::Handle new_context);
    bool needsReplanning(const Plan& plan) const;
    
    // Configuration
    void setMaxPlanningTime(Duration timeout);
    void setPlanningHeuristic(Heuristic heuristic);
};

struct Plan {
    opencog::Handle goal;
    std::vector<Action> actions;
    std::map<std::string, std::string> metadata;
    TimePoint createdAt;
};

struct Action {
    opencog::Handle actionAtom;
    std::vector<opencog::Handle> parameters;
    TemporalConstraints timing;
    std::vector<opencog::Handle> preconditions;
    std::vector<opencog::Handle> effects;
};
```

### GoalHierarchy

Manages goal trees and dependencies.

#### Class Definition
```cpp
class GoalHierarchy {
public:
    explicit GoalHierarchy(opencog::AtomSpace& as);
    
    // Goal Management
    void addGoal(opencog::Handle goal, float priority = 0.5f);
    void addSubGoal(opencog::Handle parent, opencog::Handle child);
    void removeGoal(opencog::Handle goal);
    
    // Goal Selection
    opencog::Handle selectNextGoal();
    opencog::Handle selectNextGoal(const SelectionStrategy& strategy);
    std::vector<opencog::Handle> getActiveGoals() const;
    
    // Hierarchy Navigation
    opencog::Handle getParentGoal(opencog::Handle goal) const;
    std::vector<opencog::Handle> getSubGoals(opencog::Handle goal) const;
    std::vector<opencog::Handle> getRootGoals() const;
    
    // Progress Tracking
    float getGoalProgress(opencog::Handle goal) const;
    void updateGoalProgress(opencog::Handle goal, float progress);
};
```

## Learning API

### ExperienceManager

Manages agent's experiential memory.

#### Header
```cpp
#include <agentzero/ExperienceManager.h>
```

#### Class Definition
```cpp
class ExperienceManager {
public:
    explicit ExperienceManager(opencog::AtomSpace& as);
    
    // Experience Storage
    ExperienceId storeExperience(const Experience& exp);
    Experience retrieveExperience(ExperienceId id) const;
    
    // Experience Retrieval
    std::vector<Experience> recallSimilar(const Context& ctx,
                                         int maxResults = 10);
    std::vector<Experience> recallByTimeRange(TimePoint start,
                                             TimePoint end);
    
    // Memory Consolidation
    void consolidateMemories();
    void consolidateMemories(const ConsolidationPolicy& policy);
    
    // Statistics
    size_t getExperienceCount() const;
    std::map<std::string, size_t> getExperienceCategories() const;
};

struct Experience {
    TimePoint timestamp;
    Context context;
    Action action;
    Outcome outcome;
    float reward;
    std::map<std::string, std::string> metadata;
};
```

### PolicyOptimizer

Uses MOSES for policy evolution.

#### Class Definition
```cpp
class PolicyOptimizer {
public:
    explicit PolicyOptimizer(opencog::AtomSpace& as);
    
    // Policy Evolution
    Policy evolvePolicy(const Policy& current,
                       const std::vector<Experience>& data);
    
    // Evaluation
    float evaluatePolicy(const Policy& policy);
    float evaluatePolicy(const Policy& policy,
                        const std::vector<Experience>& testData);
    
    // Configuration
    void setPopulationSize(size_t size);
    void setGenerations(size_t generations);
    void setFitnessFunction(FitnessFunction func);
};

struct Policy {
    opencog::Handle program;
    std::map<std::string, Parameter> parameters;
    float fitness;
};
```

## Communication API

### LanguageProcessor

Natural language understanding and generation.

#### Header
```cpp
#include <agentzero/LanguageProcessor.h>
```

#### Class Definition
```cpp
class LanguageProcessor {
public:
    explicit LanguageProcessor(opencog::AtomSpace& as);
    
    // NLU (Natural Language Understanding)
    opencog::Handle parseUtterance(const std::string& text);
    opencog::Handle parseUtterance(const std::string& text,
                                   const ParseOptions& options);
    
    // NLG (Natural Language Generation)
    std::string generateResponse(opencog::Handle meaning);
    std::string generateResponse(opencog::Handle meaning,
                                const GenerationOptions& options);
    
    // Dialogue Management
    void updateDialogueState(opencog::Handle utterance);
    opencog::Handle selectResponse();
    DialogueState getDialogueState() const;
    
    // Configuration
    void setLanguage(const std::string& lang);
    void setGrammar(const std::string& grammarPath);
};
```

## Memory API

### EpisodicMemory

Temporal sequences and experiences.

#### Header
```cpp
#include <agentzero/EpisodicMemory.h>
```

#### Class Definition
```cpp
class EpisodicMemory {
public:
    explicit EpisodicMemory(opencog::AtomSpace& as);
    
    // Episode Recording
    EpisodeId recordEpisode(const Episode& episode);
    void markEpisodeEnd();
    
    // Episode Retrieval
    std::vector<Episode> recallEpisodes(const Query& query);
    Episode getEpisode(EpisodeId id) const;
    
    // Temporal Reasoning
    opencog::HandleSeq getTemporalSequence(TimeRange range);
    std::vector<Event> getEventsInRange(TimeRange range);
    
    // Consolidation
    void consolidate();
    void consolidate(const ConsolidationStrategy& strategy);
};

struct Episode {
    EpisodeId id;
    TimePoint startTime;
    TimePoint endTime;
    std::vector<Event> events;
    Context context;
};
```

### ContextManager

Maintains relevant contextual information.

#### Class Definition
```cpp
class ContextManager {
public:
    explicit ContextManager(opencog::AtomSpace& as);
    
    // Context Operations
    void updateContext(const opencog::HandleSeq& relevant);
    opencog::HandleSeq getCurrentContext() const;
    void clearContext();
    
    // Context Stack
    void pushContext();
    void popContext();
    size_t getContextDepth() const;
    
    // Context Query
    bool isInContext(opencog::Handle atom) const;
    float getContextRelevance(opencog::Handle atom) const;
};
```

## Tools API

### ToolRegistry

Catalog of available tools and capabilities.

#### Header
```cpp
#include <agentzero/ToolRegistry.h>
```

#### Class Definition
```cpp
class ToolRegistry {
public:
    // Tool Management
    void registerTool(const Tool& tool);
    void unregisterTool(const std::string& name);
    Tool getTool(const std::string& name) const;
    
    // Tool Discovery
    std::vector<Tool> findTools(const Capability& required);
    std::vector<Tool> getAllTools() const;
    
    // Tool Invocation
    Result invokeTool(const std::string& name, const Args& args);
    void invokeToolAsync(const std::string& name, const Args& args,
                        std::function<void(Result)> callback);
    
    // Capability Query
    bool hasCapability(const Capability& cap) const;
    std::vector<Capability> getCapabilities() const;
};

struct Tool {
    std::string name;
    std::string description;
    std::vector<Capability> capabilities;
    std::function<Result(const Args&)> invoke;
    std::map<std::string, std::string> metadata;
};
```

## Error Handling

All API functions may throw these exceptions:

```cpp
namespace agentzero {

// Base exception
class AgentZeroException : public std::runtime_exception {};

// Specific exceptions
class InitializationException : public AgentZeroException {};
class InvalidArgumentException : public AgentZeroException {};
class ResourceException : public AgentZeroException {};
class TimeoutException : public AgentZeroException {};
class NotImplementedException : public AgentZeroException {};

} // namespace agentzero
```

## Thread Safety

- All core components are thread-safe when accessing AtomSpace
- Concurrent access to shared state is protected
- Use provided async methods for non-blocking operations
- See [Performance Optimization](PERFORMANCE.md) for threading guidelines

## Best Practices

1. **Always check initialization**: Call `isInitialized()` before use
2. **Handle exceptions**: Wrap API calls in try-catch blocks
3. **Use RAII**: Leverage constructors/destructors for resource management
4. **Prefer async methods**: For non-blocking operations
5. **Check return values**: Validate Handles and pointers
6. **Use timeouts**: Set reasonable timeouts for long operations

## Examples

See `agents/cpp/examples/` for complete working examples of all APIs.

---

*Part of the AGENT-ZERO-GENESIS documentation - Phase 9: Integration & Testing (AZ-DOC-001)*
