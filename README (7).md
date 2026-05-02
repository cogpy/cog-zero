# OpenCog AgentZero Learning System

**AZ-LEARN-004 Implementation: MetaLearning Capabilities**

This module implements comprehensive meta-learning capabilities for the AgentZero system, providing the ability to learn how to learn more effectively through experience and adaptation.

## Overview

The AgentZero Learning System consists of four main components that work together to provide advanced learning capabilities:

1. **MetaLearning** - Core meta-learning engine
2. **ExperienceManager** - Experiential memory management
3. **SkillAcquisition** - Skill learning and development
4. **PolicyOptimizer** - Policy evolution and optimization

## Architecture

```
┌─── MetaLearning ────┐     ┌─── ExperienceManager ───┐
│  • Strategy Selection│────▶│  • Experience Storage   │
│  • Transfer Learning │     │  • Memory Consolidation │
│  • Curriculum Learning│    │  • Pattern Recognition  │
│  • Performance Monitor│     │  • Contextual Indexing  │
└─────────────────────┘     └────────────────────────┘
         │                              ▲
         ▼                              │
┌─── SkillAcquisition ─┐     ┌─── PolicyOptimizer ────┐
│  • Skill Discovery  │────▶│  • Population Evolution │
│  • Imitation Learning│     │  • MOSES Integration    │
│  • Practice & Refine │     │  • Fitness Evaluation   │
│  • Context Adaptation│     │  • Multi-objective Opt. │
└─────────────────────┘     └────────────────────────┘
```

## Components

### MetaLearning

The core meta-learning engine that orchestrates learning strategies and adaptation:

**Key Features:**
- **Strategy Selection**: Dynamically selects optimal learning strategies based on context
- **Transfer Learning**: Transfers knowledge across domains and contexts
- **Curriculum Learning**: Manages structured learning progressions
- **Performance Monitoring**: Tracks and analyzes learning effectiveness
- **Meta-Reflection**: Self-analyzes and optimizes learning processes

**Learning Strategies:**
- `SUPERVISED` - Learn from labeled examples
- `UNSUPERVISED` - Discover patterns without labels
- `REINFORCEMENT` - Learn through reward/punishment
- `IMITATION` - Learn by copying successful behaviors
- `EXPLORATION` - Learn through experimentation
- `HYBRID` - Adaptive combination of strategies
- `META_ADAPTIVE` - Learn which strategy to use when

### ExperienceManager

Manages the agent's experiential memory with efficient storage and retrieval:

**Key Features:**
- **Multi-Type Storage**: Stores learning, planning, execution, social, exploration experiences
- **Contextual Indexing**: Fast retrieval by context, task, type, and importance
- **Memory Consolidation**: Automatic pruning of low-importance experiences
- **Pattern Analysis**: Discovers patterns across experience history
- **Similarity Search**: Finds similar experiences for knowledge transfer

**Experience Types:**
- `LEARNING` - Learning-related experiences
- `PLANNING` - Planning and goal-related experiences  
- `EXECUTION` - Action execution experiences
- `SOCIAL` - Social interaction experiences
- `EXPLORATION` - Exploration and discovery experiences
- `REFLECTION` - Meta-cognitive reflection experiences

### SkillAcquisition

Learns and manages skills through various acquisition methods:

**Key Features:**
- **Imitation Learning**: Learn skills from demonstrations
- **Practice-Based Learning**: Develop skills through repeated practice
- **Context Adaptation**: Apply skills appropriately in different contexts
- **Proficiency Tracking**: Monitor skill development and proficiency levels
- **Skill Composition**: Combine simple skills into complex behaviors

**Skill Representation:**
- Preconditions, actions, and postconditions
- Proficiency and confidence levels
- Practice history and usage statistics
- Context applicability mapping

### PolicyOptimizer

Evolves and optimizes policies using evolutionary algorithms:

**Key Features:**
- **Population Evolution**: Maintains and evolves policy populations
- **MOSES Integration**: Advanced optimization using MOSES algorithms
- **Multi-Objective Optimization**: Optimize multiple objectives simultaneously
- **Fitness Evaluation**: Configurable fitness functions for policy assessment
- **Elite Preservation**: Maintains best-performing policies across generations

