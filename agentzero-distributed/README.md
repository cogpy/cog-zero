# agentzero-distributed — Phase 10 Distributed Computing

OpenCog-oriented multi-node coordination stack for Agent-Zero.

## Components

| Class | Role |
|-------|------|
| `ClusterManager` | Multi-node cluster membership, health, capacity, migration hooks |
| `DistributedCoordinator` | Consistent task/state coordination across compute nodes |
| `LoadBalancer` | Cognitive load distribution (round-robin, least-loaded, weighted, affinity, locality) |
| `CoordinationProtocol` | Peer discovery, heartbeat/liveness, leader election, task hand-off, lightweight consensus |

Headers live under `include/opencog/agentzero/distributed/`
(namespace `opencog::agentzero`).

## Build

```bash
# Standalone module build (uses in-tree AtomSpace shim when OpenCog is absent)
cmake -S agentzero-distributed -B build-agentzero-distributed -DBUILD_TESTING=ON
cmake --build build-agentzero-distributed
ctest --test-dir build-agentzero-distributed --output-on-failure
```

When `cogutil` + `atomspace` are installed via pkg-config, the real OpenCog
libraries are linked automatically. Without them the same public APIs remain
available against the in-tree AtomSpace shim from `agentzero-core/shim`.

## Tests

All tests live in `agentzero-distributed/tests/` and are registered with CTest
under the `agentzero-distributed` label.

Acceptance coverage:
- ClusterManager add/remove/health/capacity/capabilities
- DistributedCoordinator register/submit/stats/shutdown
- LoadBalancer strategies (least-loaded, round-robin, empty cluster)
- CoordinationProtocol discovery, heartbeat, election, hand-off, consensus
- End-to-end pipeline integrating cluster + coordinator + balancer + protocol

## Dependencies

- **Required (or shim):** AtomSpace API (`cogutil` + `atomspace`, or in-tree shim)
- **Required:** C++17, Threads
