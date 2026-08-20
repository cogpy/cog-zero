# CogZero Development Roadmap

This document tracks the development status and planned work for the CogZero project.
For the high-level architecture specification see [AGENT-ZERO-GENESIS.md](AGENT-ZERO-GENESIS.md).

---

## ✅ Completed

### Standalone CLI Application (`standalone/`)
- **`cog0` binary** — fully self-contained, zero external dependencies (C++17 stdlib + threads only)
- **Interactive REPL** — `goal`, `task`, `percept`, `run`, `status`, `atoms`, `help`
- **rc-style script execution** (`--script <file>`) — autonomous-agency scripting from plain text files
- **Inline evaluation** (`--eval "<cmd>"`) — single-command execution for shell pipelines
- **AtomStore** — thread-safe, typed in-memory hypergraph knowledge store with TruthValues and STI/LTI attention
- **TaskManager** — priority-ordered task queue with sub-task support and goal tracking
- **ReasoningEngine** — forward-chaining inference with user-defined if/then rules
- **CognitiveLoop** — full perception → attention → reasoning → planning → action → reflection cycle
- **Background threading** — loop runs in a dedicated thread; percepts can be injected concurrently
- **Comprehensive test suite** — 143 tests (unit + e2e + integration + benchmarks + regression + perception), 0 failures

---

## Phase 1 — Foundation & Orchestration (OpenCog) ✅ Implemented

**Module:** `agentzero-core/`  
**Dependencies:** cogutil, atomspace, cogserver

### Goals
- Deep AtomSpace integration for all agent state
- CogServer module registration
- Full cognitive loop with OpenCog types

### Tasks
- [x] `AgentZeroCore` — main orchestration class backed by AtomSpace
- [x] `CognitiveLoop` — perception-action-reflection with AtomSpace handles
- [x] `TaskManager` — goal/task management using EvaluationLinks and StateLinks
- [x] `KnowledgeIntegrator` — AtomSpace bridge (semantic search, pattern mining)
- [x] `ActionExecutor` — action execution framework with status tracking
- [x] `ActionScheduler` — temporal action coordination
- [x] `MetaPlanner` — self-reflective planning optimisation
- [x] `ReasoningEngine` — PLN/URE integration with uncertainty via TruthValues
- [x] `SelfModification` — safe code-analysis and rollback framework

### Acceptance Criteria
- All `agentzero-core/tests/` pass under CTest
- Agent registers with CogServer and responds to module commands
- AtomSpace holds full agent state (goals, tasks, percepts, conclusions)

---

## Phase 2 — Perception & Sensory Processing ✅ Implemented

**Module:** `agentzero-perception/` + `standalone/`  
**Dependencies:** sensory, vision, perception *(OpenCog module)*; C++17 stdlib *(standalone)*

### Tasks
- [x] `MultiModalSensor` — unified interface for text, numeric, event, and visual inputs
- [x] `PerceptualProcessor` — converts raw inputs into AtomStore/AtomSpace representations
- [x] `AttentionManager` — ECAN-inspired attention allocation for incoming percepts
- [x] `TextualSensor` — streaming text ingestion with salience scoring

---

## Phase 3 — Knowledge Representation & Reasoning ✅ Implemented

**Module:** `agentzero-knowledge/`  
**Dependencies:** atomspace, pln *(optional)*, ure *(optional)*, miner *(optional)*

### Tasks
- [x] `KnowledgeBase` — extended AtomSpace operations (bulk load, SPARQL-like queries)
- [x] `PatternDiscovery` — unsupervised pattern mining over episode history
- [x] `ConceptFormation` — automatic concept creation and refinement
- [x] PLN rule library for common agent-reasoning patterns

---

## Phase 4 — Hierarchical Planning & Goal Management ✅ Implemented

**Module:** `agentzero-planning/`  
**Dependencies:** spacetime, cogserver

### Tasks
- [x] `GoalHierarchy` — goal tree with dependency tracking
- [x] `PlanningEngine` — STRIPS/HTN plan generation and execution
- [x] `TemporalReasoner` — temporal planning with deadline and sequencing support
- [x] `SpaceTimeIntegrator` — spacetime module bridge; trajectory planning, optimal scheduling
- [x] `MetaPlanner` enhancements — plan quality metrics and self-optimisation

---

## Phase 5 — Continuous Learning & Adaptation ✅ Implemented

**Module:** `agentzero-learning/`  
**Dependencies:** moses, asmoses, learn