**Optimization Methods:**
- Tournament selection
- Crossover and mutation operations
- Elite preservation strategies
- Custom fitness functions

## Usage Examples

### Basic MetaLearning Usage

```cpp
#include "opencog/agentzero/MetaLearning.h"

// Create and initialize
AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
MetaLearning meta_learning(atomspace);
meta_learning.initialize();

// Learn a task
Handle task = atomspace->add_node(CONCEPT_NODE, "NavigationTask");
Handle context = atomspace->add_node(CONCEPT_NODE, "IndoorEnvironment");
Handle outcome = meta_learning.learnTask(task, context);

// Adapt learning strategy
LearningStrategy strategy = meta_learning.adaptLearningStrategy(context);

// Transfer knowledge between domains
Handle source_domain = atomspace->add_node(CONCEPT_NODE, "Navigation");
Handle target_domain = atomspace->add_node(CONCEPT_NODE, "Manipulation");
double transfer_score = meta_learning.transferKnowledgeBetweenDomains(source_domain, target_domain);
```

### Experience Management

```cpp
#include "opencog/agentzero/ExperienceManager.h"

ExperienceManager experience_manager(atomspace);
experience_manager.initialize();

// Record experiences
Handle exp_id = experience_manager.recordExperience(
    ExperienceType::LEARNING, context, task, outcome, 0.8);

// Query experiences
ExperienceQuery query;
query.type_filter = ExperienceType::LEARNING;
query.min_importance = 0.5;
auto experiences = experience_manager.queryExperiences(query);

// Analyze patterns
Handle patterns = experience_manager.analyzeExperiencePatterns(std::chrono::hours(24));
```

### Skill Acquisition

```cpp
#include "opencog/agentzero/SkillAcquisition.h"

SkillAcquisition skill_acquisition(atomspace);
skill_acquisition.initialize();

// Learn from demonstration
Handle demonstration = atomspace->add_node(CONCEPT_NODE, "WalkingDemo");
Handle skill_id = skill_acquisition.learnSkillFromDemonstration(
    demonstration, context, "Walking");

// Practice skill
skill_acquisition.practiceSkill(skill_id, context);

// Execute skill
Handle result = skill_acquisition.executeSkill(skill_id, context);
```

### Policy Optimization

```cpp
#include "opencog/agentzero/PolicyOptimizer.h"

PolicyOptimizer policy_optimizer(atomspace);
policy_optimizer.initialize();

// Create policy
Handle conditions = atomspace->add_node(CONCEPT_NODE, "LowBattery");
Handle actions = atomspace->add_node(CONCEPT_NODE, "FindCharger");
Handle policy_id = policy_optimizer.createPolicy("ChargingPolicy", conditions, actions);

// Optimize policies
Handle objective = atomspace->add_node(CONCEPT_NODE, "MaximizeUptime");
Handle best_policy = policy_optimizer.optimizePolicies(context, objective, 50);
```

## Configuration

Each component provides extensive configuration options:

### MetaLearningConfig
- `meta_learning_rate` - Rate of meta-level adaptation
- `exploration_factor` - Balance between exploration/exploitation
- `max_experience_history` - Maximum experiences to retain
- `enable_transfer_learning` - Enable cross-domain transfer
- `enable_curriculum_learning` - Enable structured learning progression

### SkillAcquisitionConfig
- `min_proficiency_threshold` - Minimum proficiency to consider skill learned
- `learning_rate` - Rate of skill improvement
- `max_practice_attempts` - Maximum practice attempts per session
- `enable_imitation_learning` - Enable learning by imitation
- `skill_decay_period` - Time after which unused skills decay

### PolicyOptimizerConfig
- `population_size` - Size of policy population
- `max_generations` - Maximum generations for evolution
- `mutation_rate` - Rate of policy mutation
- `crossover_rate` - Rate of policy crossover
- `enable_moses_integration` - Enable MOSES-based optimization

## Integration with OpenCog

The learning system integrates deeply with OpenCog components:

- **AtomSpace**: All knowledge and experiences are stored as atoms
- **MOSES**: Policy optimization leverages MOSES algorithms
- **ASMoses**: AtomSpace-based evolution for complex policies
- **Learn**: Integration with existing learning components
- **PLN**: Probabilistic reasoning about learning effectiveness

