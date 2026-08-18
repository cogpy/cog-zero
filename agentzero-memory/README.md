# agentzero-memory — Phase 7 Memory & Context Management

OpenCog-oriented memory stack for Agent-Zero.

## Components

| Class | Role |
|-------|------|
| `EpisodicMemory` | Temporal sequence storage and retrieval |
| `WorkingMemory` | Active context window with decay and capacity limits |
| `LongTermMemory` | Persistent storage (optional AtomSpace-RocksDB backend) |
| `ContextManager` | Context-aware atom retrieval and relevance scoring |

Headers live under `include/opencog/agentzero/` and `include/opencog/agentzero/memory/`
(namespaces `opencog::agentzero` and `opencog::agentzero::memory`).

## Build

```bash
# Standalone module build (uses in-tree AtomSpace shim when OpenCog is absent)
cmake -S agentzero-memory -B build-agentzero-memory -DBUILD_TESTING=ON
cmake --build build-agentzero-memory
ctest --test-dir build-agentzero-memory --output-on-failure
```

When `cogutil` + `atomspace` are installed via pkg-config, the real OpenCog
libraries are linked automatically. Optional packages:

- `attention` — enables `HAVE_ATTENTION` / ECAN synchronisation hooks
- `atomspace-rocks` — enables `HAVE_ATOMSPACE_ROCKS` durable persistence

Without those packages the same public APIs remain available with in-memory
fallbacks.

## Tests

All tests live in `agentzero-memory/tests/` and are registered with CTest under
the `agentzero-memory` label.

Acceptance coverage:
- EpisodicMemory store/retrieve/temporal/context/capacity
- WorkingMemory add/access/context/importance/cleanup
- LongTermMemory store/query/consolidate/backup
- ContextManager create/switch/relevance scoring
- End-to-end pipeline integrating all four components on one AtomSpace

## Dependencies

- **Required (or shim):** AtomSpace API (`cogutil` + `atomspace`, or in-tree shim)
- **Optional:** `attention`, `atomspace-rocks`
