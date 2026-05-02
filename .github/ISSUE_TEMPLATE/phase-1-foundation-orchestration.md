---
name: "Phase 1 — Foundation & Orchestration"
about: "Track implementation of the agentzero-core orchestration module"
title: "Phase 1 — Foundation & Orchestration (agentzero-core)"
labels: enhancement
assignees: ''
---

## Phase 1 — Foundation & Orchestration 🚧 In Progress

**Module:** `agentzero-core/`
**Dependencies:** cogutil, atomspace, cogserver

### Goals
- Deep AtomSpace integration for all agent state
- CogServer module registration
- Full cognitive loop with OpenCog types

### Tasks
- [ ] `AgentZeroCore` — main orchestration class backed by AtomSpace
- [ ] `CognitiveLoop` — perception-action-reflection with AtomSpace handles
- [ ] `TaskManager` — goal/task management using EvaluationLinks and StateLinks
- [ ] `KnowledgeIntegrator` — AtomSpace bridge (semantic search, pattern mining)
- [ ] `ActionExecutor` — action execution framework with status tracking
- [ ] `ActionScheduler` — temporal action coordination
- [ ] `MetaPlanner` — self-reflective planning optimisation
- [ ] `ReasoningEngine` — PLN/URE integration with uncertainty via TruthValues
- [ ] `SelfModification` — safe code-analysis and rollback framework

### Acceptance Criteria
- All `agentzero-core/tests/` pass under CTest
- Agent registers with CogServer and responds to module commands
- AtomSpace holds full agent state (goals, tasks, percepts, conclusions)

---
*Tracked in [ROADMAP.md](https://github.com/ReZorg/cog0/blob/main/ROADMAP.md)*