## Performance Targets

- **Response Time**: < 10ms for strategy selection
- **Memory Efficiency**: Linear scaling with experience count
- **Learning Rate**: Demonstrable improvement within 100 interactions
- **Transfer Effectiveness**: > 70% knowledge retention across domains
- **Skill Proficiency**: > 80% success rate for practiced skills

## Testing

Comprehensive unit tests are provided for all components:

```bash
# Build tests (requires CxxTest)
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
make

# Run tests
./tests/MetaLearningUTest
./tests/ExperienceManagerUTest
./tests/SkillAcquisitionUTest
./tests/PolicyOptimizerUTest
# Agent-Zero Learning & Adaptation Module

## Overview

The Agent-Zero Learning & Adaptation Module (agentzero-learning) implements Phase 5 of the Agent-Zero architecture, providing ASMOSES (AtomSpace MOSES) integration for evolutionary learning and adaptation within the OpenCog cognitive architecture.

## Features

### Core Components

- **ASMOSESIntegrator**: Main integration class providing the interface between Agent-Zero and ASMOSES evolutionary systems
- **AtomSpaceEvolver**: Core evolutionary algorithms specifically designed for AtomSpace program evolution
- **ExperienceManager**: Manages agent experiential memory for learning-based adaptation
- **PolicyOptimizer**: Uses evolutionary algorithms to optimize agent policies and behaviors
- **SkillAcquisition**: Learns new capabilities through experience analysis
- **MetaLearning**: Optimizes learning parameters and strategies

### Key Capabilities

- **Program Evolution**: Evolve Atomese programs directly within the AtomSpace
- **Policy Optimization**: Evolutionary optimization of agent policies using historical experience
- **Skill Learning**: Automatic acquisition of new skills from experiential data
- **AtomSpace Integration**: Deep integration with OpenCog's AtomSpace knowledge representation
- **Semantic Preservation**: Evolution that maintains semantic validity and type constraints
- **Experience-Driven Learning**: Learning guided by agent's experiential history

## Dependencies

### Required
- **cogutil**: OpenCog utilities library
- **atomspace**: OpenCog AtomSpace framework
- **moses** or **asmoses**: MOSES evolutionary algorithm library
- **Boost**: C++ libraries (system, filesystem, program_options, etc.)

### Optional
- **cogserver**: CogServer integration (enables additional features)
- **cxxtest**: Unit testing framework (for running tests)

## Building

The module uses CMake and integrates with the OpenCog unified build system:

```bash
# From the main repository root
mkdir -p /tmp/opencog-build && cd /tmp/opencog-build
cmake /path/to/repository
make agentzero-learning
```

Or build individually:

```bash
cd agents/cpp/agentzero-learning
mkdir build && cd build
cmake ..
make
```

## Usage

### Basic Integration

```cpp
#include "agentzero-learning/ASMOSESIntegrator.h"

// Create AtomSpace and integrator
auto atomspace = std::make_shared<AtomSpace>();
ASMOSESIntegrator integrator(atomspace, "my-agent");

// Initialize
if (!integrator.initialize()) {
    // Handle initialization error
}

// Evolve a program
Handle problem = atomspace->add_node(CONCEPT_NODE, "problem");
auto fitness_fn = [](const Handle& program) -> double {
    // Evaluate program fitness
    return evaluate_program(program);
};

Handle evolved_program = integrator.evolveProgram(problem, fitness_fn, 100);
```

### Policy Optimization

```cpp
// Optimize agent policy using experience
Handle policy = atomspace->add_node(CONCEPT_NODE, "current_policy");
std::vector<Handle> experiences = get_agent_experiences();

auto reward_fn = [](const Handle& policy, const std::vector<Handle>& exp) -> double {
    return calculate_policy_reward(policy, exp);
};

Handle optimized_policy = integrator.optimizePolicy(policy, experiences, reward_fn);
```

### Skill Learning

```cpp
// Learn new skills from experiences
std::vector<Handle> experience_data = get_learning_experiences();
auto learned_skills = integrator.learnSkills(experience_data);

