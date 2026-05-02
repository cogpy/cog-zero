# Agent-Zero Python Bridge

Python interoperability bridge for Agent-Zero C++ components integrated with OpenCog.

## Overview

The Agent-Zero Python Bridge provides Python bindings to the C++ Agent-Zero core components, enabling Python developers to leverage the full power of OpenCog's cognitive architecture through an intuitive Python API.

## Features

- **Complete Bindings**: Full access to AgentZeroCore, CognitiveLoop, TaskManager, and KnowledgeIntegrator
- **AtomSpace Integration**: Seamless integration with OpenCog's AtomSpace through Python
- **High-Level API**: Pythonic interface that abstracts C++ complexity
- **Type Safety**: Proper error handling and type conversions between Python and C++
- **Performance**: Efficient Cython-based bindings with minimal overhead

## Key Components

### 1. AgentZeroCore Bridge
- Main orchestration interface
- Cognitive loop management
- Agent state access and control

### 2. CognitiveLoop Bridge
- Perception-action-reflection cycle control
- Step-by-step execution
- Asynchronous loop management

### 3. TaskManager Bridge
- Goal decomposition and management
- Task execution and monitoring
- Hierarchical goal structures

### 4. KnowledgeIntegrator Bridge
- AtomSpace knowledge operations
- Knowledge graph queries and updates
- Pattern matching and reasoning

## Dependencies

- Python 3.8+
- Cython 0.29+
- cogutil (OpenCog utilities)
- atomspace (OpenCog knowledge representation)
- agentzero-core (Agent-Zero C++ core)

## Installation

### From Source

```bash
cd agents/cpp/agentzero-python-bridge
mkdir build && cd build
cmake ..
make
sudo make install
```

### Verify Installation

```python
from opencog.agentzero import AgentZeroCore, CognitiveLoop
from opencog.atomspace import AtomSpace

# Create AtomSpace
atomspace = AtomSpace()

# Initialize Agent-Zero
agent = AgentZeroCore(atomspace)
print(f"Agent initialized: {agent.is_initialized()}")
```

## Usage Examples

### Basic Agent Creation

```python
from opencog.agentzero import AgentZeroCore
from opencog.atomspace import AtomSpace, ConceptNode

# Create knowledge base
atomspace = AtomSpace()

# Initialize agent
agent = AgentZeroCore(atomspace)

# Add knowledge
goal = ConceptNode("LearnPython")
agent.add_goal(goal)

# Run cognitive cycle
agent.step()
```

### Task Management

```python
from opencog.agentzero import TaskManager
from opencog.atomspace import AtomSpace, ConceptNode

atomspace = AtomSpace()
task_manager = TaskManager(atomspace)

# Create hierarchical goals
main_goal = ConceptNode("BuildRobot")
sub_goal1 = ConceptNode("DesignBlueprint")
sub_goal2 = ConceptNode("AssembleParts")

# Decompose goals
task_manager.decompose_goal(main_goal, [sub_goal1, sub_goal2])

# Execute tasks
task_manager.execute_next_task()
```

### Knowledge Integration

```python
from opencog.agentzero import KnowledgeIntegrator
from opencog.atomspace import AtomSpace, ConceptNode, InheritanceLink

atomspace = AtomSpace()
integrator = KnowledgeIntegrator(atomspace)

# Query knowledge
pattern = ConceptNode("Animal")
results = integrator.query_subclasses(pattern)

# Update knowledge
new_concept = ConceptNode("Dog")
integrator.add_knowledge(InheritanceLink(new_concept, pattern))
```

### Cognitive Loop Control

```python
from opencog.agentzero import CognitiveLoop
from opencog.atomspace import AtomSpace

atomspace = AtomSpace()
loop = CognitiveLoop(atomspace)

# Configure loop parameters
loop.set_cycle_time(0.1)  # 100ms per cycle
loop.enable_reflection(True)

# Run loop
loop.start()

# Monitor progress
while loop.is_running():
    stats = loop.get_statistics()
    print(f"Cycles: {stats['cycles']}, Actions: {stats['actions']}")
    time.sleep(1)

loop.stop()
```

## Architecture

The Python bridge uses a multi-layer architecture:

1. **Cython Layer** (.pyx files): Low-level bindings to C++ classes
2. **Python Wrapper Layer** (.py files): High-level Pythonic API
3. **Helper Utilities**: Type conversion, error handling, and convenience functions

## Development

### Building from Source

```bash
# Install dependencies
pip install cython numpy

# Configure
cmake -DCMAKE_BUILD_TYPE=Release .

# Build
make -j$(nproc)

# Run tests
make test
```

### Running Tests

```bash
# Unit tests
python -m pytest tests/

# Integration tests
python -m pytest tests/integration/

# Specific test
python tests/test_agentzero_core.py
```

## Performance Considerations

- Cython bindings provide near-native C++ performance
- Minimize Python/C++ boundary crossings for optimal performance
- Use batch operations when working with multiple atoms
- Consider using AtomSpace callbacks for event-driven architectures

## Error Handling

The bridge provides proper Python exceptions for C++ errors:

```python
from opencog.agentzero import AgentZeroError

try:
    agent.execute_invalid_action()
except AgentZeroError as e:
    print(f"Agent error: {e}")
```

## Contributing

See the main [AGENT-ZERO-GENESIS.md](../../../AGENT-ZERO-GENESIS.md) for development guidelines.

## License

This code is part of the OpenCog project and follows the same licensing terms.

## Status

🚧 **Phase 10 Implementation** - This is part of the Agent-Zero-Genesis advanced features phase (AZ-HYBRID-001).

## Links

- [OpenCog Wiki](https://wiki.opencog.org/)
- [AtomSpace Python Documentation](https://wiki.opencog.org/w/Python)
- [Agent-Zero Documentation](../README.md)