### Tasks
- [x] `ExperienceManager` — episodic memory for agent trajectories
- [x] `SkillAcquisition` — learn reusable skills from experience
- [x] `PolicyOptimizer` — MOSES-based policy evolution
- [x] `MetaLearning` — learning-to-learn improvements with strategy selection and curriculum management
- [x] `ASMOSESIntegrator` — AtomSpace-backed MOSES evolutionary policy search
- [x] `AtomSpaceEvolver` — evolutionary computation directly over AtomSpace graphs

---

## Phase 6 — Communication & NLP ✅ Implemented

**Module:** `agentzero-communication/`  
**Dependencies:** lg-atomese, opencog NLP

### Tasks
- [x] `LanguageProcessor` — NLU/NLG with Link Grammar
- [x] `DialogueManager` — multi-turn conversation management
- [x] `AgentComms` — inter-agent messaging protocol
- [x] `HumanInterface` — user-facing dialogue layer
- [x] `MultiAgentCoordinator` — peer discovery, heartbeat/liveness, leader election, task hand-off, lightweight consensus
- [x] `MessageRouter` / `MessageSerializer` / `ProtocolManager` — flexible inter-agent transport layer

---

## Phase 7 — Memory & Context Management ✅ Implemented

**Module:** `agentzero-memory/`  
**Dependencies:** atomspace-rocks, attention

### Tasks
- [x] `EpisodicMemory` — temporal sequence storage and retrieval
- [x] `WorkingMemory` — active context window
- [x] `LongTermMemory` — persistent storage via AtomSpace-RocksDB backend
- [x] `ContextManager` — context-aware atom retrieval and relevance scoring

---

## Phase 8 — External Tool Integration ✅ Implemented

**Module:** `agentzero-tools/`  
**Dependencies:** external-tools, ros-behavior-scripting

### Tasks
- [x] `ToolRegistry` — dynamic tool discovery and capability catalogue
- [x] `ToolExecutor` — sandboxed tool invocation with result normalisation
- [x] REST API tool adapter (`RestApiAdapter`)
- [x] ROS behaviour scripting bridge (`RosBehaviorBridge`)

---

## Phase 9 — Integration & End-to-End Testing ✅ Implemented

### Tasks
- [x] Full system integration tests (all modules together)
  - Standalone: `tests/test_integration.cpp` (linked via `standalone/tests`)
  - Cross-module: `agentzero-integration/tests/test_full_system.cpp` (core+perception+knowledge+planning+learning+communication+memory+tools)
- [x] Performance benchmarks vs. baseline (< 100 ms routine decisions)
  - Standalone: `tests/test_benchmarks.cpp`
  - Cross-module: `agentzero-integration/tests/test_system_benchmarks.cpp`
- [x] Regression baseline establishment
  - Standalone: `tests/test_regression.cpp`
  - Cross-module: `agentzero-integration/tests/test_system_regression.cpp`
- [x] Python interoperability bridge (`agentzero-python-bridge/`)
  - C API: `include/cog0/cog0_capi.h` + `src/cog0_capi.cpp` → `libcog0_capi`
  - ctypes package: `agentzero-python-bridge/cog0/`
  - Optional Cython: `agentzero-python-bridge/cog0/_cog0.pyx`
- [x] Documentation pass: API reference (`docs/API_REFERENCE.md`), deployment guide (`docs/DEPLOYMENT_GUIDE.md` + `docs/DEPLOYMENT.md`)
- [x] Bug fix: `CognitiveLoop::start()` now safely joins any previous thread, enabling clean agent restart

---

## Phase 10 — Distributed Computing & Advanced Features ✅ Implemented

**Module:** `agentzero-distributed/`

### Tasks
- [x] `ClusterManager` — multi-node agent cluster coordination
- [x] `DistributedCoordinator` — consistent agent state across nodes
- [x] `LoadBalancer` — cognitive load distribution
- [x] Multi-agent coordination protocols (`CoordinationProtocol` — peer discovery, heartbeat/liveness, leader election, task hand-off, lightweight consensus)

---

## Phase 11 — Performance Profiling & Optimisation ✅ Implemented

**Module:** `profiling/`

### Tasks
- [x] `AgentZeroProfiler` — nanosecond-precision, RAII-based profiling framework
- [x] Hotspot identification with per-function mean/min/max/call-count statistics
- [x] RSS memory-delta tracking per profiling scope
- [x] AtomSpace operation profiling (node/link creation, queries)
- [x] Integration with external tools: gprof, perf, Valgrind/Callgrind
- [x] `PROFILING_GUIDE.md` — comprehensive 800-line usage and integration guide

---

