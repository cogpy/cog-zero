# MetaPlanner API Documentation

## Overview

The MetaPlanner is a sophisticated self-reflective planning optimization system that implements meta-cognitive capabilities for analyzing and optimizing the planning process itself. It is part of the Agent-Zero Genesis project and integrates deeply with OpenCog's AtomSpace for knowledge representation and reasoning.

## Architecture

### Core Components

1. **Planning Strategy Management**: Supports 6 different planning strategies
2. **Performance Metrics Tracking**: Monitors success rates, execution times, and efficiency
3. **Self-Reflection Capabilities**: Analyzes planning effectiveness and adapts strategies
4. **Context-Aware Optimization**: Selects optimal strategies based on situational context
5. **AtomSpace Integration**: Uses OpenCog patterns for persistent knowledge storage

### Dependencies

- **AtomSpace**: Core knowledge representation system
- **CogServer**: Optional integration for distributed operation  
- **Spacetime**: Temporal reasoning integration (planned)

## Planning Strategies

### Available Strategies

```cpp
enum class PlanningStrategy {
    GREEDY,              // Simple greedy planning
    HIERARCHICAL,        // Hierarchical task network planning
    TEMPORAL,            // Temporal reasoning based planning
    ADAPTIVE,            // Adaptive strategy selection
    LEARNING_BASED,      // Experience-driven planning
    HYBRID               // Combination of multiple strategies
};
```

### Strategy Characteristics

| Strategy | Best For | Strengths | Weaknesses |
|----------|----------|-----------|------------|
| **Greedy** | Simple tasks, time-critical situations | Fast execution, low complexity | May miss optimal solutions |
| **Hierarchical** | Complex goals, structured domains | Handles complexity well | Higher computational cost |
| **Temporal** | Time-critical, scheduling-heavy tasks | Excellent temporal reasoning | May sacrifice other objectives |
| **Adaptive** | Uncertain environments, dynamic goals | Flexible, context-aware | Overhead of adaptation |
| **Learning-Based** | Repetitive tasks, pattern-rich domains | Improves with experience | Requires training data |
| **Hybrid** | Mixed complexity, multi-objective tasks | Balanced performance | Complexity of coordination |

## Optimization Objectives

### Available Objectives

```cpp
enum class OptimizationObjective {
    MINIMIZE_TIME,       // Optimize for execution time
    MINIMIZE_RESOURCES,  // Optimize for resource usage
    MAXIMIZE_SUCCESS,    // Optimize for success probability
    MINIMIZE_COMPLEXITY, // Optimize for plan simplicity
    BALANCED            // Multi-objective optimization
};
```

## Core API

### Initialization

```cpp
#include "opencog/agentzero/MetaPlanner.h"

// Create MetaPlanner instance
auto atomspace = std::make_shared<AtomSpace>();
AgentZeroCore* agent_core = nullptr; // Your AgentZeroCore instance
auto meta_planner = std::make_unique<MetaPlanner>(agent_core, atomspace);

// Configure parameters
meta_planner->configure(
    0.1,    // learning_rate (0.0-1.0)
    0.15,   // adaptation_threshold
    true    // enable_temporal_optimization
);
```

### Strategy Management

```cpp
// Set optimization objective
meta_planner->setOptimizationObjective(OptimizationObjective::MAXIMIZE_SUCCESS);

// Get current strategy
auto current_strategy = meta_planner->getCurrentStrategy();

// Force specific strategy
meta_planner->setStrategy(PlanningStrategy::HIERARCHICAL);

// Optimize strategy based on context
Handle context = atomspace->add_node(CONCEPT_NODE, "CurrentPlanningContext");
auto optimal_strategy = meta_planner->optimizePlanningStrategy(context);
```

### Performance Analysis

```cpp
// Analyze planning effectiveness
Handle analysis = meta_planner->analyzePlanningEffectiveness(context);

// Get current performance metrics
auto metrics = meta_planner->getCurrentMetrics();
std::cout << "Success Rate: " << metrics.success_rate << std::endl;
std::cout << "Avg Execution Time: " << metrics.average_execution_time << "ms" << std::endl;

// Get performance trend
auto trend = meta_planner->getPerformanceTrend(std::chrono::hours(24));
```

### Learning and Adaptation

```cpp
// Record planning episode for learning
Handle episode = atomspace->add_node(CONCEPT_NODE, "PlanningEpisode_123");
bool success = true;
auto execution_time = std::chrono::milliseconds(1500);
meta_planner->recordPlanningEpisode(episode, success, execution_time);

// Trigger reflection cycle
Handle reflection_result = meta_planner->triggerReflection();

// Learn optimization patterns
int patterns_learned = meta_planner->learnOptimizationPatterns(100);

// Apply learned optimizations
int optimizations_applied = meta_planner->applyOptimizations(context);
```

