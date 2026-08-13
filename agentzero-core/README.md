# agentzero-core — Phase 1 Foundation & Orchestration

OpenCog-oriented orchestration engine for Agent-Zero.

## Components

| Class | Role |
|-------|------|
| `AgentZeroCore` | Main orchestrator; optional CogServer `Module` |
| `CognitiveLoop` | Perception → reasoning → planning → action → reflection |
| `TaskManager` | Goals/tasks as EvaluationLinks + StateLinks |
| `KnowledgeIntegrator` | Semantic search and lightweight pattern mining |
| `ActionExecutor` | Action execution with AtomSpace status tracking |
| `ActionScheduler` | Temporal coordination of actions |
| `MetaPlanner` | Self-reflective planning optimisation |
| `ReasoningEngine` | Forward/backward chaining with TruthValues |
| `SelfModification` | Safe code-analysis and rollback framework |

## Build

```bash
# Standalone module build (uses in-tree AtomSpace shim when OpenCog is absent)
cmake -S agentzero-core -B build-agentzero-core -DBUILD_TESTING=ON
cmake --build build-agentzero-core
ctest --test-dir build-agentzero-core --output-on-failure
```

When `cogutil` + `atomspace` (+ optional `cogserver`) are installed via pkg-config,
the real OpenCog libraries are linked automatically.

## Tests

All tests live in `agentzero-core/tests/` and are registered with CTest under the
`agentzero-core` label.

Acceptance coverage:
- AtomSpace holds agent self, goals, tasks, percepts, conclusions
- Cognitive loop single-cycle and background start/stop
- CogServer module registration and command surface (`agent-status`, `agent-start`, …)