for (const auto& skill : learned_skills) {
    std::cout << "Learned skill: " << skill->to_string() << std::endl;
}
```

## Examples

See the `examples/` directory for complete usage examples:

- `asmoses_evolution_example.cpp`: Demonstrates basic program evolution
- Additional examples showing policy optimization and skill learning

Run examples:

```bash
# After building
./build/examples/asmoses_evolution_example
```

## Testing

The module includes comprehensive unit tests using CxxTest:

```bash
# Build and run tests
cd build
make test
```

Individual test suites:
- `ASMOSESIntegratorTest`: Tests main integration functionality
- `AtomSpaceEvolverTest`: Tests evolutionary algorithms
- Additional component-specific tests

## Architecture Integration

### Agent-Zero Integration

This module integrates with the Agent-Zero core module (agentzero-core) to provide learning capabilities:

```cpp
// In Agent-Zero core initialization
if (moses_available) {
    _learning_module = std::make_unique<ASMOSESIntegrator>(_atomspace, _agent_name);
    _learning_module->initialize();
}
```

### OpenCog Integration

- **AtomSpace**: All evolved programs and learned knowledge stored as atoms
- **Type System**: Respects OpenCog type constraints during evolution
- **Truth Values**: Utilizes truth values for fitness and confidence measures
- **Pattern Matching**: Leverages pattern matching for experience analysis

## Configuration

### Evolution Parameters

```cpp
std::map<std::string, std::string> params;
params["max_generations"] = "100";
params["population_size"] = "500";
params["mutation_rate"] = "0.1";
params["crossover_rate"] = "0.8";
params["fitness_threshold"] = "0.95";

integrator.setEvolutionParams(params);
```

### Logging

```cpp
// Enable/disable detailed logging
integrator.setLogging(true);

