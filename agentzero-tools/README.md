# agentzero-tools — Phase 8 External Tool Integration

OpenCog-oriented external tool integration stack for Agent-Zero.

## Components

| Class | Role |
|-------|------|
| `ToolRegistry` | Dynamic tool discovery and capability catalogue |
| `ToolWrapper` | Unified interface for external tool types |
| `ToolExecutor` | Sandboxed tool invocation with result normalisation |
| `RestApiAdapter` | REST API tool adapter (POSIX sockets, no libcurl) |
| `RosBehaviorBridge` | ROS behaviour scripting bridge (simulation fallback) |
| `CapabilityComposer` | Combines tools/capabilities for complex tasks |
| `ResourceManager` | Computational and physical resource management |

Headers live under `include/opencog/agentzero/` and
`include/opencog/agentzero/tools/` (namespaces `opencog::agentzero` and
`opencog::agentzero::tools`).

## Build

```bash
# Standalone module build (uses in-tree AtomSpace shim when OpenCog is absent)
cmake -S agentzero-tools -B build-agentzero-tools -DBUILD_TESTING=ON
cmake --build build-agentzero-tools
ctest --test-dir build-agentzero-tools --output-on-failure
```

When `cogutil` + `atomspace` are installed via pkg-config, the real OpenCog
libraries are linked automatically. Optional packages:

- `external-tools` — enables `HAVE_EXTERNAL_TOOLS` discovery hooks
- `ros-behavior-scripting` — enables `HAVE_ROS_BEHAVIOR_SCRIPTING` live ROS links

Without those packages the same public APIs remain available with built-in
discovery and ROS simulation-mode fallbacks.

## Tests

All tests live in `agentzero-tools/tests/` and are registered with CTest under
the `agentzero-tools` label.

Acceptance coverage:
- ToolRegistry register/discover/category/capability/execute
- ToolExecutor normalise/sandbox policy/command allowlist
- RestApiAdapter config/JSON/response atoms/unreachable endpoint handling
- RosBehaviorBridge connect/publish/subscribe/simulate/behavior tree
- End-to-end pipeline integrating registry, executor, REST, ROS, capabilities, resources

## Dependencies

- **Required (or shim):** AtomSpace API (`cogutil` + `atomspace`, or in-tree shim)
- **Optional:** `external-tools`, `ros-behavior-scripting`
