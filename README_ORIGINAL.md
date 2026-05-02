# Agent-Zero C++ Implementation for OpenCog Integration

This directory contains the C++ implementation of Agent-Zero, specially optimized for integration with OpenCog as an Orchestration Workbench System of Cognitive Architecture Tools.

## 🎯 Project Overview

Agent-Zero-Genesis implements a high-performance C++ variant of Agent-Zero that provides optimal grip and maximum effectiveness within OpenCog's cognitive architecture. The system creates a well-orchestrated and modular catalog of powerful tools, skills, abilities and knowledge enhancements as a coherent and integrated whole.

## 🏗️ Architecture

The system is organized into 8 specialized modules, each optimized for specific cognitive functions:

### Core Components

| Module | Purpose | Dependencies |
|--------|---------|--------------|
| **agentzero-core** | Main orchestration engine | cogutil, atomspace, cogserver |
| **agentzero-perception** | Multi-modal sensory processing | sensory, vision, perception |
| **agentzero-knowledge** | Knowledge representation & reasoning | atomspace, pln, ure, miner |
| **agentzero-planning** | Hierarchical planning & goals | spacetime, cogserver |
| **agentzero-learning** | Continuous learning & adaptation | moses, asmoses, learn |
| **agentzero-communication** | NLP & multi-agent communication | lg-atomese, opencog |
| **agentzero-memory** | Memory & context management | atomspace-rocks, attention |
| **agentzero-tools** | External tool integration | external-tools, ros-behavior-scripting |

## 🚀 Quick Start

### Prerequisites

Before building Agent-Zero C++, you need the OpenCog ecosystem installed:

```bash
# Option 1: Use the OpenCog Dependency Build workflow (Recommended)
# See: docs/OPENCOG_DEPENDENCY_BUILD.md

# Option 2: Manual installation (see individual component READMEs)
# Install in order: cogutil → atomspace → cogserver → [other components]
```

### Building Agent-Zero

```bash
# Clone and navigate to the C++ implementation
cd agents/cpp

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build the system
make -j$(nproc)

# Run tests (when implemented)
make test

# Install (optional)
make install
```

### Build Options

```bash
# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Without examples
cmake -DBUILD_EXAMPLES=OFF ..

# Without tests  
cmake -DBUILD_TESTING=OFF ..

# With documentation (requires Doxygen)
cmake -DBUILD_DOCS=ON ..
```

## 📚 Documentation

**📖 [Complete Documentation Index](docs/INDEX.md)** - Start here for all documentation

### Quick Links