// Set log level
logger().set_level(Logger::DEBUG);
```

## Performance Considerations

- **Population Size**: Larger populations provide better solutions but require more computation
- **Generation Limits**: Balance between solution quality and computation time  
- **Memory Usage**: Evolution maintains populations in memory; monitor usage for large problems
- **AtomSpace Size**: Large AtomSpaces may impact evolution performance

## Extension Points

The module is designed for extensibility:

- **Custom Fitness Functions**: Implement domain-specific evaluation functions
- **Evolution Operators**: Add specialized crossover and mutation operators
- **Experience Parsers**: Create parsers for different experience formats
- **Integration Modules**: Connect with additional OpenCog components

## License

This module is part of the OpenCog project and is licensed under the GNU Affero General Public License v3.

## Contributing

Contributions are welcome! Please follow OpenCog coding standards and include tests for new functionality.

## Documentation

- API documentation: Generated from source comments using Doxygen
- Architecture overview: See main Agent-Zero documentation
- OpenCog integration: See OpenCog documentation

## Support

For questions and support:
- OpenCog mailing list
- GitHub issues
- OpenCog forums
# Agent-Zero Learning Module

**Comprehensive Learning & Adaptation with MOSES Policy Optimization and Advanced Experience Management**

This module implements comprehensive learning and adaptation capabilities for Agent-Zero, combining:
- **AZ-LEARN-003**: MOSES genetic programming for policy optimization
- **Advanced Experience Management**: Sophisticated experience classification and pattern discovery
- **Full AtomSpace Integration**: Native storage with hierarchical organization

## Overview

The Agent-Zero Learning Module provides a complete learning ecosystem:

- **PolicyOptimizer**: MOSES-based genetic programming for policy evolution
- **ExperienceManager**: Advanced experience memory with 8 experience types and 6 outcome classifications
- **SkillAcquisition**: Hierarchical skill learning framework
- **MetaLearning**: Learning how to learn more effectively
- **LearningUtils**: Comprehensive utility functions and common operations

## Key Features

### MOSES Policy Optimization Integration
- Seamless integration with MOSES genetic programming
- Custom fitness functions for domain-specific optimization
- Policy evolution with configurable parameters
- AtomSpace storage for evolved policies with metadata
- Continuous background optimization with threading support

### Advanced Experience Management
- **8 Experience Types**: ACTION_OUTCOME, INTERACTION, PROBLEM_SOLVING, SKILL_APPLICATION, GOAL_PURSUIT, UNEXPECTED, LEARNING_EPISODE, EMOTIONAL
- **6 Outcome Classifications**: SUCCESS, FAILURE, PARTIAL_SUCCESS, UNEXPECTED_OUTCOME, INCONCLUSIVE, LEARNING_OPPORTUNITY
- **5 Importance Levels**: CRITICAL (100), HIGH (75), MEDIUM (50), LOW (25), ROUTINE (10)
- **Pattern Discovery**: Sequential patterns, causal patterns, success/failure analysis
- **Experience Replay**: Both prioritized and uniform sampling strategies
- **Statistical Analysis**: Real-time reward distribution tracking

### AtomSpace Integration
- Native AtomSpace storage for policies and experiences
- Hierarchical organization with inheritance links
- Temporal indexing using timestamps
- Context-aware relationships between experiences
- Metadata preservation and rich querying capabilities
- Persistent storage across sessions
# Agent-Zero Learning Module - ExperienceManager

This module implements the ExperienceManager component as part of Phase 5 (Learning & Adaptation) of the AGENT-ZERO-GENESIS project.

## Overview

The ExperienceManager provides comprehensive experience management capabilities for Agent-Zero, integrating experiential learning with OpenCog's AtomSpace. It handles experience acquisition, storage, retrieval, analysis, and learning from past experiences.

## Key Features

### Experience Management
- **8 Experience Types**: ACTION_OUTCOME, INTERACTION, PROBLEM_SOLVING, SKILL_APPLICATION, GOAL_PURSUIT, UNEXPECTED, LEARNING_EPISODE, EMOTIONAL
- **6 Outcome Classifications**: SUCCESS, FAILURE, PARTIAL_SUCCESS, UNEXPECTED_OUTCOME, INCONCLUSIVE, LEARNING_OPPORTUNITY
- **5 Importance Levels**: CRITICAL (100), HIGH (75), MEDIUM (50), LOW (25), ROUTINE (10)

### AtomSpace Integration
- All experiences stored as AtomSpace atoms with proper truth values
- Hierarchical organization with inheritance links
- Temporal indexing using timestamps
- Context-aware relationships between experiences

### Pattern Discovery
- **Sequential Patterns**: Discovers action sequences that lead to consistent outcomes
- **Causal Patterns**: Identifies cause-effect relationships in experiences
- **Success/Failure Analysis**: Tracks pattern success rates for learning
- **Pattern Significance**: Filters patterns based on configurable thresholds

### MOSES Integration
- Policy optimization based on experience data
- Experience-to-policy mapping for reinforcement learning
- Policy variant generation and testing
- Integration with AtomSpace evolution (ASMOSES)

## Architecture

```
agentzero-learning/
├── include/agentzero/learning/
│   ├── LearningTypes.h          # Core types, experience classification
│   ├── PolicyOptimizer.h        # MOSES policy optimization
│   ├── ExperienceManager.h      # Advanced experience management
│   ├── SkillAcquisition.h       # Hierarchical skill learning
│   ├── MetaLearning.h          # Meta-learning capabilities
│   └── LearningUtils.h         # Utility functions
├── src/                        # Implementation files
├── tests/                      # Unit tests
├── examples/                   # Usage examples
└── cmake/                      # Build configuration

Experience Management Architecture:
### Memory Management
- **Experience Consolidation**: Removes redundant and low-value experiences
- **Temporal Organization**: Time-based experience indexing and retrieval
- **Context Similarity**: Experience matching based on environmental and agent state
- **Retention Policies**: Configurable thresholds for experience pruning

## Architecture

```
ExperienceManager
├── Experience Storage (AtomSpace)
│   ├── Experience Atoms
│   ├── Pattern Library
│   └── Temporal Index
├── Pattern Discovery
│   ├── Sequential Patterns
│   ├── Causal Patterns
│   └── Success Analysis
├── MOSES Integration
│   ├── Policy Creation
│   ├── Policy Optimization
│   └── Experience Mapping
└── Memory Management
    ├── Consolidation
    ├── Pruning
    └── Statistics
```

## Dependencies

### Required
- **cogutil**: OpenCog utilities and logging
- **atomspace**: AtomSpace knowledge representation