### Configuration

```cpp
// Set reflection interval
meta_planner->setReflectionInterval(std::chrono::minutes(5));

// Reset metrics and learning state
meta_planner->resetMetrics();

// Check initialization status
bool ready = meta_planner->isInitialized();
```

## Integration with Other Components

### TaskManager Integration

```cpp
// Set component references for integration
auto task_manager = std::make_shared<TaskManager>(agent_core, atomspace);
auto action_scheduler = std::make_shared<ActionScheduler>(agent_core, atomspace);

meta_planner->setComponentReferences(task_manager, action_scheduler);
```

### AtomSpace Usage Patterns

The MetaPlanner creates several types of atoms in the AtomSpace:

#### Context Atoms
```scheme
(ConceptNode "MetaPlanningContext")
(ConceptNode "PlanningStrategyContext") 
(ConceptNode "OptimizationContext")
(ConceptNode "LearningContext")
(ConceptNode "PerformanceContext")
```

#### Strategy Atoms
```scheme
(ConceptNode "PlanningStrategy_Greedy")
(ConceptNode "PlanningStrategy_Hierarchical")
; ... etc
```

#### Performance Tracking
```scheme
(EvaluationLink
  (PredicateNode "planning_episode_recorded")
  (ConceptNode "PlanningEpisode_123")
  (ConceptNode "success")
  (NumberNode "1500"))
```

## Performance Metrics

### Core Metrics

```cpp
struct PlanningMetrics {
    double success_rate;              // Plan success percentage
    double average_execution_time;    // Average plan execution time (ms)
    double resource_efficiency;       // Resource utilization efficiency
    double adaptability_score;        // How well plans adapt to changes
    double learning_rate;             // Rate of planning improvement
    std::chrono::steady_clock::time_point last_updated;
};
```

### Performance Targets

- **Response Time**: < 100ms for strategy optimization
- **Memory Efficiency**: Linear scaling with episode history
- **Learning Rate**: Measurable improvement within 100 episodes
- **Scalability**: Support for 1000+ planning episodes

## Usage Examples

### Basic Usage

```cpp
// Initialize and configure
auto meta_planner = std::make_unique<MetaPlanner>(agent_core, atomspace);
meta_planner->setOptimizationObjective(OptimizationObjective::BALANCED);

// Create planning context
Handle context = atomspace->add_node(CONCEPT_NODE, "NavigationPlanning");

// Optimize strategy for context
auto strategy = meta_planner->optimizePlanningStrategy(context);

// Execute planning with optimized strategy
// ... (your planning code here)

// Record results for learning
Handle episode = atomspace->add_node(CONCEPT_NODE, "NavigationEpisode_1");
meta_planner->recordPlanningEpisode(episode, true, std::chrono::milliseconds(800));
```

### Advanced Learning Scenario

```cpp
// Configure for aggressive learning
meta_planner->configure(0.2, 0.1, true);
meta_planner->setReflectionInterval(std::chrono::minutes(1));

// Training loop
for (int i = 0; i < 100; ++i) {
    Handle context = createPlanningContext(i);
    auto strategy = meta_planner->optimizePlanningStrategy(context);
    
    // Execute planning...
    bool success = executePlanning(strategy, context);
    auto time = measureExecutionTime();
    
    // Record episode
    Handle episode = atomspace->add_node(CONCEPT_NODE, "Episode_" + std::to_string(i));
    meta_planner->recordPlanningEpisode(episode, success, time);
    
    // Periodic reflection
    if (i % 10 == 0) {
        meta_planner->triggerReflection();
    }
}

// Analyze learned patterns
int patterns = meta_planner->learnOptimizationPatterns();
std::cout << "Learned " << patterns << " optimization patterns" << std::endl;
```

### Multi-Objective Optimization

```cpp
// Switch objectives based on context
if (isTimeCritical(context)) {
    meta_planner->setOptimizationObjective(OptimizationObjective::MINIMIZE_TIME);
} else if (isResourceConstrained(context)) {
    meta_planner->setOptimizationObjective(OptimizationObjective::MINIMIZE_RESOURCES);
} else {
    meta_planner->setOptimizationObjective(OptimizationObjective::BALANCED);
}

auto strategy = meta_planner->optimizePlanningStrategy(context);
```

## Error Handling

The MetaPlanner provides robust error handling:

