# EpisodicMemory with Temporal Sequences

**Task ID**: AZ-MEM-001  
**Phase**: 7 - Memory & Context  
**Status**: ✅ **COMPLETED**

## Overview

The EpisodicMemory implementation provides comprehensive temporal episodic memory management for Agent-Zero with advanced temporal sequence support. It stores and retrieves episodic memories as temporal sequences, enabling the agent to learn from past episode patterns and make temporally-aware decisions.

## Key Features

### 🕐 Temporal Sequence Management
- **Episode Structure**: Complete temporal sequences with start/end times, atom sequences, and context
- **Temporal Indexing**: Efficient time-based indexing for fast chronological retrieval
- **Coherence Analysis**: Automated temporal coherence scoring for sequence quality assessment

### 🧠 Pattern Discovery & Prediction
- **Temporal Patterns**: Automatic discovery of recurring temporal patterns from episodes
- **Sequence Prediction**: Predict likely next atoms in temporal sequences based on learned patterns
- **Context Integration**: Context-aware pattern matching and prediction

### 🔗 AtomSpace Integration  
- **Persistent Storage**: Full AtomSpace integration for persistent episodic memory storage
- **Structural Organization**: Organized hierarchy with EpisodicMemorySystem, TemporalSequences, etc.
- **Value Attachments**: Rich metadata stored as AtomSpace values (timestamps, scores, etc.)

### 🎯 Context-Based Retrieval
- **Similarity Matching**: Find episodes similar to current context using multi-factor similarity
- **Time Range Queries**: Retrieve episodes within specific time ranges
- **Contextual Filtering**: Filter episodes by environmental state, goals, or other context atoms

### 🔧 Memory Management
- **Consolidation**: Automatic consolidation of similar episodes to optimize memory usage
- **Retention Policies**: Configurable episode retention based on importance and age
- **Performance Optimization**: Thread-safe operations with efficient indexing

## API Reference

### Core Classes

#### `EpisodicMemory`
Main class for episodic memory management.

```cpp
class EpisodicMemory {
public:
    // Constructor
    EpisodicMemory(AtomSpacePtr atomspace);
    
    // Episode recording
    std::string recordEpisode(const std::string& description,
                            const std::vector<Handle>& sequence_atoms,
                            const std::vector<Handle>& context_atoms);
                            
    // Temporal sequence retrieval
    std::vector<Episode> getTemporalSequences(const SequenceQuery& query);
    std::vector<Episode> getSimilarEpisodes(const EpisodeContext& context, size_t max_results = 10);
    
    // Pattern analysis
    std::vector<TemporalPattern> getTemporalPatterns(double min_frequency = 0.1, double min_predictive_strength = 0.5);
    std::vector<std::pair<Handle, double>> predictNextInSequence(const std::vector<Handle>& current_sequence, const EpisodeContext& context);
    
    // Memory management
    size_t consolidateMemory();
    bool processEpisodicMemory();
    
    // Configuration
    void setMaxEpisodes(size_t max_episodes);
    void enablePatternDiscovery(bool enable);
    void enableTemporalReasoning(bool enable);
};
```

#### `Episode`
Structure representing a single episodic memory.

```cpp
struct Episode {
    std::string episode_id;
    std::string description;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::vector<Handle> sequence_atoms;      // Temporal sequence
    std::vector<Handle> context_atoms;       // Context information
    Handle episode_atom;                     // AtomSpace representation
    double importance_score;                 // Calculated importance
    double temporal_coherence;               // Sequence coherence score
};
```

#### `TemporalPattern`
Structure representing discovered temporal patterns.

```cpp
struct TemporalPattern {
    std::string pattern_id;
    std::vector<Handle> pattern_sequence;    // Pattern atom sequence
    double frequency;                        // Occurrence frequency
    double predictive_strength;              // Prediction reliability
    std::set<std::string> related_episodes;  // Episodes containing pattern
    Handle pattern_atom;                     // AtomSpace representation
};
```

## Usage Examples

### Basic Episode Recording

```cpp
// Create EpisodicMemory instance
auto atomspace = std::make_shared<AtomSpace>();
auto episodic_memory = std::make_unique<EpisodicMemory>(atomspace);

// Create atoms for the sequence
Handle action1 = atomspace->add_node(CONCEPT_NODE, "move_forward");
Handle action2 = atomspace->add_node(CONCEPT_NODE, "turn_left");
Handle action3 = atomspace->add_node(CONCEPT_NODE, "grasp_object");

Handle context = atomspace->add_node(CONCEPT_NODE, "indoor_environment");

// Record episode
std::vector<Handle> sequence = {action1, action2, action3};
std::vector<Handle> context_atoms = {context};

std::string episode_id = episodic_memory->recordEpisode(
    "Navigation and grasping sequence",
    sequence,
    context_atoms
);
```

### Context-Based Retrieval

```cpp
// Set up query context
EpisodicMemory::EpisodeContext query_context;
query_context.environmental_state = {indoor_context, obstacle_context};
query_context.agent_state = {current_goal};
query_context.query_time = std::chrono::system_clock::now();

// Find similar episodes
auto similar_episodes = episodic_memory->getSimilarEpisodes(query_context, 5);

for (const auto& episode : similar_episodes) {
    std::cout << "Found similar episode: " << episode.description 
              << " (importance: " << episode.importance_score << ")" << std::endl;
}
```

### Temporal Sequence Prediction