## Near-Term Enhancements (Cross-Cutting)

### Standalone CLI (`standalone/`)
- [x] `--batch` mode: execute a script and emit JSON status for programmatic use
- [x] `save <file>` / `load <file>` commands: persist and restore AtomStore snapshots
- [x] `rule <name> if-exists <concept> then-add <concept>` command: register inference rules interactively
- [x] Colour output for interactive REPL with `--no-color` toggle
- [x] Tab-completion for interactive REPL (via `readline` optional dep; enabled when `libreadline-dev` is installed)
- [x] `goals` command: list all active goals with priority, description, and achievement status
- [x] `infer` command: manually trigger a forward-chaining inference pass and display which rules fired

### Testing
- [x] Property-based (fuzz) tests for AtomStore and ReasoningEngine
- [x] Benchmark suite integrated into `ctest -R benchmark`
- [x] CI workflow that builds and tests the standalone module on Linux + macOS

---

## Phase 12 — Advanced Coordination & Observability ✅ Implemented

### Goals
Extend the system with production-readiness features: advanced distributed-consensus
protocols, agent migration across nodes, and a real-time monitoring dashboard.

### Tasks
- [x] **Distributed consensus** — implement Raft-based leader election as an alternative to the current lightweight consensus protocol in `MultiAgentCoordinator` (`include/RaftConsensus.h`, `src/RaftConsensus.cpp`): in-process simulation via `RaftCluster` message bus; full leader election, log replication, and network-partition handling; pure C++17/stdlib — zero external deps
- [x] **Agent migration** — live hand-off of agent state and active tasks between cluster nodes (`ClusterManager` extension) *(completed in Phase 14)*
- [x] **Priority-based conflict resolution** — `ConflictResolver` (`include/ConflictResolver.h`, `src/ConflictResolver.cpp`): three strategies — `STRICT_PRIORITY`, `FAIRNESS_WEIGHTED` (anti-starvation via wait-time boost), and `SLA_PRIORITY` (EDF-inspired deadline urgency); resource-conflict detection; `ranked()` view
- [x] **Real-time monitoring dashboard** — `MonitoringServer` (`include/MonitoringServer.h`, `src/MonitoringServer.cpp`): lightweight HTTP/1.1 server (POSIX sockets, no external deps); endpoints `/health`, `/metrics`, `/atoms`, `/attention`; `AgentMetrics` snapshot API; extra-metrics hook for application-specific counters
- [x] **Structured logging** — `Logger` enhanced with JSON-lines mode (`setJsonMode(true)`), custom output sink (`setSink(ostream*)`), and `logJson(level, msg, fields)` for structured key/value logging compatible with ELK/Loki/Datadog pipelines
- [ ] **gRPC agent interface** — replace the current CogServer TCP protocol with a typed gRPC API *(deferred — requires external grpc dependency)*

