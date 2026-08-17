# agentzero-learning — Phase 5 Continuous Learning & Adaptation

OpenCog-oriented continuous learning stack for Agent-Zero.

## Components

| Class | Role |
|-------|------|
| `ExperienceManager` | Episodic memory for agent trajectories (record, query, consolidate) |
| `SkillAcquisition` | Extract and refine reusable skills from experience |
| `PolicyOptimizer` | MOSES-based (or GA-fallback) policy evolution |
| `MetaLearning` | Learning-to-learn: strategy selection, hyper-parameter adaptation, sub-system coordination |

Headers live under `include/opencog/agentzero/` (namespace `opencog::agentzero`).

## Build

```bash
# Standalone module build (uses in-tree AtomSpace shim when OpenCog is absent)
cmake -S agentzero-learning -B build-agentzero-learning -DBUILD_TESTING=ON
cmake --build build-agentzero-learning
ctest --test-dir build-agentzero-learning --output-on-failure
```

When `cogutil` + `atomspace` are installed via pkg-config, the real OpenCog libraries are linked automatically. Optional `moses` enables `HAVE_MOSES` for native MOSES policy evolution; a built-in genetic-algorithm fallback covers the same APIs without it.

## Tests

All tests live in `agentzero-learning/tests/` and are registered with CTest under the `agentzero-learning` label.

Acceptance coverage:
- ExperienceManager record/query/similarity/consolidation
- SkillAcquisition learn/refine/transfer from experiences
- PolicyOptimizer seed/evolve/evaluate with GA fallback
- MetaLearning processExperience, strategy selection, adaptation
- End-to-end pipeline integrating all components on one AtomSpace

## Dependencies

- **Required (or shim):** AtomSpace API (`cogutil` + `atomspace`, or in-tree shim)
- **Optional:** `moses` (native evolutionary policy search)