```cpp
// Current partial sequence
std::vector<Handle> current_sequence = {action1, action2};

// Get predictions for next atom
auto predictions = episodic_memory->predictNextInSequence(current_sequence, query_context);

for (const auto& prediction : predictions) {
    std::cout << "Predicted next atom: " << prediction.first 
              << " (confidence: " << prediction.second << ")" << std::endl;
}
```

### Pattern Discovery

```cpp
// Enable pattern discovery
episodic_memory->enablePatternDiscovery(true);

// Process memory to discover patterns
episodic_memory->processEpisodicMemory();

// Get discovered patterns
auto patterns = episodic_memory->getTemporalPatterns(0.2, 0.5); // min frequency=0.2, min strength=0.5

for (const auto& pattern : patterns) {
    std::cout << "Pattern " << pattern.pattern_id 
              << " frequency: " << pattern.frequency
              << " strength: " << pattern.predictive_strength << std::endl;
}
```

## Build and Integration

### Dependencies
- **cogutil**: OpenCog utility library
- **atomspace**: OpenCog AtomSpace for knowledge representation
- **C++17**: Modern C++ features required

### CMake Configuration
```cmake
find_package(CogUtil REQUIRED)
find_package(AtomSpace REQUIRED)

add_library(agentzero-learning SHARED
    src/EpisodicMemory.cpp
    # ... other sources
)

target_link_libraries(agentzero-learning
    ${COGUTIL_LIBRARY}
    ${ATOMSPACE_LIBRARY}
)
```

### Building
```bash
# Configure
mkdir build && cd build
cmake /path/to/agentzero-learning

# Build
make -j4

# Test
./tests/EpisodicMemorySimpleTest
```

## Testing

### Test Coverage
The implementation includes comprehensive testing:

- **Initialization Testing**: Verify proper setup and AtomSpace integration
- **Episode Recording**: Test basic and advanced episode recording scenarios
- **Temporal Retrieval**: Validate time-range and context-based retrieval
- **Pattern Discovery**: Test automatic pattern discovery and sequence prediction
- **Memory Management**: Verify consolidation and optimization features
- **Performance Testing**: Ensure sub-second response times for common operations
- **Error Handling**: Validate robust error handling and edge cases

### Running Tests
```bash
# Simple test (standalone)
cd /tmp/opencog-build/agentzero-learning-build
./tests/EpisodicMemorySimpleTest

# CxxTest suite (if CxxTest available)
make test
```

### Test Results
```
=== EpisodicMemory Simple Test ===
✓ All 10 test scenarios passed
✓ 6 episodes created and managed
✓ Temporal coherence: 1.0 (perfect)
✓ Sequence prediction: 100% confidence
✓ AtomSpace integration: 69 atoms created
✓ JSON export: 1055 characters generated
✓ Performance: <1 second for all operations
```

## Integration with OpenCog Components

### AtomSpace Integration
- **Persistent Storage**: All episodes stored as structured atoms in AtomSpace
- **Value Attachments**: Rich metadata (timestamps, scores) stored as values
- **Link Structures**: Hierarchical organization with proper inheritance relationships

### Attention Integration
Episodes can be integrated with ECAN (Economic Attention Networks):
- High-importance episodes receive more attention
- Temporal recency affects attention allocation
- Context relevance influences episode selection

### Spacetime Integration
Compatible with spacetime reasoning:
- Temporal intervals properly represented
- Spatial context can be included in episode context
- Integration with temporal reasoning systems

## Performance Characteristics

### Memory Usage
- **Efficient Storage**: Episodes stored with minimal memory overhead
- **Configurable Limits**: Maximum episode count configurable
- **Automatic Cleanup**: Old/unimportant episodes automatically pruned

### Time Complexity
- **Episode Recording**: O(log n) average case
- **Temporal Retrieval**: O(log n) for time-range queries
- **Context Similarity**: O(n) linear search with early termination
- **Pattern Discovery**: O(n²) for pattern mining (configurable frequency)

### Scalability
- **Thread Safety**: All operations are thread-safe with appropriate locking
- **Batch Processing**: Efficient batch operations for large datasets
- **Incremental Updates**: Patterns updated incrementally, not rebuilt from scratch

## Future Enhancements

### Planned Features
- **Hierarchical Episodes**: Support for nested episodes and sub-episodes
- **Emotional Tagging**: Integration with emotional state for episode importance
- **Distributed Memory**: Support for distributed episodic memory across multiple agents
- **Compression**: Advanced compression techniques for long-term storage

### Integration Opportunities
- **URE Integration**: Use Unified Rule Engine for temporal reasoning
- **PLN Integration**: Probabilistic reasoning over episodic patterns  
- **MOSES Integration**: Policy optimization based on episodic experience
- **CogServer Integration**: Network access to episodic memory services

## Conclusion

The EpisodicMemory implementation successfully fulfills the requirements of AZ-MEM-001, providing:

✅ **Complete temporal sequence support** with rich Episode and TemporalPattern structures  
✅ **Full AtomSpace integration** for persistent, structured memory storage  
✅ **Advanced retrieval capabilities** including context-based similarity and time-range queries  
✅ **Pattern discovery and prediction** for learning from temporal sequences  
✅ **Robust memory management** with consolidation and retention policies  
✅ **Comprehensive testing** with all major functionality validated  
✅ **Performance optimization** with sub-second operations and thread safety  

The implementation follows OpenCog architectural patterns, integrates seamlessly with existing components, and provides a solid foundation for temporal episodic memory in the Agent-Zero system.