### Optional (Enhanced Functionality)
- **moses**: MOSES genetic programming system
- **asmoses**: AtomSpace MOSES integration
- **learn**: Additional learning algorithms
- **miner**: Pattern mining capabilities  
- **ure**: Unified Rule Engine integration
- **cogserver**: Server integration and monitoring

## Usage Examples

### Advanced Experience Recording

```cpp
#include <agentzero/learning/ExperienceManager.h>
#include <agentzero/learning/LearningTypes.h>

// Create AtomSpace and experience manager
auto atomspace = std::make_shared<AtomSpace>();
ExperienceManager exp_manager(atomspace);

// Record a successful problem-solving experience
std::vector<Handle> actions = {
    atomspace->add_node(CONCEPT_NODE, "AnalyzeProblem"),
    atomspace->add_node(CONCEPT_NODE, "FormulateStrategy"),
    atomspace->add_node(CONCEPT_NODE, "ExecuteSolution")
};

std::vector<Handle> consequences = {
    atomspace->add_node(CONCEPT_NODE, "ProblemSolved")
};

ExperienceId exp_id = exp_manager.recordExperience(
    "Successfully solved navigation puzzle",
    ExperienceType::PROBLEM_SOLVING,
    ExperienceOutcome::SUCCESS,
    actions,
    consequences,
    0.9  // high confidence
## Usage Example

```cpp
#include <opencog/agentzero/learning/ExperienceManager.h>

// Create AtomSpace and Agent Core
AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
AgentZeroCore* agent_core = /* initialize agent core */;

// Create ExperienceManager
ExperienceManager exp_manager(agent_core, atomspace);

// Record a successful experience
ExperienceManager::ExperienceContext context;
context.timestamp = std::chrono::system_clock::now();
context.confidence_level = 0.8;

// Add environmental state atoms
Handle env_state = atomspace->add_node(CONCEPT_NODE, "CleanEnvironment");
context.environmental_state.push_back(env_state);

// Record experience with detailed context
Handle exp_atom = exp_manager.recordExperience(
    "Successfully completed navigation task",
    ExperienceManager::ExperienceType::PROBLEM_SOLVING,
    ExperienceManager::ExperienceOutcome::SUCCESS,
    context,
    {action1, action2},  // actions taken
    {consequence1}       // observed consequences
);

// Discover patterns from accumulated experiences
size_t patterns_found = exp_manager.discoverExperiencePatterns();

// Get successful patterns for decision making
std::vector<Handle> current_context = {
    atomspace->add_node(CONCEPT_NODE, "SimilarPuzzle")
};

auto successful_patterns = exp_manager.getSuccessfulPatterns(current_context, 0.8);
```

### MOSES Policy Optimization

```cpp
#include <agentzero/learning/PolicyOptimizer.h>
#include <agentzero/learning/LearningUtils.h>

// Create policy optimizer with comprehensive configuration
LearningConfig config = utils::getDefaultConfig("thorough");
auto optimizer = std::make_unique<PolicyOptimizer>(atomspace, config);

// Create custom fitness function
class NavigationFitness : public PolicyFitnessFunction {
    double evaluate(const combo_tree& program, 
                   const std::map<std::string, Handle>& context = {}) override {
        // Evaluate navigation policy performance
        return evaluateNavigationSuccess(program);
    }
    
    std::string getName() const override { return "NavigationFitness"; }
    std::vector<std::string> getInputFeatures() const override { 
        return {"position_x", "position_y", "goal_x", "goal_y", "obstacles"}; 
    }
};

auto fitness_func = std::make_shared<NavigationFitness>();
optimizer->initialize(fitness_func);

// Evolve navigation policy
auto policy = optimizer->evolvePolicy("navigation_v1");
if (policy) {
    std::cout << "Evolved policy with fitness: " << policy->fitness_score << std::endl;
}
```

### Experience-Based Learning

```cpp
// Sample experiences for learning
auto recent_experiences = exp_manager.getRecentExperiences(50);
auto successful_experiences = exp_manager.getExperiencesByOutcome(ExperienceOutcome::SUCCESS);
auto problem_solving_experiences = exp_manager.getExperiencesByType(ExperienceType::PROBLEM_SOLVING);

// Prioritized experience replay for training
auto training_batch = exp_manager.sampleExperiences(32, true);

