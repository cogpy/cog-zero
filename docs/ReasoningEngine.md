# ReasoningEngine Documentation

## Overview

The `ReasoningEngine` class provides PLN-based inference and reasoning capabilities for the Agent-Zero cognitive architecture. It implements multiple reasoning strategies including forward chaining, backward chaining, abductive reasoning, and analogical reasoning.

## Key Features

- **Multi-Modal Reasoning**: Supports forward, backward, mixed, abductive, analogical, and causal reasoning
- **PLN Integration**: Uses Probabilistic Logic Networks for uncertainty handling
- **URE Integration**: Leverages the Unified Rule Engine for flexible rule application
- **Truth Value Propagation**: Proper handling of uncertainty through reasoning chains
- **Rule Management**: Dynamic loading and management of reasoning rules
- **Performance Optimization**: Caching and optimization for reasoning cycles

## Usage Examples

### Basic Reasoning

```cpp
#include <opencog/agentzero/ReasoningEngine.h>

// Create AtomSpace and ReasoningEngine
auto atomspace = std::make_shared<AtomSpace>();
auto reasoning_engine = std::make_unique<ReasoningEngine>(agent_core, atomspace);

// Create some premises
Handle concept_a = atomspace->add_node(CONCEPT_NODE, "A");
Handle concept_b = atomspace->add_node(CONCEPT_NODE, "B");
Handle implication_ab = atomspace->add_link(IMPLICATION_LINK, concept_a, concept_b);

// Set truth values
concept_a->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));
implication_ab->setTruthValue(SimpleTruthValue::createTV(0.8, 0.9));

// Perform forward chaining reasoning
std::vector<Handle> premises = {concept_a, implication_ab};
auto results = reasoning_engine->reason(premises, ReasoningMode::FORWARD_CHAINING);

// Process results
for (const auto& result : results) {
    std::cout << "Conclusion: " << result.conclusion->get_name() 
              << " (confidence: " << result.confidence << ")" << std::endl;
}
```

### Adding Custom Rules

```cpp
// Create a custom reasoning rule
ReasoningEngine::ReasoningRule custom_rule;
custom_rule.name = "my_rule";
custom_rule.weight = 0.9;
custom_rule.rule_type = "custom";
custom_rule.applicability_check = [](const std::vector<Handle>& facts) -> bool {
    // Check if rule is applicable to these facts
    return facts.size() >= 2;
};

// Add the rule to the reasoning engine
reasoning_engine->addReasoningRule(custom_rule);
```

### Hypothesis Generation

```cpp
// Create observations
std::vector<Handle> observations = {
    atomspace->add_node(CONCEPT_NODE, "Observation1"),
    atomspace->add_node(CONCEPT_NODE, "Observation2")
};

// Generate hypotheses to explain observations
auto hypotheses = reasoning_engine->generateHypotheses(observations);

for (const auto& hypothesis : hypotheses) {
    std::cout << "Hypothesis: " << hypothesis.explanation << std::endl;
}
```

## Reasoning Modes

### Forward Chaining
Derives new conclusions from given premises using available rules.

### Backward Chaining
Works backwards from goals to find supporting evidence or premises.

### Mixed Chaining
Combines forward and backward chaining for more comprehensive reasoning.

### Abductive Reasoning
Generates hypotheses to explain observed phenomena.

### Analogical Reasoning
Performs reasoning based on similarities between cases.

### Causal Reasoning
Focuses on cause-effect relationships in reasoning chains.

## Configuration

### PLN Configuration
```cpp
reasoning_engine->configurePLN(
    true,    // Enable PLN
    0.6,     // Confidence threshold
    0.5      // Truth value threshold
);
```

### URE Configuration
```cpp
reasoning_engine->configureURE(
    true,    // Enable URE
    100,     // Max iterations
    0.1      // Complexity penalty
);
```

### Reasoning Limits
```cpp
reasoning_engine->setReasoningLimits(
    15,      // Max inference steps
    0.6      // Confidence threshold
);
```

## Default Rules

The ReasoningEngine comes with several built-in rules:

1. **Deduction**: If A→B and B→C then A→C
2. **Modus Ponens**: If A→B and A then B
3. **Abduction**: If B and A→B then maybe A
4. **Inheritance**: If A isa B and B has property P then A has property P

## Integration with Agent-Zero

The ReasoningEngine is automatically integrated into the AgentZeroCore cognitive loop:

```cpp
// Access the reasoning engine from AgentZeroCore
auto* reasoning_engine = agent_core->getReasoningEngine();

// The reasoning engine processes automatically in each cognitive cycle
// through agent_core->processReasoningCycle()
```

## Performance Considerations

- Use appropriate reasoning limits to prevent excessive computation
- Cache frequently used reasoning results
- Monitor reasoning statistics for optimization
- Consider rule complexity when adding custom rules

## Error Handling

The ReasoningEngine provides robust error handling:

- Graceful handling of invalid premises
- Protection against infinite reasoning loops  
- Proper cleanup of reasoning resources
- Detailed logging for debugging

## Dependencies

- OpenCog AtomSpace
- OpenCog PLN (optional, enhances reasoning capabilities)
- OpenCog URE (optional, for advanced rule management)
- OpenCog Miner (optional, for pattern discovery)

## Thread Safety

The ReasoningEngine is designed for single-threaded use within the Agent-Zero cognitive loop. For multi-threaded applications, ensure proper synchronization.