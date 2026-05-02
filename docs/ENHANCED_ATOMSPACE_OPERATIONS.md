# Enhanced AtomSpace Operations for Agent-Zero

This document describes the enhanced AtomSpace operations implemented for Agent-Zero as part of task AZ-KNOW-001.

## Overview

The enhanced KnowledgeIntegrator provides advanced knowledge representation, reasoning, and pattern mining capabilities that extend the basic AtomSpace operations with:

- Advanced inference and reasoning (PLN-compatible)
- Pattern mining and discovery
- Semantic similarity search
- Knowledge synthesis
- Hypothesis generation
- Rule-based reasoning (URE-compatible)

## New API Methods

### Advanced Reasoning

#### `performAdvancedReasoning(query_atoms, reasoning_type, max_steps)`
Performs advanced reasoning with inference over the knowledge base.

**Parameters:**
- `query_atoms`: Vector of atoms to reason about
- `reasoning_type`: "forward", "backward", or "mixed" 
- `max_steps`: Maximum number of reasoning steps

**Returns:** Vector of inferred knowledge atoms

**Example:**
```cpp
std::vector<Handle> premises = {concept1, concept2};
auto results = ki->performAdvancedReasoning(premises, "mixed", 10);
```

### Pattern Discovery

#### `discoverKnowledgePatterns(data_atoms, pattern_type, minimum_support)`
Discovers knowledge patterns using pattern mining algorithms.

**Parameters:**
- `data_atoms`: Vector of atoms to analyze for patterns
- `pattern_type`: "sequential", "associative", or "causal"
- `minimum_support`: Minimum support threshold (0.0-1.0)

**Returns:** Vector of discovered pattern atoms

**Example:**
```cpp
std::vector<Handle> data = {interaction1, interaction2, interaction3};
auto patterns = ki->discoverKnowledgePatterns(data, "temporal", 0.3);
```

### Semantic Operations

#### `findSemanticallySimilar(target_concept, similarity_threshold, max_results)`
Performs semantic similarity search in the knowledge base.

**Parameters:**
- `target_concept`: The concept to find similar items for
- `similarity_threshold`: Minimum similarity score (0.0-1.0)
- `max_results`: Maximum number of results

**Returns:** Vector of similar concept atoms with similarity scores

**Example:**
```cpp
Handle dog_concept = atomspace->add_node(CONCEPT_NODE, "dog");
auto similar = ki->findSemanticallySimilar(dog_concept, 0.7, 5);
```

## Configuration Methods

### `configureAdvancedReasoning(enable_reasoning, confidence_threshold, max_inference_steps)`
Configures advanced reasoning parameters.

### `configurePatternMining(enable_mining, min_support, enable_hypothesis_generation)`
Configures pattern mining parameters.

## Usage Examples

### Basic Setup and Configuration

```cpp
// Create AtomSpace and KnowledgeIntegrator
AtomSpacePtr atomspace = std::make_shared<AtomSpace>();
MockAgentZeroCore agent_core;
KnowledgeIntegrator ki(&agent_core, atomspace);

// Configure advanced features
ki.configureAdvancedReasoning(true, 0.6, 15);
ki.configurePatternMining(true, 0.3, true);
```

### Adding Knowledge and Performing Reasoning

```cpp
// Add some facts
Handle fact1 = ki.addFact("Socrates is human", ConfidenceLevel::HIGH);
Handle fact2 = ki.addFact("All humans are mortal", ConfidenceLevel::HIGH);

// Perform advanced reasoning
std::vector<Handle> premises = {fact1, fact2};
auto results = ki.performAdvancedReasoning(premises, "forward", 5);
```

## Performance Considerations

### Computational Complexity
- Advanced reasoning: O(n × m × s) where n=premises, m=rules, s=max_steps
- Pattern discovery: O(n²) for pairwise pattern analysis
- Semantic similarity: O(n) where n=number of concepts

### Optimization Recommendations
- Set appropriate confidence thresholds to limit inference explosion
- Use maximum step limits for reasoning chains
- Implement periodic memory consolidation

## Integration with OpenCog Components

### PLN (Probabilistic Logic Networks)
The enhanced operations provide a foundation for PLN integration:
- Truth value propagation in inference chains
- Confidence-based reasoning decisions
- Hypothesis generation and validation

### URE (Unified Rule Engine)
Rule application methods are designed for URE compatibility:
- Rule selection strategies
- Truth value combination
- Flexible rule application patterns

### Pattern Miner
Pattern discovery methods integrate with the pattern miner:
- Configurable support thresholds
- Multiple pattern types (temporal, causal, statistical)
- Pattern storage and retrieval

## Testing

The implementation includes comprehensive tests in `EnhancedKnowledgeIntegratorTest.cpp`:

Run tests with:
```bash
cd agents/cpp/agentzero-core/build
export PKG_CONFIG_PATH="/usr/local/share/opencog/pkgconfig:$PKG_CONFIG_PATH"
make test
```

## References

- [OpenCog AtomSpace Documentation](https://wiki.opencog.org/w/AtomSpace)
- [PLN Tutorial](https://wiki.opencog.org/w/PLNTutorial)
- [URE Documentation](https://wiki.opencog.org/w/URE)
- [Pattern Miner Guide](https://wiki.opencog.org/w/Pattern_miner)