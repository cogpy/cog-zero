# Agent-Zero Python Bridge API Reference

## Core Classes

### AgentZeroCore

Main orchestration engine for Agent-Zero.

```python
from opencog.atomspace import AtomSpace
from opencog.agentzero import AgentZeroCore

atomspace = AtomSpace()
core = AgentZeroCore(atomspace)
core.initialize()
```

#### Methods

- `initialize() -> bool`: Initialize the core engine
- `is_initialized() -> bool`: Check initialization status
- `shutdown()`: Shutdown the core gracefully
- `step() -> bool`: Execute one cognitive cycle
- `add_goal(goal_handle) -> bool`: Add a goal
- `remove_goal(goal_handle) -> bool`: Remove a goal
- `get_active_goals() -> list`: Get active goals
- `get_status() -> str`: Get status string
- `set_config(key: str, value: str)`: Set configuration
- `get_config(key: str) -> str`: Get configuration

---

### CognitiveLoop

Implements the perception-action-reflection cycle.

```python
from opencog.agentzero import CognitiveLoop

loop = CognitiveLoop(atomspace)
loop.set_cycle_time(0.1)
loop.start()
```

#### Methods

- `start() -> bool`: Start the cognitive loop
- `stop() -> bool`: Stop the cognitive loop
- `is_running() -> bool`: Check if running
- `step()`: Execute one cycle manually
- `set_cycle_time(seconds: float)`: Set cycle time
- `get_cycle_time() -> float`: Get cycle time
- `enable_reflection(enable: bool)`: Enable/disable reflection
- `is_reflection_enabled() -> bool`: Check reflection status
- `get_statistics() -> dict`: Get execution statistics

#### Context Manager

```python
with CognitiveLoop(atomspace) as loop:
    # Loop runs automatically
    pass
# Stopped automatically
```

---

### TaskManager

Handles goal decomposition and task execution.

```python
from opencog.agentzero import TaskManager

tm = TaskManager(atomspace)
tm.decompose_goal(main_goal, [subgoal1, subgoal2])
tm.execute_next_task()
```

#### Methods

- `decompose_goal(goal, subgoals: list) -> bool`: Decompose goal
- `execute_next_task() -> bool`: Execute next pending task
- `execute_task(task) -> bool`: Execute specific task
- `get_pending_tasks() -> list`: Get pending tasks
- `get_completed_tasks() -> list`: Get completed tasks
- `is_goal_achieved(goal) -> bool`: Check if goal achieved
- `clear_completed()`: Clear completed tasks list
- `get_task_status(task) -> str`: Get task status

#### Properties

- `pending_count`: Number of pending tasks
- `completed_count`: Number of completed tasks

---

### KnowledgeIntegrator

Manages AtomSpace knowledge operations.

```python
from opencog.agentzero import KnowledgeIntegrator

ki = KnowledgeIntegrator(atomspace)
ki.add_knowledge(atom)
results = ki.query_subclasses(concept)
```

#### Methods

- `add_knowledge(atom) -> bool`: Add knowledge
- `remove_knowledge(atom) -> bool`: Remove knowledge
- `query_pattern(pattern) -> list`: Query by pattern
- `query_subclasses(concept) -> list`: Query subclasses
- `query_instances(concept) -> list`: Query instances
- `infer(query) -> bool`: Perform inference
- `get_truth_value(atom) -> float`: Get truth value
- `set_truth_value(atom, strength: float, confidence: float)`: Set truth value
- `query(pattern, query_type: str) -> list`: General query

---

### AgentZero

High-level Pythonic API for Agent-Zero.

```python
from opencog.agentzero import AgentZero

agent = AgentZero()
agent.add_goal("LearnPython", priority=0.9)
agent.run(cycles=10)
```

#### Methods

- `__init__(atomspace=None, auto_start=False)`: Create agent
- `start()`: Start cognitive loop
- `stop()`: Stop cognitive loop
- `step() -> bool`: Execute one cycle
- `run(cycles=None, duration=None)`: Run for cycles or duration
- `add_goal(name: str, priority: float) -> Handle`: Add goal
- `remove_goal(name: str) -> bool`: Remove goal
- `add_knowledge(statement)`: Add knowledge
- `query(pattern) -> list`: Query knowledge
- `configure(**kwargs)`: Configure parameters
- `shutdown()`: Shutdown gracefully

#### Properties

- `status`: Current status string
- `is_running`: Whether agent is running
- `goals`: Active goals list
- `pending_tasks`: Pending tasks list
- `statistics`: Execution statistics

#### Context Manager

```python
with AgentZero() as agent:
    agent.add_goal("MyGoal")
    agent.run(cycles=5)
# Automatically shut down
```

---

## Utility Functions

### create_goal

```python
from opencog.agentzero.utils import create_goal

goal = create_goal("LearnPython", "Learn Python programming", priority=0.9)
```

### create_task

```python
from opencog.agentzero.utils import create_task

task = create_task("InstallPython", goal=main_goal)
```

### query_knowledge

```python
from opencog.agentzero.utils import query_knowledge

results = query_knowledge(atomspace, "Animal", query_type="subclasses")
```

---

## Exception Classes

All exceptions inherit from `AgentZeroError`:

- `AgentZeroRuntimeError`: Runtime errors
- `AgentZeroConfigError`: Configuration errors
- `AgentZeroInitializationError`: Initialization errors
- `AgentZeroTaskError`: Task execution errors
- `AgentZeroKnowledgeError`: Knowledge operation errors

Example:

```python
from opencog.agentzero.exceptions import AgentZeroError

try:
    agent.execute_invalid_operation()
except AgentZeroError as e:
    print(f"Error: {e}")
```

---

## Complete Example

```python
from opencog.atomspace import AtomSpace, ConceptNode
from opencog.agentzero import AgentZero

# Create agent
with AgentZero() as agent:
    # Add goals
    goal1 = agent.add_goal("LearnProgramming", priority=0.9)
    goal2 = agent.add_goal("BuildProject", priority=0.7)
    
    # Add knowledge
    from opencog.atomspace import InheritanceLink
    python = ConceptNode("Python")
    language = ConceptNode("ProgrammingLanguage")
    agent.add_knowledge(InheritanceLink(python, language))
    
    # Run agent
    agent.run(cycles=10)
    
    # Check results
    print(f"Status: {agent.status}")
    print(f"Statistics: {agent.statistics}")
```
