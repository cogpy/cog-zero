# agentzero-knowledge — Phase 3 Knowledge Representation & Reasoning

OpenCog-oriented knowledge and reasoning stack for Agent-Zero.

## Components

| Class | Role |
|-------|------|
| `KnowledgeBase` | Extended AtomSpace operations: bulk load, SPARQL-like triple queries, TV filters |
| `PatternDiscovery` | Unsupervised pattern mining over episode history (Miner fallback when absent) |
| `ConceptFormation` | Automatic concept creation/refinement (COBWEB/CLASSIT-inspired) |
| `PLNRuleLibrary` | PLN rule library for agent reasoning (deduction, modus ponens, abduction, …) |

Headers live under `include/opencog/agentzero/knowledge/` (namespace `opencog::agentzero::knowledge`). Standalone/zero-dep counterparts may also appear under `include/cog0/`.

## Build

```bash
# Standalone module build (uses in-tree AtomSpace shim when OpenCog is absent)
cmake -S agentzero-knowledge -B build-agentzero-knowledge -DBUILD_TESTING=ON
cmake --build build-agentzero-knowledge
ctest --test-dir build-agentzero-knowledge --output-on-failure
```

When `cogutil` + `atomspace` are installed via pkg-config, the real OpenCog libraries are linked automatically. Optional `pln`, `ure`, and `miner` packages enable `HAVE_*` compile definitions when present; built-in fallbacks cover the same APIs without them.

## Tests

All tests live in `agentzero-knowledge/tests/` and are registered with CTest under the `agentzero-knowledge` label.

Acceptance coverage:
- KnowledgeBase bulk load (triples, adjacency, file) and SPARQL-like queries
- PatternDiscovery episode corpus + frequency/surprisingness mining
- ConceptFormation observe → form → merge/split → hierarchy
- PLNRuleLibrary builtin rules, TV formulas, forward/backward chaining
- End-to-end pipeline integrating all four components on one AtomSpace

## Dependencies

- **Required (or shim):** AtomSpace API (`cogutil` + `atomspace`, or in-tree shim)
- **Optional:** `pln`, `ure`, `miner`
