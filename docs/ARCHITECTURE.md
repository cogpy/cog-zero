# Agent-Zero Architecture Overview

This document provides a comprehensive overview of the Agent-Zero C++ implementation architecture and its integration with OpenCog's cognitive architecture.

## Table of Contents

1. [System Architecture](#system-architecture)
2. [Core Components](#core-components)
3. [Cognitive Loop](#cognitive-loop)
4. [OpenCog Integration](#opencog-integration)
5. [Design Patterns](#design-patterns)
6. [Data Flow](#data-flow)
7. [Extension Points](#extension-points)

## System Architecture

### High-Level Design

Agent-Zero is designed as a modular cognitive architecture that integrates deeply with OpenCog's ecosystem:

```
┌─────────────────────────────────────────────────────────────┐
│                      Agent-Zero Core                        │
│  ┌──────────────────────────────────────────────────────┐  │
│  │            Cognitive Loop Orchestrator               │  │
│  └────────────┬──────────────────────────┬──────────────┘  │
│               │                          │                   │
│  ┌────────────▼────────┐    ┌───────────▼──────────┐       │
│  │   Perception Layer   │    │   Action Layer       │       │
│  │  - Multi-modal Input │    │  - Action Planning   │       │
│  │  - Sensor Fusion     │    │  - Execution Control │       │
│  └────────────┬─────────┘    └──────────┬───────────┘       │
│               │                          │                   │
│  ┌────────────▼──────────────────────────▼──────────┐       │
│  │              AtomSpace (OpenCog)                  │       │
│  │  - Knowledge Representation                       │       │
│  │  - Pattern Matching                               │       │
│  │  - Truth Value Propagation                        │       │
│  └───────────┬────────────────────────┬──────────────┘       │
│              │                        │                      │
│  ┌───────────▼──────────┐  ┌─────────▼────────────┐        │
│  │  Reasoning Engine     │  │  Learning Engine     │        │
│  │  - PLN Integration    │  │  - MOSES Optimization│        │
│  │  - URE Inference      │  │  - Experience-based  │        │
│  │  - Goal Decomposition │  │  - Online Adaptation │        │
│  └───────────────────────┘  └──────────────────────┘        │
└─────────────────────────────────────────────────────────────┘
```

### Design Philosophy

Agent-Zero follows these core principles:

1. **OpenCog-Native**: All cognitive state represented in AtomSpace
2. **Modular Architecture**: Clear separation of concerns across 8 modules
3. **Extensible Design**: Plugin architecture for custom components
4. **Performance-Oriented**: C++17 optimizations throughout
5. **Production-Ready**: Robust error handling and monitoring

## Core Components

### 1. AgentZeroCore (Foundation)

The central orchestration component that manages the cognitive cycle.

**Location**: `agents/cpp/agentzero-core/`

**Key Classes**:
```cpp
class AgentZeroCore {
public:
    // Lifecycle management
    void initialize();
    void shutdown();
    
    // Cognitive operations
    void cognitiveStep();
    void runCognitiveLoop(int iterations);
    
    // Goal management
    Handle addGoal(const std::string& description, float priority = 0.5);
    Handle addSubGoal(Handle parent, const std::string& description);
    GoalStatus getGoalStatus(Handle goal);
    
    // Knowledge integration
    void integrateKnowledge(const HandleSeq& atoms);
    HandleSeq queryKnowledge(Handle pattern);
};
```

**Responsibilities**:
- Coordinate cognitive cycle phases
- Manage goal hierarchies
- Interface with AtomSpace
- Resource allocation and scheduling

### 2. Perception System

Multi-modal sensory input processing and integration.

**Location**: `agents/cpp/agentzero-perception/`

**Architecture**:
```
Sensory Input → Preprocessing → Feature Extraction → AtomSpace Integration
     ↓              ↓                  ↓                      ↓
  Raw Data    Normalization      Patterns              Percept Atoms
```

**Key Classes**:
```cpp
class PerceptualProcessor {
public:
    // Process sensory input
    void process(const SensoryData& input);
    
    // Multi-modal integration
    void fuseSensoryModalities();
    
    // Attention allocation
    void allocateAttention();
    
    // Output to AtomSpace
    HandleSeq getPercepts();
};
```

### 3. Knowledge Representation & Reasoning

Deep integration with OpenCog's reasoning systems.

**Location**: `agents/cpp/agentzero-knowledge/`

**Components**:

#### ReasoningEngine
```cpp
class ReasoningEngine {
public:
    // PLN-based inference
    Handle infer(Handle premise, int maxSteps);
    
    // Pattern discovery
    HandleSeq minePatterns(const HandleSeq& data);
    
    // Concept formation
    Handle formConcept(const HandleSeq& instances);
    
    // Query answering
    TruthValuePtr answerQuery(Handle question);
};
```

#### KnowledgeBase
```cpp
class KnowledgeBase {
public:
    // Knowledge operations
    void add(Handle atom);
    HandleSeq query(Handle pattern);
    void update(Handle atom, TruthValuePtr tv);
    
    // Semantic operations
    HandleSeq findSimilar(Handle concept, float threshold);
    float computeSimilarity(Handle a, Handle b);
};
```

### 4. Planning System

Hierarchical planning with temporal reasoning.

**Location**: `agents/cpp/agentzero-planning/`

**Architecture**:
```
Goals → Decomposition → Plan Generation → Temporal Scheduling → Execution
  ↓          ↓               ↓                   ↓                 ↓
High-Level  Subgoals      Action Seq         Time Constraints   Actions
```

**Key Classes**:
```cpp
class PlanningEngine {
public:
    // Plan generation
    Plan generatePlan(Handle goal);
    
    // Plan execution
    void executePlan(const Plan& plan);
    
    // Plan monitoring
    PlanStatus checkPlanStatus(const Plan& plan);
    
    // Replanning
    Plan replan(const Plan& failed_plan, Handle new_context);
};

class GoalHierarchy {
public:
    // Goal management
    void addGoal(Handle goal, float priority);
    void addSubGoal(Handle parent, Handle child);
    
    // Goal selection
    Handle selectNextGoal();
    
    // Progress tracking
    float getGoalProgress(Handle goal);
};
```

### 5. Learning System

Continuous learning and adaptation.

**Location**: `agents/cpp/agentzero-learning/`

**Learning Mechanisms**:
1. **Experience-Based**: Learn from interaction history
2. **MOSES Optimization**: Evolve policies and strategies
3. **Pattern Mining**: Discover regularities in data
4. **Meta-Learning**: Learn how to learn more effectively

**Key Classes**:
```cpp
class ExperienceManager {
public:
    // Experience storage
    void storeExperience(const Experience& exp);
    
    // Experience retrieval
    std::vector<Experience> recallSimilar(const Context& ctx);
    
    // Consolidation
    void consolidateMemories();
};

class PolicyOptimizer {
public:
    // Policy evolution
    Policy evolvePolicy(const Policy& current, 
                       const std::vector<Experience>& data);
    
    // Evaluation
    float evaluatePolicy(const Policy& policy);
};
```

### 6. Communication System

Natural language processing and multi-agent communication.

**Location**: `agents/cpp/agentzero-communication/`

**Capabilities**:
- Natural language understanding (Link Grammar integration)
- Dialogue management
- Inter-agent protocols
- Human-agent interfaces

**Key Classes**:
```cpp
class LanguageProcessor {
public:
    // NLU/NLG
    Handle parseUtterance(const std::string& text);
    std::string generateResponse(Handle meaning);
    
    // Dialogue management
    void updateDialogueState(Handle utterance);
    Handle selectResponse();
};
```

### 7. Memory System

Context-aware memory management.

**Location**: `agents/cpp/agentzero-memory/`

**Memory Types**:
- **Working Memory**: Current active context
- **Episodic Memory**: Temporal sequences of experiences
- **Long-Term Memory**: Persistent knowledge (RocksDB backend)

**Key Classes**:
```cpp
class ContextManager {
public:
    // Context operations
    void updateContext(const HandleSeq& relevant);
    HandleSeq getCurrentContext();
    
    // Context switching
    void pushContext();
    void popContext();
};

class EpisodicMemory {
public:
    // Episode recording
    void recordEpisode(const Episode& episode);
    
    // Episode retrieval
    std::vector<Episode> recallEpisodes(const Query& query);
    
    // Temporal reasoning
    HandleSeq getTemporalSequence(TimeRange range);
};
```

### 8. Tool Integration

External tool and resource management.

**Location**: `agents/cpp/agentzero-tools/`

**Capabilities**:
- Tool registry and discovery
- Unified tool invocation interface
- Capability composition
- Resource management

**Key Classes**:
```cpp
class ToolRegistry {
public:
    // Tool management
    void registerTool(const Tool& tool);
    Tool getTool(const std::string& name);
    std::vector<Tool> findTools(const Capability& required);
    
    // Tool invocation
    Result invokeTool(const std::string& name, const Args& args);
};
```

## Cognitive Loop

### The Seven-Phase Cycle

Agent-Zero implements a continuous cognitive loop with seven phases:

```cpp
void AgentZeroCore::cognitiveStep() {
    // 1. Perception: Process sensory input
    auto percepts = perceptionSystem->process(getSensorData());
    atomspace.add(percepts);
    
    // 2. Attention: Allocate computational resources
    attentionSystem->allocate();
    auto focused = attentionSystem->getFocusedAtoms();
    
    // 3. Reasoning: Inference and knowledge integration
    auto inferences = reasoningEngine->infer(focused);
    atomspace.add(inferences);
    
    // 4. Planning: Goal selection and plan generation
    auto goal = goalHierarchy->selectNextGoal();
    auto plan = planningEngine->generatePlan(goal);
    
    // 5. Action: Execute planned actions
    actionScheduler->schedule(plan.actions);
    actionScheduler->executeReady();
    
    // 6. Learning: Integrate experience and adapt
    auto experience = recordExperience();
    learningEngine->learn(experience);
    
    // 7. Reflection: Meta-cognitive analysis
    metaCognition->analyze();
    metaCognition->optimize();
}
```

### Phase Details

#### Phase 1: Perception
- Collect multi-modal sensory input
- Preprocess and normalize data
- Extract features and patterns
- Create AtomSpace representations
- Allocate attention to salient features

#### Phase 2: Attention Allocation
- ECAN-based importance spreading
- Resource allocation to focused atoms
- Context maintenance
- Salience calculation

#### Phase 3: Reasoning
- PLN-based inference on focused atoms
- Pattern matching for relevant knowledge
- Uncertainty propagation
- Knowledge integration

#### Phase 4: Planning
- Goal selection based on priority and context
- Hierarchical goal decomposition
- Plan generation with temporal constraints
- Plan validation and feasibility checking

#### Phase 5: Action
- Action scheduling with dependencies
- Temporal coordination
- Execution monitoring
- Feedback collection

#### Phase 6: Learning
- Experience recording
- Pattern mining from experience
- Policy optimization
- Knowledge base updates

#### Phase 7: Reflection
- Performance analysis
- Strategy evaluation
- Self-optimization
- Meta-cognitive adjustments

## OpenCog Integration

### AtomSpace Integration

All cognitive state is represented as Atoms:

```cpp
// Goals as EvaluationLinks
(EvaluationLink
    (PredicateNode "Goal")
    (ListLink
        (ConceptNode "LearnSkill")
        (NumberNode 0.9)))  // Priority

// Plans as SequentialLinks
(SequentialAndLink
    (ExecutionLink
        (SchemaNode "Action1")
        (ListLink args...))
    (ExecutionLink
        (SchemaNode "Action2")
        (ListLink args...)))

// Experiences as ContextLinks
(ContextLink
    (ConceptNode "Situation_123")
    (AndLink
        percepts...
        actions...
        outcomes...))
```

### CogServer Integration

Agent-Zero runs as a CogServer module:

```cpp
// Module registration
class AgentZeroModule : public Module {
public:
    void init() override {
        // Initialize agent
        agent = std::make_unique<AgentZeroCore>(cogserver.getAtomSpace());
    }
    
    // Expose commands
    registerCommand("agent-step", &AgentZeroModule::cmdStep);
    registerCommand("agent-status", &AgentZeroModule::cmdStatus);
};
```

### PLN Reasoning Integration

```cpp
// Custom PLN rules for Agent-Zero
Handle AgentZeroCore::createAgentRules() {
    // Goal decomposition rule
    Handle goalRule = createLink(BIND_LINK,
        // Variables
        createLink(VARIABLE_LIST, ...),
        // Pattern
        createLink(AND_LINK, ...),
        // Rewrite
        createLink(EXECUTION_OUTPUT_LINK, ...));
    
    // Add to URE rule base
    ure.addRule(goalRule, weight);
    
    return goalRule;
}
```

### Learning Integration

```cpp
// MOSES integration for policy optimization
class PolicyOptimizer {
    Policy optimize(const std::vector<Experience>& data) {
        // Convert experiences to fitness function
        auto fitness = createFitnessFunction(data);
        
        // Use MOSES to evolve policy
        moses::moses_parameters params;
        moses::metapopulation metapop(atomspace, params);
        
        // Run evolution
        metapop.run();
        
        // Extract best policy
        return extractPolicy(metapop.best_candidate());
    }
};
```

## Design Patterns

### 1. AtomSpace-First Design

All state lives in AtomSpace:
```cpp
// ✅ Good: AtomSpace-centric
Handle state = atomspace.add_node(CONCEPT_NODE, "AgentState");
atomspace.set_value(state, key, value);

// ❌ Avoid: Separate C++ state
std::map<std::string, Value> state;  // Bypasses AtomSpace
```

### 2. Handle Management

Proper lifetime management of Handles:
```cpp
// ✅ Good: RAII and smart pointers where needed
class Component {
    AtomSpace& atomspace;
    Handle state;  // Kept alive by AtomSpace
    
public:
    Component(AtomSpace& as) : atomspace(as) {
        state = atomspace.add_node(CONCEPT_NODE, "ComponentState");
    }
};
```

### 3. Modular Composition

Components communicate via AtomSpace:
```cpp
// Component A writes
Handle result = atomspace.add_link(EVALUATION_LINK, pred, args);

// Component B reads
HandleSeq matches = atomspace.get_atoms_by_type(EVALUATION_LINK);
```

### 4. Error Handling

Robust error handling throughout:
```cpp
try {
    auto result = operation();
} catch (const RuntimeException& e) {
    // Log error
    logger().error("Operation failed: {}", e.what());
    
    // Record in AtomSpace
    recordError(e);
    
    // Attempt recovery
    fallbackStrategy();
}
```

## Data Flow

### Information Flow Diagram

```
External World
     ↓
Sensors → Perception → AtomSpace ← Reasoning
     ↓                    ↑ ↓           ↑
Actuators ← Action ← Planning ← Learning
     ↓                         ↑
External World ───────────────┘
```

### Typical Data Transformations

```
1. Raw Sensor Data (bytes/signals)
   ↓ [PerceptualProcessor]
2. Structured Percepts (C++ objects)
   ↓ [AtomSpace Integration]
3. Atoms and Links (AtomSpace representation)
   ↓ [Pattern Matching & Reasoning]
4. Inferences and Plans (Goal-oriented Atoms)
   ↓ [Action Scheduling]
5. Action Commands (Execution Atoms)
   ↓ [Actuator Interface]
6. Physical Actions (motor commands)
```

## Extension Points

### Adding Custom Components

1. **Create Component Class**:
```cpp
class MyCustomComponent {
    AtomSpace& atomspace;
    
public:
    MyCustomComponent(AtomSpace& as) : atomspace(as) {}
    
    void process() {
        // Your processing logic
    }
};
```

2. **Register with Core**:
```cpp
AgentZeroCore agent(atomspace);
agent.registerComponent("MyComponent", 
    std::make_shared<MyCustomComponent>(atomspace));
```

3. **Integrate with Cognitive Loop**:
```cpp
void AgentZeroCore::cognitiveStep() {
    // ... existing phases ...
    
    // Custom phase
    auto custom = getComponent<MyCustomComponent>("MyComponent");
    custom->process();
}
```

### Custom Reasoning Rules

```cpp
// Define custom rule
Handle createCustomRule(AtomSpace& as) {
    // Rule pattern and rewrite
    return as.add_link(BIND_LINK, ...);
}

// Register with URE
ure.addRule(createCustomRule(atomspace), weight);
```

### Custom Learning Algorithms

```cpp
class CustomLearner : public LearningComponent {
public:
    void learn(const Experience& exp) override {
        // Your learning algorithm
    }
};

// Register
learningEngine.registerLearner(
    std::make_unique<CustomLearner>());
```

## Performance Considerations

### Memory Management
- AtomSpace handles most memory management
- Use RAII for resources
- Minimize heap allocations in hot paths

### Concurrency
- AtomSpace is thread-safe
- Use thread pools for parallel operations
- Lock-free data structures where appropriate

### Optimization Targets
- **Response Time**: < 100ms for routine decisions
- **Memory**: Linear scaling with knowledge base size
- **Throughput**: 100+ cognitive steps per second

## Summary

Agent-Zero provides a comprehensive cognitive architecture that:
- ✅ Integrates deeply with OpenCog's ecosystem
- ✅ Implements a full perception-action-learning loop
- ✅ Follows clean architectural patterns
- ✅ Provides extensive customization points
- ✅ Delivers production-ready performance

For implementation details, see:
- [API Reference](API_REFERENCE.md)
- [Integration Guide](INTEGRATION_GUIDE.md)
- [Developer Guide](DEVELOPER_GUIDE.md)

---

*Part of the AGENT-ZERO-GENESIS documentation - Phase 9: Integration & Testing (AZ-DOC-001)*
