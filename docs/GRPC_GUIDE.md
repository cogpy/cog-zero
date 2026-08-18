# CogZero gRPC Agent Interface Guide

Phase 14 Feature 2.2 provides a typed **AgentService** API for inter-module and
external client communication. The canonical schema lives in
[`proto/agent.proto`](../proto/agent.proto).

## Service definition

```protobuf
service AgentService {
  rpc SetGoal(GoalRequest) returns (GoalResponse);
  rpc InjectPercept(PerceptRequest) returns (PerceptResponse);
  rpc RunCycles(RunRequest) returns (stream CycleStatus);
  rpc GetStatus(Empty) returns (AgentStatus);
  rpc QueryAtoms(AtomQuery) returns (AtomList);
}
```

| RPC | Purpose |
|-----|---------|
| `SetGoal` | Create / update a top-level goal |
| `InjectPercept` | Inject perceptual input into the cognitive loop |
| `RunCycles` | Run N synchronous cognitive cycles (streamed progress) |
| `GetStatus` | Snapshot name, running flag, atom/task counts, report |
| `QueryAtoms` | Filter atoms by name prefix and optional type |

## Build options

| CMake option | Default | Effect |
|--------------|---------|--------|
| `USE_GRPC` | `OFF` | When `ON`, probe for system gRPC/protobuf and define `COG0_HAVE_GRPC` if found |

Standalone builds **do not require gRPC**. When real gRPC is unavailable, CogZero
uses a **length-prefixed JSON transport** that mirrors the proto methods 1:1.

```bash
# Default: JSON AgentService fallback (zero extra deps)
cmake -S standalone -B build-standalone -DBUILD_TESTING=ON
cmake --build build-standalone

# Optional: probe for real gRPC
cmake -S standalone -B build-standalone -DUSE_GRPC=ON
```

## JSON transport (fallback)

### Framing

Each message is:

```
uint32 length (network byte order) | UTF-8 JSON payload
```

Maximum payload size: 16 MiB.

### Request

```json
{
  "method": "SetGoal",
  "params": {
    "name": "Explore",
    "description": "map the room",
    "priority": 0.9
  }
}
```

### Response

Success:

```json
{
  "ok": true,
  "result": { "...": "method-specific object" }
}
```

Failure:

```json
{
  "ok": false,
  "error": "human-readable message"
}
```

### Methods and params

| method | params | result highlights |
|--------|--------|-------------------|
| `SetGoal` | `name`, `description`, `priority` | `goal_id`, `message` |
| `InjectPercept` | `source`, `content`, `salience` | `message` |
| `RunCycles` | `cycles` | `statuses[]` with `cycle`, `total`, `phase`, `done`, `detail` |
| `GetStatus` | `{}` | `name`, `running`, `cycle_count`, `atom_count`, `pending_tasks`, `report` |
| `QueryAtoms` | `name_prefix`, `type`, `limit` | `atoms[]` with `type`, `name`, `sti`, `lti`, `strength`, `confidence` |

## C++ usage

### Server

```cpp
#include "cog0/Agent.h"
#include "cog0/GrpcAgentServer.h"

cog0::Agent agent;
cog0::GrpcAgentServer server(agent, /*port=*/50051);
server.start();
// ...
server.stop();
```

In-process dispatch (no sockets) is available for tests and embedding:

```cpp
std::string json = server.dispatchJson(
    R"({"method":"GetStatus","params":{}})");
```

### Client

```cpp
#include "cog0/GrpcAgentClient.h"

cog0::GrpcAgentClient client("127.0.0.1", 50051);
if (!client.connected()) {
    // check client.lastError()
}

std::string goalId, msg;
client.setGoal("Explore", "map room", 0.9, goalId, msg);

cog0::GrpcAgentStatus st;
client.getStatus(st);

std::vector<cog0::GrpcCycleStatus> cycles;
client.runCycles(3, cycles);
```

## Default port

`50051` (gRPC convention). Passing port `0` binds an ephemeral port; call
`server.port()` after `start()` to discover it.

## Testing

```bash
ctest --test-dir build-standalone -R phase14_grpc --output-on-failure
```

Coverage includes in-process dispatch, network round-trips, error paths, and
streamed `RunCycles` status.

## Future: real gRPC

When `USE_GRPC=ON` and gRPC/protobuf are found, `COG0_HAVE_GRPC` is defined and
`GrpcAgentServer::usingRealGrpc()` can report true once generated stubs are
wired. The JSON transport remains the supported default path for standalone
deployments without external dependencies.