**Getting Started:**
- [Quick Start Guide](docs/QUICK_START.md) - Get up and running in 30 minutes
- [Architecture Overview](docs/ARCHITECTURE.md) - System design and components
- [Installation Guide](docs/QUICK_START.md#installation) - Detailed setup

**Developer Resources:**
- [API Reference](docs/API_REFERENCE.md) - Complete API documentation
- [Developer Guide](docs/DEVELOPER_GUIDE.md) - Development workflow and standards
- [Integration Guide](docs/INTEGRATION_GUIDE.md) - OpenCog integration patterns
- [Code Standards](docs/CODE_STANDARDS.md) - Coding conventions

**Testing & Quality:**
- [Testing Guide](docs/TESTING_GUIDE.md) - Unit, integration, and performance testing
- [Benchmarking Guide](docs/BENCHMARKING_GUIDE.md) - Performance measurement

**Operations:**
- [Deployment Guide](docs/DEPLOYMENT_GUIDE.md) - Production deployment
- [Troubleshooting Guide](docs/TROUBLESHOOTING.md) - Common issues and solutions

**Project Resources:**
- [AGENT-ZERO-GENESIS.md](../../AGENT-ZERO-GENESIS.md) - Complete project roadmap
- [Module READMEs](.) - Each module has detailed documentation in its subdirectory
- [OpenCog Documentation](https://wiki.opencog.org/) - Background on OpenCog architecture
- [Build Documentation](../../docs/) - OpenCog ecosystem build guides

## 🔧 Development Workflow

### Automated Issue Generation

This project uses an automated workflow to generate development tasks:

1. **View Roadmap**: See [AGENT-ZERO-GENESIS.md](../../AGENT-ZERO-GENESIS.md) for all planned tasks
2. **Generate Issues**: Use the "Generate Next Steps" workflow in GitHub Actions
3. **Select Phase**: Choose from Phase 1-10 or "all" phases
4. **Preview or Create**: Review tasks before creating GitHub issues

### Development Phases

| Phase | Focus | Status |
|-------|-------|--------|
| **Phase 1** | Foundation Layer | 🚧 In Progress |
| **Phase 2** | Perception & Action | 📅 Planned |
| **Phase 3** | Knowledge & Reasoning | 📅 Planned |
| **Phase 4** | Planning & Goals | 📅 Planned |
| **Phase 5** | Learning & Adaptation | 📅 Planned |
| **Phase 6** | Communication & NLP | 📅 Planned |
| **Phase 7** | Memory & Context | 📅 Planned |
| **Phase 8** | Tool Integration | 📅 Planned |
| **Phase 9** | Integration & Testing | 📅 Planned |
| **Phase 10** | Advanced Features | 📅 Planned |

### Task Naming Convention

All tasks follow the pattern: `AZ-{CATEGORY}-{NUMBER}`

- **AZ**: Agent-Zero prefix
- **CATEGORY**: Component category (CORE, PERC, KNOW, etc.)
- **NUMBER**: Sequential task number (001, 002, etc.)

Examples:
- `AZ-CORE-001`: Core orchestration task
- `AZ-PERC-002`: Perception processing task  
- `AZ-BUILD-001`: Build system task

## 🧪 Testing

### Test Structure

```
tests/
├── unit/           # Unit tests for individual classes
├── integration/    # Integration tests with OpenCog
├── performance/    # Performance benchmarks
└── examples/       # Example applications
```

### Running Tests

```bash
# All tests
make test

# Specific test suite
ctest -R unit_tests

# With verbose output
ctest -V

# Performance benchmarks
ctest -R performance
```

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

## 📊 Performance Targets

- **Response Time**: < 100ms for routine decisions
- **Memory Efficiency**: Linear scaling with knowledge base size  
- **Learning Rate**: Demonstrable improvement within 1000 interactions
- **Integration Overhead**: < 10% vs. standalone Agent-Zero
- **Scalability**: Support for 10M+ Atoms in knowledge base

## 🤝 Contributing

### Getting Started

1. **Read the Roadmap**: Understand the project vision in [AGENT-ZERO-GENESIS.md](../../AGENT-ZERO-GENESIS.md)
2. **Find a Task**: Use the automated issue generation or browse existing issues
3. **Setup Environment**: Install OpenCog dependencies and build system
4. **Submit PR**: Follow standard GitHub workflow with tests and documentation

### Code Standards

- **C++17 Standard**: Modern C++ features encouraged
- **OpenCog Patterns**: Follow established OpenCog architectural patterns
- **Documentation**: Document all public interfaces
- **Testing**: Comprehensive unit and integration tests
- **Performance**: Consider memory and CPU optimization

### Issue Labels

- `phase-N`: Development phase (1-10)
- `priority-{high|medium|low}`: Task priority
- `{category}`: Component category (core-architecture, perception, etc.)
- `enhancement`: New feature implementation
- `testing`: Test-related tasks
- `documentation`: Documentation updates

## 🔮 Future Roadmap

### Short Term (3 months)
- [x] Foundation architecture and build system
- [ ] Core orchestration engine (Phase 1)
- [ ] Basic perception-action loop (Phase 2)
- [ ] Initial knowledge representation (Phase 3)

### Medium Term (6 months)  
- [ ] Advanced reasoning and planning (Phase 4-5)
- [ ] Natural language interaction (Phase 6)
- [ ] Memory and context management (Phase 7)

### Long Term (12 months)
- [ ] Complete cognitive architecture (Phase 8-9)
- [ ] Multi-agent coordination (Phase 10)
- [ ] Python interoperability bridge
- [ ] Distributed computing integration

## 🆘 Troubleshooting

For comprehensive troubleshooting information, see the [Troubleshooting Guide](docs/TROUBLESHOOTING.md).

### Quick Fixes

**Build fails with "cogutil not found"**
- Install OpenCog dependencies first
- Use the OpenCog Dependency Build workflow
- Check pkg-config path: `export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH`
- See: [Troubleshooting Guide - Build Issues](docs/TROUBLESHOOTING.md#build-issues)

**CMake configuration errors**
- Ensure CMake 3.16+ is installed
- Check C++17 compiler support
- Verify all OpenCog components are installed in the same prefix
- See: [Troubleshooting Guide - CMake Configuration](docs/TROUBLESHOOTING.md#cmake-configuration-fails)

**Runtime errors with AtomSpace**
- Check AtomSpace initialization
- Verify proper Handle usage and memory management
- Ensure thread-safe operations where needed
- See: [Troubleshooting Guide - Runtime Errors](docs/TROUBLESHOOTING.md#runtime-errors)

### Getting Help

- **Documentation**: Check [docs/](docs/) directory for comprehensive guides
- **GitHub Issues**: Report bugs and request features at https://github.com/OpenCoq/pycog0/issues
- **OpenCog Community**: Join the OpenCog discussion forums
- **Troubleshooting Guide**: See [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) for detailed solutions

---

*This project is part of the AGENT-ZERO-GENESIS initiative to create a C++ variant of Agent-Zero optimized for OpenCog integration as an orchestration workbench system.*