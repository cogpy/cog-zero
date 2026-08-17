# agentzero-communication — Phase 6 Communication & NLP

OpenCog-oriented communication and natural-language stack for Agent-Zero.

## Components

| Class | Role |
|-------|------|
| `LanguageProcessor` | NLU/NLG with optional Link Grammar (`lg-atomese`); rule-based fallback |
| `DialogueManager` | Multi-turn conversation management with context, topics, and goals |
| `AgentComms` | Inter-agent messaging protocol (send/receive/broadcast, handlers) |
| `HumanInterface` | User-facing dialogue layer with session management |

Headers live under `include/opencog/agentzero/communication/` (namespace `opencog::agentzero::communication`).

## Build

```bash
# Standalone module build (uses in-tree AtomSpace shim when OpenCog is absent)
cmake -S agentzero-communication -B build-agentzero-communication -DBUILD_TESTING=ON
cmake --build build-agentzero-communication
ctest --test-dir build-agentzero-communication --output-on-failure
```

When `cogutil` + `atomspace` are installed via pkg-config, the real OpenCog libraries are linked automatically. Optional `lg-atomese` enables `HAVE_LG_ATOMESE` for Link Grammar integration; a deterministic rule-based NLP fallback covers the same APIs without it.

## Tests

All tests live in `agentzero-communication/tests/` and are registered with CTest under the `agentzero-communication` label.

Acceptance coverage:
- LanguageProcessor parse/intent/entity/response and AtomSpace round-trip
- DialogueManager multi-turn history, context, topics, goals
- AgentComms peer registry, loopback send/receive, broadcast + dispatch
- HumanInterface sessions and formatted replies
- End-to-end pipeline integrating all components on one AtomSpace

## Dependencies

- **Required (or shim):** AtomSpace API (`cogutil` + `atomspace`, or in-tree shim)
- **Optional:** `lg-atomese` (Link Grammar / OpenCog NLP)