// Get experience statistics
auto stats = exp_manager.getExperienceStats();
auto reward_stats = exp_manager.getRewardStats();

std::cout << "Total experiences: " << stats["total_experiences"] << std::endl;
std::cout << "Average reward: " << reward_stats["mean"] << std::endl;
```

## Configuration

The learning module supports comprehensive configuration:

```cpp
LearningConfig config;

// MOSES parameters
config.max_evals = 50000;           // MOSES evaluations
config.max_gens = 5000;             // MOSES generations
config.population_size = 1000;      // Population size
config.diversity_pressure = 0.15;   // Diversity pressure

// Experience management
config.experience_buffer_size = 5000; // Experience buffer size
config.learning_rate = 0.01;        // Learning rate

// Pattern discovery thresholds
// (configured via ExperienceManager methods)
```

Preset configurations available:
- `"fast"`: Quick evolution for testing and development
- `"thorough"`: Comprehensive evolution for production use
- `"memory_efficient"`: Optimized for constrained memory environments
- `"default"`: Balanced configuration for general use

## Testing

Comprehensive unit tests for all components:

```bash
# Build with tests enabled
cmake -DBUILD_TESTING=ON ..
make

# Run all tests
make test

# Run specific test suites
make PolicyOptimizerUTest
make ExperienceManagerUTest
```

## Examples

Complete examples demonstrating real-world usage:

- `policy_optimization_example.cpp`: MOSES-based XOR problem solving
- `experience_management_example.cpp`: Advanced experience management with classification

```bash
# Build and run examples
make policy_optimization_example
./policy_optimization_example

make experience_management_example  
./experience_management_example
```

## Performance Considerations

- **Memory Usage**: Experience buffer size and pattern storage directly affect memory usage
- **Evolution Time**: MOSES parameters significantly impact evolution time
- **Pattern Discovery**: O(n²) complexity for similarity calculations, optimized with indexing
- **AtomSpace Storage**: Consider periodic cleanup for long-running systems
- **Thread Safety**: All components are thread-safe for concurrent access

## Integration with Agent-Zero

This module integrates seamlessly with the complete Agent-Zero architecture:

- **agentzero-core**: Provides foundational cognitive loop integration
- **agentzero-perception**: Supplies sensory input for experience generation  
- **agentzero-planning**: Uses evolved policies for action planning
- **agentzero-knowledge**: Leverages learned patterns for reasoning
// Retrieve similar experiences for current context
std::vector<Handle> current_context = {env_state};
std::vector<Handle> similar_exp = exp_manager.getSimilarExperiences(
    current_context, 
    ExperienceManager::ExperienceType::PROBLEM_SOLVING, 
    5
);

// Get successful patterns for decision making
std::vector<Handle> successful_patterns = exp_manager.getSuccessfulPatterns(
    current_context, 
    0.7  // minimum 70% success rate
);

// Process experience management (call periodically)
bool success = exp_manager.processExperienceManagement();
```

## Configuration

The ExperienceManager supports various configuration options:

```cpp
// Enable/disable features
exp_manager.enablePatternDiscovery(true);
exp_manager.enableMOSESIntegration(true);
exp_manager.enableTemporalModeling(true);

// Set thresholds
exp_manager.setExperienceRetentionThreshold(0.3);
exp_manager.setMaxRecentExperiences(100);
exp_manager.setPatternSignificanceThreshold(0.6);

// Get configuration status
std::string status = exp_manager.getConfigurationStatus();
std::cout << status << std::endl;
```

## Dependencies

- **Required**: OpenCog AtomSpace, CogUtil
- **Optional**: MOSES, ASMoses, Learn components
- **Build**: CMake 3.16+, C++17 compiler
- **Test**: CxxTest (for unit tests)

## Building

The component is integrated with the OpenCog unified build system:

```bash
# Configure
mkdir build && cd build
cmake /path/to/opencog

# Build learning system
make agents

# Or build just the learning component
cd agents-build/cpp/agentzero-learning
make
```

## Future Enhancements

- **Deep Learning Integration**: Neural network-based meta-learning
- **Multi-Agent Learning**: Collaborative learning across agents
- **Continual Learning**: Lifelong learning without catastrophic forgetting
- **Causal Learning**: Understanding causal relationships in learning
- **Active Learning**: Intelligent selection of learning experiences

