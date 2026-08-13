# agentzero-perception — Phase 2 Perception & Sensory Processing

OpenCog-oriented perception stack for Agent-Zero.

## Components

| Class | Role |
|-------|------|
| `MultiModalSensor` | Unified sensor interface (visual, auditory, tactile, textual, numeric, event, …) |
| `MockSensor` | In-process sensor for tests and demos |
| `PerceptualProcessor` | Encodes `SensoryInput` samples as AtomSpace concepts / EvaluationLinks |
| `AttentionManager` | ECAN-inspired STI allocation, decay, focus set, and spreading |
| `TextualSensor` | Streaming text ingestion with salience scoring and AtomSpace encoding |

Standalone (zero-dep) counterparts live under `include/` + `src/` (`cog0` namespace) and are exercised by `tests/test_perception.cpp` via the `standalone/` build. This module provides the OpenCog / AtomSpace-facing API under `opencog::agentzero`.

## Build

```bash
# Standalone module build (uses in-tree AtomSpace shim when OpenCog is absent)
cmake -S agentzero-perception -B build-agentzero-perception -DBUILD_TESTING=ON
cmake --build build-agentzero-perception
ctest --test-dir build-agentzero-perception --output-on-failure
```

When `cogutil` + `atomspace` are installed via pkg-config, the real OpenCog libraries are linked automatically. Optional `sensory`, `vision`, and `attention` packages enable `HAVE_*` compile definitions when present.

## Tests

All tests live in `agentzero-perception/tests/` and are registered with CTest under the `agentzero-perception` label.

Acceptance coverage:
- Sensors produce `SensoryInput` samples and invoke callbacks safely
- `PerceptualProcessor` creates AtomSpace handles tied to agent self
- `AttentionManager` STI scales with salience; decay and focus boundary work
- `TextualSensor` queue + sentence/word/document/stream modes
- End-to-end pipeline: sensor → processor → attention