### New test coverage
- `tests/test_phase12.cpp` — 25 tests covering Logger JSON mode, ConflictResolver all three strategies, Raft 3-node and 5-node election, MonitoringServer snapshot and lifecycle
- `tests/test_phase14_migration.cpp` — 29 tests covering Agent serialize/deserialize roundtrip, migration protocol, state preservation, and error handling
- `tests/test_action_executor_cog0.cpp` — 8 standalone ActionExecutor unit tests
- `tests/test_action_scheduler_cog0.cpp` — 8 standalone ActionScheduler unit tests
- Total: **190+ tests, 0 failures** across 13 CTest targets (unit, e2e, integration×3, regression, benchmark×2, fuzz×2, phase14_migration, phase14_websocket, cli smoke)
- Near-term testing (issue #13): property-based fuzz suites for AtomStore/ReasoningEngine (`ctest -L fuzz`), benchmark suite via `ctest -R benchmark`, and multi-OS standalone CI (Linux + macOS + Windows)

---

## Phase 13 — Agent-Driven Development & CI/CD Hardening ✅ Implemented

### Goals
Establish a self-describing GitHub Copilot agent (`cog0`) as the canonical orchestrating
agent for the repository. Harden the build system so the standalone target always succeeds
independent of OpenCog availability, and make the CI pipeline fully comprehensive with
labelled unit, e2e, integration, and benchmark test stages.

### Tasks
- [x] **cog0 agent definition** — `.github/agents/cog0.md`: GitHub Copilot agent definition declaring `cog0` as a hybrid AgentZero+OpenCog orchestrating agent; includes domain knowledge, responsibilities, and operating principles
- [x] **CMake graceful fallback** — root `CMakeLists.txt` now uses `BUILD_OPENCOG_MODULES` option; OpenCog dependencies emit `WARNING` instead of `FATAL_ERROR` when absent, ensuring `standalone/` always builds
- [x] **CTest labels** — `standalone/tests/CMakeLists.txt` assigns `unit`, `e2e`, `integration`, and `benchmark` labels to test targets, enabling `ctest --label-regex <label>` filtering
- [x] **Comprehensive CI workflow** — `.github/workflows/ci.yml` splits the test run into three explicit steps: *Unit Tests*, *E2E & Integration Tests*, and *Benchmarks*, each using CTest label filtering

---

## Phase 14 — Production Hardening & Advanced Interfaces ✅ Implemented

### Goals
Complete the remaining Phase 12 deferred items plus new production-readiness work.

### Tasks
- [x] **Agent migration** — live hand-off of agent state and active tasks between cluster nodes; implemented `Agent::serialize()` / `Agent::deserialize()` for full state roundtrip (atoms, goals, tasks, episodes); added `ClusterManager::initiateMigration()` / `receiveMigration()` / `getMigrationStatus()` for distributed migration protocol; 29 migration tests covering serialization roundtrip, state preservation, and error handling
- [x] **gRPC agent interface** — `proto/agent.proto` AgentService schema; `GrpcAgentServer` / `GrpcAgentClient` with length-prefixed JSON transport (zero-dep fallback); CMake `USE_GRPC` option probes for real gRPC when available; covers SetGoal, InjectPercept, RunCycles (streamed), GetStatus, QueryAtoms
- [x] **WebSocket monitoring dashboard** — upgrade `MonitoringServer` with a WebSocket upgrade path for real-time push metrics and a minimal HTML/JS frontend served at `/dashboard`; implemented `WebSocketHandler` (RFC 6455 frame encoding/decoding, SHA-1, Base64), `DashboardAssets` (self-contained responsive HTML/JS), integrated WebSocket client management and periodic metrics broadcast; 39 WebSocket tests
- [x] **Pluggable Raft log store** — implemented `RaftLogStore` abstract interface with `InMemoryLogStore` default implementation; integrated with `RaftNode` for state persistence (term, votedFor); factory function supports future RocksDB/SQLite backends; 21 persistence tests covering log operations, state recovery, and cluster integration
- [x] **TLS for MonitoringServer** — optional OpenSSL TLS via `TlsSocket` / `TlsContext`; `MonitoringServer::enableTLS(cert, key)`; `/health` reports `tls_enabled` / `tls_ready` / `tls_available`; CMake `USE_TLS` (default ON) defines `COG0_HAVE_OPENSSL` when found
- [x] **ToolWrapper stub completions** — replace placeholder implementations in `ToolWrapper.cpp` with functional code for REST API (POSIX sockets), Python script (popen), and shell command execution; added security validation and output size limits
- [x] **MessageSerializer stub completions** — implement zero-dependency recursive descent JSON parser (`parseJsonObject`, `parseJsonString`, `parseJsonValue`); add proper JSON string escaping; implement RLE compression/decompression; add ISO 8601 timestamp parsing

### New test coverage
- `tests/test_phase14_migration.cpp` — 29 tests covering Agent serialize/deserialize roundtrip, migration protocol, state preservation, and error handling
- `tests/test_phase14_websocket.cpp` — 39 tests covering WebSocket upgrade detection, frame encoding/decoding, SHA-1/Base64, and dashboard assets
- `tests/test_phase14_raft_persistence.cpp` — 21 tests covering RaftLogStore operations, state persistence/recovery, and RaftNode integration
- `tests/test_phase14_grpc.cpp` — AgentService dispatch + network client/server round-trips
- `tests/test_phase14_tls.cpp` — TlsContext loading, MonitoringServer TLS flags, HTTPS health scrape
- Total: **230+ tests, 0 failures** across 14 CTest targets

### Documentation
- `docs/MIGRATION_GUIDE.md` — Agent migration procedures and API reference
- `docs/DASHBOARD_GUIDE.md` — WebSocket monitoring dashboard usage and configuration
- `docs/GRPC_GUIDE.md` — AgentService proto, JSON fallback transport, C++ client/server usage
- `docs/PRODUCTION_DEPLOYMENT.md` — TLS, monitoring, clustering, and security checklist

---

## Phase 15 — Packaging & Release Targets ✅ Implemented

### Goals
Ship installable artifacts for all primary release targets (standalone-first),
with a single product version, CMake install components, CPack archives,
container images, Python MVP packaging, and tag-driven GitHub Releases.

### Release targets

| ID | Target | Status |
|----|--------|--------|
| RT1 | Standalone runtime (`cog0` binary) | ✅ CPack TGZ/ZIP/DEB, install `Runtime` |
| RT2 | Standalone SDK (`libcog0.a`, headers, `Cog0` CMake, `cog0.pc`) | ✅ install `Development` + consumer smoke |
| RT3 | Python `cog0` | ✅ pure-Python sdist/wheel MVP + CI/release smoke |
| RT4 | OpenCog libraries | ✅ install/export when OpenCog present (source track) |
| RT5 | Containers | ✅ multi-stage `Dockerfile` + GHCR workflow |
| RT6 | System packages | ✅ TGZ/ZIP always; DEB (+ RPM when `rpmbuild` present) |
| RT7 | Metadata | ✅ `VERSION`, `CHANGELOG.md`, `SHA256SUMS` on release |

### Tasks
- [x] Unified `VERSION` + `cmake/CogZeroVersion.cmake` consumed by root, standalone, Python, CPack, containers
- [x] Install components: `Runtime`, `Development`, `Python`, `OpenCog`
- [x] Standalone header install under `include/cog0/`, `Cog0Config.cmake` / pkg-config
- [x] `cmake/CogZeroCPack.cmake` shared CPack module
- [x] `scripts/packaging_smoke.sh` — DESTDIR install, `find_package(Cog0)`, TGZ, Linux DEB extract
- [x] `scripts/python_packaging_smoke.sh` — sdist/wheel build + venv import/version check
- [x] `scripts/package.sh` — local Release packaging helper
- [x] `.github/workflows/release.yml` — tag validation, multi-OS artifacts, Python sdist/wheel, SHA256SUMS, GH release
- [x] `.github/workflows/docker.yml` + `Dockerfile` — GHCR `ghcr.io/cogpy/cog-zero`
- [x] CI packaging + Python packaging smoke jobs
- [x] Docs: `docs/PACKAGING.md`, `docs/RELEASE.md`, `docs/VERSIONING.md`

### Follow-ups (not blocking)
- Binary wheels bundling `libcog0_capi` (cibuildwheel)
- OpenCog binary packages in CI (needs containerized OpenCog deps)
- Secondary channels: Homebrew, Snap, Conan/vcpkg, conda-forge, MSI, Helm

### Documentation
- `docs/PACKAGING.md` — targets, components, local package commands
- `docs/RELEASE.md` — operator runbook for cutting a release
- `docs/VERSIONING.md` — semver + SOVERSION policy

---

| Version | Highlights |
|---------|-----------|
| **0.5.0** | Phase 14: Agent migration (`serialize`/`deserialize`); WebSocket dashboard (`/dashboard`); pluggable `RaftLogStore`; AgentService gRPC/JSON API; MonitoringServer TLS; `ToolWrapper` REST/Python/Shell; `MessageSerializer` JSON parser; 230+ tests (14 CTest targets) |
| **0.4.0** | Phase 12: `RaftConsensus` (Raft leader election, pure C++17); `ConflictResolver` (STRICT_PRIORITY / FAIRNESS_WEIGHTED / SLA_PRIORITY); `MonitoringServer` (HTTP /health /metrics /atoms /attention); `Logger` JSON-lines structured logging; 161 tests (7 CTest targets); CTest discoverable from root build dir |
| **3.1.0** | Phase 13 + Phase 15 packaging: `cog0` agent definition; CMake graceful fallback; CTest labels; CI; unified `VERSION`/CPack/install components; release + Docker workflows; Python wheel MVP |
| **0.2.0** | Phases 4–7 (Planning, Learning, Communication, Memory) marked complete; Phase 11 (Profiling) implemented; `goals` and `infer` REPL commands; Phase 12 roadmap defined; 147 tests |
| **0.1.3** | Phase 2 Perception: `MultiModalSensor`, `PerceptualProcessor`, `AttentionManager`, `TextualSensor` — 36 new tests (143 total); CI workflow complete |
| **0.1.2** | ANSI colour output with `--no-color` toggle; readline tab-completion (optional); ROADMAP near-term enhancements complete |
| **0.1.1** | Phase 9: 30 new tests (integration, benchmarks, regression); C API for Python bridge; API reference & deployment guide; `CognitiveLoop` restart bug fix |
| **0.1.0** | Standalone CLI (`cog0`) with REPL, scripting (`--script`), `--eval`; 49-test suite (unit + e2e) |
| **0.0.1** | Initial repository structure; module skeletons; build system |

---

*Last updated: 2026-08-20