## Contributing

When contributing to the learning system:

1. Follow OpenCog coding standards
2. Add comprehensive unit tests
3. Update documentation for new features
4. Ensure AtomSpace integration consistency
5. Validate performance impact

## License

SPDX-License-Identifier: AGPL-3.0-or-later

Copyright (C) 2024 OpenCog Foundation
### Required
- **cogutil**: OpenCog utility library
- **atomspace**: OpenCog AtomSpace for knowledge representation
- **Boost**: C++ libraries for various utilities

### Optional (for enhanced functionality)
- **moses**: Policy optimization and genetic programming
- **asmoses**: AtomSpace evolution and program synthesis
- **learn**: Additional learning algorithms
- **cogserver**: Server integration and monitoring

## Building

The learning module is automatically included in the main Agent-Zero build when dependencies are available:

```bash
cd /path/to/pycog0/agents/cpp
mkdir build && cd build
cmake ..
make agentzero-learning
```

Or using the unified OpenCog build system:

```bash
cd /path/to/pycog0
mkdir /tmp/opencog-build && cd /tmp/opencog-build
cmake /path/to/pycog0
make Foundation-layer  # Build cogutil first
make atomspace         # Build atomspace
# Then build agent-zero components
```

## Testing

Run the comprehensive test suite:

```bash
cd agents/cpp/agentzero-learning/build
make test
```

Or run the simple test directly:

```bash
./tests/experience_manager_simple_test
```

## Integration with Agent-Zero Core

The ExperienceManager integrates seamlessly with other Agent-Zero components:

- **TaskManager**: Records task execution experiences
- **KnowledgeIntegrator**: Shares experience-based knowledge
- **ReasoningEngine**: Uses experience patterns for inference
- **MetaPlanner**: Optimizes planning based on past experiences

## Performance Considerations

- **Memory Usage**: Scales linearly with experience count
- **Pattern Discovery**: O(n²) complexity for similarity calculations
- **Retrieval**: Optimized with temporal and type-based indexing
- **Consolidation**: Periodic cleanup prevents memory bloat

## Future Enhancements

- **Distributed Experience Sharing**: Multi-agent experience exchange
- **Hierarchical Pattern Mining**: Multi-level pattern abstraction
- **Emotional Experience Processing**: Integration with affective systems
- **Real-time Learning**: Online experience processing and adaptation
- **Advanced prioritized experience replay algorithms**
- **Multi-objective policy optimization**
- **Transfer learning between related tasks**
- **Online skill discovery and composition**
- **Integration with deep learning frameworks**

## Building

The learning module integrates with the unified OpenCog build system:

```bash
# Using unified build system
cd /path/to/pycog0
mkdir /tmp/opencog-build && cd /tmp/opencog-build
cmake /path/to/pycog0
make Foundation-layer  # Build dependencies
make atomspace         # Build atomspace
make agents           # Build agent components including learning module

# Direct build
cd /path/to/pycog0/agents/cpp/agentzero-learning
mkdir build && cd build
cmake ..
make
```

## Contributing

When contributing to this comprehensive learning module:

1. Follow OpenCog coding standards and architectural patterns
2. Add comprehensive unit tests for new features
3. Update documentation and examples for new functionality
4. Ensure thread safety for concurrent components
5. Test integration with MOSES, AtomSpace, and other components
6. Consider both policy optimization and experience management aspects
7. Maintain compatibility with the broader Agent-Zero ecosystem

## License

This module is part of the Agent-Zero project and follows the same licensing terms as the parent project.

## Related Components

This module is part of the broader Agent-Zero architecture:

- **Phase 1**: Foundation Layer (AgentZeroCore, CognitiveLoop, TaskManager, KnowledgeIntegrator)
- **Phase 2**: Perception & Action
- **Phase 3**: Knowledge & Reasoning
- **Phase 4**: Planning & Goals
- **Phase 5**: Learning & Adaptation ← **This Module**
- **Phase 6**: Communication & NLP
- **Phase 7**: Memory & Context
- **Phase 8**: Tool Integration

See `AGENT-ZERO-GENESIS.md` for the complete roadmap and task dependencies.