```cpp
// Check for valid handles
if (context == Handle::UNDEFINED) {
    // MetaPlanner will handle gracefully and use current strategy
}

// Validate initialization
if (!meta_planner->isInitialized()) {
    logger().error() << "MetaPlanner not properly initialized";
    return;
}

// Handle invalid episodes
meta_planner->recordPlanningEpisode(Handle::UNDEFINED, true, std::chrono::milliseconds(1000));
// This will log an error but not crash
```

## Best Practices

### 1. Context Creation
- Create meaningful context atoms that capture relevant planning situation
- Use consistent naming conventions for contexts
- Include temporal and resource information in contexts

### 2. Episode Recording
- Record all planning episodes, both successful and failed
- Include accurate execution times for learning
- Use descriptive episode names for debugging

### 3. Reflection Tuning
- Set reflection intervals based on planning frequency
- Trigger manual reflection after significant changes
- Monitor adaptation threshold to avoid over-optimization

### 4. Strategy Selection
- Let MetaPlanner optimize strategy automatically when possible
- Use manual strategy override only for critical situations  
- Consider context-specific strategy preferences

## Integration Notes

### With Existing Agent-Zero Components

The MetaPlanner is designed to integrate seamlessly with existing Agent-Zero components:

- **TaskManager**: Shares goal and task information for context-aware optimization
- **ActionScheduler**: Coordinates with temporal planning optimization
- **ReasoningEngine**: Uses PLN reasoning for strategy effectiveness analysis
- **KnowledgeIntegrator**: Leverages knowledge base for context feature extraction

### Future Extensions

Planned enhancements include:

- **Spacetime Integration**: Full temporal reasoning integration
- **Multi-Agent Coordination**: Distributed meta-planning capabilities  
- **Advanced Learning**: Deep learning integration for pattern recognition
- **Real-time Optimization**: Sub-millisecond strategy optimization
- **Explanation Generation**: Human-readable explanations of planning decisions

## Troubleshooting

### Common Issues

1. **Initialization Failures**
   - Ensure AtomSpace is properly initialized
   - Check that all dependencies are installed
   - Verify component references are set correctly

2. **Poor Learning Performance**
   - Increase learning rate if adaptation is too slow
   - Decrease adaptation threshold if over-optimizing
   - Ensure sufficient episode diversity for learning

3. **Memory Usage Growth**
   - Monitor episode history size
   - Use resetMetrics() periodically if needed
   - Check for AtomSpace memory leaks

### Debug Information

Enable verbose logging to troubleshoot issues:

```cpp
// Enable debug logging (implementation dependent)
Logger::setLevel(Logger::DEBUG);

// Check MetaPlanner status
if (meta_planner->isInitialized()) {
    auto metrics = meta_planner->getCurrentMetrics();
    logger().info() << "MetaPlanner Status: " << metrics.success_rate;
}
```

## Thread Safety

The MetaPlanner provides basic thread safety:

- **Read Operations**: Generally thread-safe for metrics and strategy queries
- **Write Operations**: Episode recording and configuration changes should be synchronized
- **AtomSpace Access**: Thread safety depends on AtomSpace implementation

For multi-threaded applications, consider using external synchronization:

```cpp
std::mutex meta_planner_mutex;

// Thread-safe episode recording
void recordEpisode(Handle episode, bool success, std::chrono::milliseconds time) {
    std::lock_guard<std::mutex> lock(meta_planner_mutex);
    meta_planner->recordPlanningEpisode(episode, success, time);
}
```

## API Reference Summary

### Core Methods
- `setOptimizationObjective(OptimizationObjective)` - Set optimization target
- `optimizePlanningStrategy(Handle)` - Get optimal strategy for context
- `recordPlanningEpisode(Handle, bool, milliseconds)` - Record episode for learning
- `triggerReflection()` - Analyze performance and adapt strategies

### Configuration Methods  
- `configure(double, double, bool)` - Set learning parameters
- `setReflectionInterval(milliseconds)` - Set automatic reflection timing
- `setComponentReferences(TaskManager, ActionScheduler)` - Enable integration

### Query Methods
- `getCurrentStrategy()` - Get active planning strategy
- `getCurrentMetrics()` - Get current performance metrics
- `getStrategyEvaluation(PlanningStrategy)` - Get strategy-specific metrics
- `isInitialized()` - Check initialization status

### Utility Methods
- `strategyToString(PlanningStrategy)` - Convert strategy enum to string
- `objectiveToString(OptimizationObjective)` - Convert objective enum to string
- `resetMetrics()` - Clear performance history and reset learning state