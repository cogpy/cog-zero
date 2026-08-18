# CogZero - Agent-Zero C++ Implementation for OpenCog

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL%203.0-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![OpenCog](https://img.shields.io/badge/OpenCog-Integration-green.svg)](https://opencog.org)
[![Tests](https://img.shields.io/badge/tests-210%2B%20passing-brightgreen.svg)](#-testing)

**CogZero** is a high-performance C++ implementation of Agent-Zero, specially optimized for integration with the OpenCog cognitive architecture. It serves as an orchestration workbench system that provides a modular catalog of powerful cognitive tools, skills, abilities, and knowledge enhancements.

## 🚀 Standalone CLI — Quick Start (No OpenCog Required)

The `standalone/` directory contains a fully self-contained CLI application (`cog0`) that runs the complete cognitive loop with **no external dependencies** beyond a C++17 compiler.

### Build

```bash
cd standalone
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Run

```bash
# Interactive REPL
./cog0

# Built-in demo
./cog0 --demo

# Run N cognitive cycles and print status
./cog0 --cycles 10

# Execute an rc-style script file
./cog0 --script my_agent.cog0

# Execute command(s) inline ('; separates commands)
./cog0 --eval "goal explore-world Explore the environment; run 3"

# Batch / programmatic mode — JSON status on stdout, no prompts
./cog0 --batch --script my_agent.cog0
./cog0 --batch --eval "goal patrol; run 5"

# Disable ANSI colour (also auto-disabled in --batch / non-TTY)
./cog0 --no-color
```

### Interactive Commands

```
cog0> goal  <name> [description]   Set a new goal
cog0> goals                        List all goals with priority and status
cog0> task  <name> [description]   Schedule a task
cog0> percept <text>               Inject a text percept
cog0> run [N]                      Run N cognitive cycles (default: 1)
cog0> infer                        Run one forward-chaining inference pass
cog0> status                       Print agent status
cog0> atoms                        List all atoms in the knowledge store
cog0> save  <file>                 Save AtomStore snapshot to file
cog0> load  <file>                 Load AtomStore snapshot from file
cog0> rule  <name> if-exists <concept> then-add <concept>
                                   Register an inference rule
cog0> help                         Show command reference
cog0> quit                         Exit
```

Tab-completion of command names is available when built with GNU readline
(`USE_READLINE=ON`, the default when `libreadline-dev` is installed).

### Script Files (rc-style)

`cog0` accepts plain-text script files — one command per line, `#` for comments.
This enables autonomous-agency scripting without an interactive terminal:

```
# my_agent.cog0
goal learn-about-opencog Understand the OpenCog ecosystem
percept AtomSpace is a hypergraph knowledge store
task summarise-components Summarise key OpenCog components
run 5
save state.snap
status
```

```bash
./cog0 --script my_agent.cog0
./cog0 --batch --script my_agent.cog0   # JSON status for CI / tooling
```

### Run Tests

```bash
# From the standalone/build directory
ctest -V
# → 147 tests passed (unit + e2e + integration + benchmarks + regression + perception)
```

---

## 🎯 Project Vision

CogZero implements the **AGENT-ZERO-GENESIS** initiative, creating a C++ variant of Agent-Zero that provides optimal grip and maximum effectiveness within OpenCog's cognitive architecture. The system creates a well-orchestrated and modular catalog of powerful tools, skills, abilities and knowledge enhancements as a coherent and integrated whole.

## 🏗️ Architecture

CogZero is organized into 9 specialized modules, each optimized for specific cognitive functions:

### Core Modules

| Module | Purpose | Key Dependencies |
|--------|---------|------------------|
| **[agentzero-core](agentzero-core/)** | Main orchestration engine & cognitive loop | cogutil, atomspace, cogserver |
| **[agentzero-perception](agentzero-perception/)** | Multi-modal sensory processing | sensory, vision, perception |
| **[agentzero-planning](agentzero-planning/)** | Hierarchical planning & goal management | spacetime, cogserver |
| **[agentzero-learning](agentzero-learning/)** | Continuous learning & adaptation | moses, asmoses, learn |
| **[agentzero-memory](agentzero-memory/)** | Memory & context management | atomspace-rocks, attention |
| **[agentzero-communication](agentzero-communication/)** | NLP & multi-agent communication | lg-atomese, opencog |
| **[agentzero-tools](agentzero-tools/)** | External tool integration | external-tools, ros-behavior-scripting |
| **[agentzero-distributed](agentzero-distributed/)** | Distributed computing support | Network protocols, clustering |
| **[agentzero-python-bridge](agentzero-python-bridge/)** | Python interoperability | Python C API, pybind11 |

### Module Relationships

```
┌─────────────────────────────────────────────────────────────┐
│                     agentzero-core                          │
│              (Orchestration & Cognitive Loop)               │
└─────────────────────────────────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
┌───────▼────────┐  ┌──────▼──────┐  ┌────────▼────────┐
│  perception    │  │   planning   │  │   learning      │
└───────┬────────┘  └──────┬───────┘  └────────┬────────┘
        │                  │                    │
        └──────────────────┼────────────────────┘
                           │
        ┌──────────────────┼──────────────────┐
        │                  │                  │
┌───────▼────────┐  ┌─────▼──────┐  ┌───────▼────────┐
│  memory        │  │ communication│  │    tools       │
└────────────────┘  └──────────────┘  └────────────────┘
        │                  │                  │
        └──────────────────┼──────────────────┘
                           │
        ┌──────────────────┴──────────────────┐
        │                                     │
┌───────▼────────┐              ┌────────────▼────────┐
│  distributed   │              │  python-bridge      │
└────────────────┘              └─────────────────────┘
```

## 🚀 Quick Start

### Prerequisites

**Required:**
- CMake 3.16+
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Boost 1.46+
- OpenCog core components:
  - [cogutil](https://github.com/opencog/cogutil)
  - [atomspace](https://github.com/opencog/atomspace)
  - [cogserver](https://github.com/opencog/cogserver)

**Optional (for specific modules):**
- PLN (Probabilistic Logic Networks)
- URE (Unified Rule Engine)
- MOSES (Meta-Optimizing Semantic Evolutionary Search)
- Pattern Miner
- Link Grammar

### Installation

```bash
# Clone the repository
git clone https://github.com/o9nn/cogzero.git
cd cogzero

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build the system
make -j$(nproc)

# Run tests
make test

# Install (optional)
sudo make install
```

### Build Options

```bash
# Debug build with symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release build with optimizations
cmake -DCMAKE_BUILD_TYPE=Release ..

# Without examples
cmake -DBUILD_EXAMPLES=OFF ..

# Without tests
cmake -DBUILD_TESTING=OFF ..

# With documentation (requires Doxygen)
cmake -DBUILD_DOCS=ON ..

# Custom install prefix
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
```

## 📚 Documentation

**📖 [Complete Documentation Index](docs/INDEX.md)** - Start here for all documentation

### Quick Links

**Getting Started:**
- [Quick Start Guide](docs/QUICK_START.md) - Get up and running in 30 minutes
- [Architecture Overview](docs/ARCHITECTURE.md) - System design and components
- [Build System Guide](BUILD_SYSTEM.md) - Detailed build instructions

**Developer Resources:**
- [API Reference](docs/API_REFERENCE.md) - Complete API documentation
- [Developer Guide](docs/DEVELOPER_GUIDE.md) - Development workflow and standards
- [Integration Guide](docs/INTEGRATION_GUIDE.md) - OpenCog integration patterns
- [Code Standards](docs/CODE_STANDARDS.md) - Coding conventions

**Testing & Quality:**
- [Testing Guide](docs/TESTING_GUIDE.md) - Unit, integration, and performance testing
- [Benchmarking Guide](docs/BENCHMARKING_GUIDE.md) - Performance measurement
- [CI/CD Documentation](AGENT_ZERO_CI_README.md) - Continuous integration setup

**Operations:**
- [Deployment Guide](docs/DEPLOYMENT_GUIDE.md) - Production deployment
- [Troubleshooting Guide](docs/TROUBLESHOOTING.md) - Common issues and solutions

**Project Resources:**
- [AGENT-ZERO-GENESIS.md](AGENT-ZERO-GENESIS.md) - Complete project roadmap
- [Module READMEs](.) - Each module has detailed documentation in its subdirectory

## 🔄 Integration with OpenCog

### AtomSpace Integration
- All agent state represented as Atoms
- Knowledge structures use standard AtomSpace types
- Temporal information via TimeNodes and AtTimeLinks
- Goal hierarchies as structured Atom trees

### CogServer Integration
- Agent runs as CogServer module
- Real-time state monitoring via CogServer commands
- Network interface for distributed operation
- Debug and introspection capabilities

### Reasoning Integration
- PLN rules for inference and reasoning
- URE integration for flexible reasoning patterns
- Custom Agent-Zero reasoning rules
- Uncertainty handling through TruthValues

### Learning Integration
- MOSES integration for policy optimization
- Pattern mining for knowledge discovery
- Online learning through AtomSpace updates
- Experience replay using stored trajectories

## 🧪 Testing

### Standalone Test Suite (no OpenCog required)

The `standalone/` module ships a comprehensive test suite that is built and run automatically:

```bash
cd standalone/build
ctest -V
# Results: 49 passed, 0 failed
```

**Test categories:**

| Suite | File | Tests | Coverage |
|-------|------|-------|----------|
| AtomStore | `test_atom_store.cpp` | 11 | CRUD, TruthValues, STI/LTI, type queries |
| TaskManager | `test_task_manager.cpp` | 7 | Goals, tasks, priorities, execution |
| ReasoningEngine | `test_reasoning_engine.cpp` | 7 | Rules, forward chaining, queries |
| CognitiveLoop | `test_cognitive_loop.cpp` | 6 | Single cycle, percepts, hooks, threading |
| Agent | `test_agent.cpp` | 7 | Full agent API, rules, status |
| **E2E Scenarios** | **`test_e2e.cpp`** | **10** | **Full multi-component integration** |
| **Perception** | **`test_perception.cpp`** | **36** | **MultiModalSensor, PerceptualProcessor, AttentionManager, TextualSensor** |

**End-to-end scenarios covered:**
- Full perception → reasoning → action cycle
- Multiple goals with priority ordering
- Task dependency chains
- Reasoning chains across multiple cycles
- Reflection hook statistics collection
- Agent stability under 50+ cycles
- Percept-driven autonomous goal creation
- Concurrent perception (background thread)
- Status report completeness
- Script-style command sequence

### Full OpenCog Test Suite

```
tests/
├── unit-tests/         # Unit tests for individual classes
├── integration-tests/  # Integration tests with OpenCog
├── performance-tests/  # Performance benchmarks
├── regression-tests/   # Regression test suite
├── framework/          # Testing framework utilities
└── mocks/             # Mock objects for testing
```

```bash
# All tests (requires OpenCog)
make test

# Specific test suite
ctest -R unit_tests

# With verbose output
ctest -V

# Performance benchmarks
ctest -R performance

# Integration tests
ctest -R integration
```

## 📊 Performance Targets

- **Response Time**: < 100ms for routine decisions
- **Memory Efficiency**: Linear scaling with knowledge base size
- **Learning Rate**: Demonstrable improvement within 1000 interactions
- **Integration Overhead**: < 10% vs. standalone Agent-Zero
- **Scalability**: Support for 10M+ Atoms in knowledge base

## 🤝 Contributing

We welcome contributions! Please see our contributing guidelines:

### Getting Started

1. **Read the Roadmap**: Understand the project vision in [AGENT-ZERO-GENESIS.md](AGENT-ZERO-GENESIS.md)
2. **Find a Task**: Browse existing issues or check the roadmap
3. **Setup Environment**: Install OpenCog dependencies and build system
4. **Submit PR**: Follow standard GitHub workflow with tests and documentation

### Code Standards

- **C++17 Standard**: Modern C++ features encouraged
- **OpenCog Patterns**: Follow established OpenCog architectural patterns
- **Documentation**: Document all public interfaces
- **Testing**: Comprehensive unit and integration tests
- **Performance**: Consider memory and CPU optimization

See [CODE_STANDARDS.md](docs/CODE_STANDARDS.md) for detailed guidelines.

### Development Phases

| Phase | Focus | Status |
|-------|-------|--------|
| **Phase 1** | Foundation Layer | ✅ Implemented |
| **Phase 2** | Perception & Action | ✅ Implemented |
| **Phase 3** | Knowledge & Reasoning | ✅ Implemented |
| **Phase 4** | Planning & Goals | ✅ Implemented |
| **Phase 5** | Learning & Adaptation | ✅ Implemented |
| **Phase 6** | Communication & NLP | ✅ Implemented |
| **Phase 7** | Memory & Context | ✅ Implemented |
| **Phase 8** | Tool Integration | ✅ Implemented |
| **Phase 9** | Integration & Testing | ✅ Implemented |
| **Phase 10** | Advanced Features | ✅ Implemented |
| **Phase 11** | Performance Profiling | ✅ Implemented |
| **Phase 12** | Advanced Coordination & Observability | ✅ Implemented |
| **Phase 13** | CI/CD Hardening | ✅ Implemented |
| **Phase 14** | Production Hardening | ✅ Implemented |

## 🔮 Roadmap

See [ROADMAP.md](ROADMAP.md) for the full development roadmap.

### Completed
- [x] Standalone CLI application (`cog0`) with interactive REPL
- [x] rc-style script file execution (`--script`)
- [x] Inline command evaluation (`--eval`, semicolon-separated multi-command)
- [x] `--batch` mode with JSON status output for programmatic use
- [x] `save` / `load` AtomStore snapshot commands
- [x] Interactive `rule` registration and on-demand `infer`
- [x] Colour output with `--no-color` toggle; optional readline tab-completion
- [x] Comprehensive unit + e2e + CLI smoke test suite (210+ tests, 0 failures)
- [x] Full cognitive architecture Phases 1–14 implemented
- [x] `goals` and `infer` REPL commands for introspection and on-demand inference
- [x] Raft-based distributed consensus for `MultiAgentCoordinator`
- [x] Live agent migration across cluster nodes
- [x] Real-time monitoring dashboard (WebSocket + HTML/JS at `/dashboard`)
- [x] Structured JSON-lines logging for log-aggregation pipelines

### Future Work
- [ ] gRPC agent interface (requires external grpc dependency)
- [ ] TLS for MonitoringServer (requires mbedTLS/OpenSSL)
- [ ] RocksDB-backed persistent Raft log

See [AGENT-ZERO-GENESIS.md](AGENT-ZERO-GENESIS.md) for the full architecture specification and [ROADMAP.md](ROADMAP.md) for the detailed development roadmap.

## 📦 Project Structure

```
cogzero/
├── CMakeLists.txt              # Main build configuration
├── README.md                   # This file
├── ROADMAP.md                  # Development roadmap
├── LICENSE                     # AGPL-3.0 license
├── AGENT-ZERO-GENESIS.md      # Architecture specification
├── BUILD_SYSTEM.md            # Build system documentation
├── AGENT_ZERO_CI_README.md    # CI/CD documentation
├── cmake/                      # CMake configuration files
├── docs/                       # Comprehensive documentation
├── standalone/                 # ✅ Standalone CLI app (no OpenCog needed)
│   ├── main.cpp                #    cog0 CLI entry point
│   ├── include/cog0/           #    Public headers
│   ├── src/                    #    Implementation (AtomStore, TaskManager, …)
│   └── tests/                  #    Unit + e2e test suite (49 tests)
├── agentzero-core/            # Core orchestration module
├── agentzero-perception/      # Perception module
├── agentzero-planning/        # Planning module
├── agentzero-learning/        # Learning module
├── agentzero-memory/          # Memory module
├── agentzero-communication/   # Communication module
├── agentzero-tools/           # Tools integration module
├── agentzero-distributed/     # Distributed computing module
├── agentzero-python-bridge/   # Python bridge module
├── tests/                     # Full OpenCog test suites
├── profiling/                 # Profiling tools
└── examples/                  # Example applications
```

## 🆘 Troubleshooting

For comprehensive troubleshooting information, see the [Troubleshooting Guide](docs/TROUBLESHOOTING.md).

### Quick Fixes

**Build fails with "cogutil not found"**
```bash
# Install OpenCog dependencies first
# See: https://github.com/opencog/cogutil
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

**CMake configuration errors**
```bash
# Ensure CMake 3.16+ is installed
cmake --version

# Check C++17 compiler support
g++ --version  # GCC 7+ required
```

**Runtime errors with AtomSpace**
- Check AtomSpace initialization
- Verify proper Handle usage and memory management
- Ensure thread-safe operations where needed

See [Troubleshooting Guide](docs/TROUBLESHOOTING.md) for detailed solutions.

## 📄 License

This project is licensed under the **AGPL-3.0-or-later** license. See [LICENSE](LICENSE) for details.

## 🙏 Acknowledgments

- **OpenCog Foundation** - For the cognitive architecture framework
- **Agent-Zero Project** - For the original agent architecture
- **Contributors** - All developers who have contributed to this project

## 📞 Contact & Support

- **GitHub Issues**: [Report bugs and request features](https://github.com/o9nn/cogzero/issues)
- **Documentation**: Check [docs/](docs/) directory for comprehensive guides
- **OpenCog Community**: Join the [OpenCog discussion forums](https://wiki.opencog.org/)

## 🔗 Related Projects

- [OpenCog](https://github.com/opencog) - Cognitive architecture framework
- [AtomSpace](https://github.com/opencog/atomspace) - Hypergraph knowledge representation
- [CogServer](https://github.com/opencog/cogserver) - Network server for AtomSpace
- [pycog0](https://github.com/o9nn/pycog0) - Parent repository with full OpenCog ecosystem

---

**CogZero** is part of the AGENT-ZERO-GENESIS initiative to create a C++ variant of Agent-Zero optimized for OpenCog integration as an orchestration workbench system.

*Built with ❤️ for the OpenCog community*
