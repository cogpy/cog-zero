# agentzero-planning — Phase 4 Hierarchical Planning & Goal Management

OpenCog-oriented hierarchical planning stack for Agent-Zero.

## Components

| Class | Role |
|-------|------|
| `GoalHierarchy` | Goal tree with parent/child links, prerequisite dependencies, hierarchical achievement |
| `PlanningEngine` | STRIPS operators + HTN methods, plan generation, execution, quality metrics |
| `TemporalReasoner` | Intervals, deadlines, ordering constraints, schedule optimisation |
| `SpaceTimeIntegrator` | Trajectory planning and optimal time windows (native spacetime or in-process fallback) |
| `MetaPlanner` | Plan quality tracking, strategy selection, self-optimisation |

Headers live under `include/opencog/agentzero/planning/` (namespace `opencog::agentzero::planning`).

## Build

```bash
# Standalone module build (uses in-tree AtomSpace shim when OpenCog is absent)
cmake -S agentzero-planning -B build-agentzero-planning -DBUILD_TESTING=ON
cmake --build build-agentzero-planning
ctest --test-dir build-agentzero-planning --output-on-failure
```

When `cogutil` + `atomspace` are installed via pkg-config, the real OpenCog libraries are linked automatically. Optional `spacetime` enables `HAVE_SPACETIME` for native TimeOctomap integration; a lightweight occupancy/timeline fallback covers the same APIs without it.

## Tests

All tests live in `agentzero-planning/tests/` and are registered with CTest under the `agentzero-planning` label.

Acceptance coverage:
- GoalHierarchy tree, dependencies, activation, hierarchical achievement
- PlanningEngine STRIPS plan/execute and HTN decomposition
- TemporalReasoner intervals, deadlines, schedule optimisation
- SpaceTimeIntegrator insert/query, trajectory, optimal window
- MetaPlanner quality recording and plan-and-learn loop
- End-to-end pipeline integrating all components on one AtomSpace

## Dependencies

- **Required (or shim):** AtomSpace API (`cogutil` + `atomspace`, or in-tree shim)
- **Optional:** `spacetime` (trajectory map backend)
