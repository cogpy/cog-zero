# Agent-Zero CMake Build System

This directory contains the Agent-Zero C++ implementation with a comprehensive CMake build system that follows OpenCog architectural patterns.

## Build System Features

### Core Features
- **CMake 3.16+ compatibility** - Modern CMake standards
- **OpenCog integration** - Seamless integration with OpenCog ecosystem  
- **Cross-platform support** - Proper Boost dependency handling
- **Package configuration** - Exportable CMake targets for downstream projects
- **Test framework** - CxxTest-based unit testing
- **Modular architecture** - Phase-based component organization

### OpenCog Dependencies
Agent-Zero requires the following OpenCog components to be installed:
- **cogutil** - Core utilities and base functionality
- **atomspace** - Knowledge representation system
- **cogserver** - Cognitive processing server

## Quick Start

### Prerequisites
```bash
# Install system dependencies
sudo apt-get install -y build-essential cmake libboost-all-dev

# Install OpenCog ecosystem (required)
# See: docs/OPENCOG_DEPENDENCY_BUILD.md
```

### Building Agent-Zero
```bash
# From the main repository
mkdir build && cd build
cmake ..
make agents
```

### Building Individual Components
```bash
# Configure and build specific modules
make configure-agentzero-core
make agentzero-core
```

## Build Configuration

### CMake Options
- `BUILD_TESTING=ON` - Enable unit tests (default: ON)
- `BUILD_EXAMPLES=ON` - Build example applications (default: ON)  
- `BUILD_DOCS=OFF` - Build documentation (default: OFF)
- `CMAKE_BUILD_TYPE=Release` - Build configuration (default: Release)

### Cross-Platform Boost Support
The build system automatically detects and configures Boost with:
- Dynamic linking (Boost_USE_STATIC_LIBS=OFF)
- Multithreading support (Boost_USE_MULTITHREADED=ON)
- Required components: system, filesystem, thread
- Minimum version: 1.46

## Architecture Overview

### Module Organization
```
agents/cpp/
├── CMakeLists.txt              # Main build configuration
├── cmake/                      # Package configuration templates
│   └── AgentZeroCppConfig.cmake.in
├── agentzero-core/             # Phase 1: Foundation Layer
│   ├── include/                # Public headers
│   ├── src/                    # Implementation
│   ├── tests/                  # Unit tests
│   └── CMakeLists.txt          # Module configuration
└── tests/                      # Integration tests
    └── CMakeLists.txt          # Test configuration
```

### Phase-Based Dependencies
The build system supports 8 development phases as defined in AGENT-ZERO-GENESIS.md:

1. **Foundation Layer** (agentzero-core) - Core orchestration engine
2. **Perception & Action** (agentzero-perception) - Sensory integration
3. **Knowledge & Reasoning** (agentzero-knowledge) - PLN/URE integration
4. **Planning & Goals** (agentzero-planning) - Spacetime integration
5. **Learning & Adaptation** (agentzero-learning) - MOSES integration
6. **Communication & NLP** (agentzero-communication) - Language integration
7. **Memory & Context** (agentzero-memory) - Attention integration
8. **Tool Integration** (agentzero-tools) - External tool integration

## Testing

### Unit Tests
```bash
# Enable testing and build tests
cmake -DBUILD_TESTING=ON ..
make agentzero-tests

# Run specific tests
ctest -R AgentZeroCore
```

### Test Requirements
- **CxxTest** - Unit testing framework
- **OpenCog components** - For integration testing

## Installation

### Installing Agent-Zero
```bash
# Build and install
make agents
cd agents-build
sudo make install
sudo ldconfig
```

### Package Configuration
After installation, downstream projects can use:
```cmake
find_package(AgentZeroCpp REQUIRED)
target_link_libraries(your_target AgentZero::agentzero-core)
```

## Integration with OpenCog Build System

### Main Repository Integration
Agent-Zero is fully integrated with the main OpenCog repository build system:
- Registered as "agents" component in Integration layer
- Discoverable via `make list-components`
- Buildable via `make agents` or `make Integration-layer`

### Dependency Management
The build system follows OpenCog patterns:
- Uses pkg-config for OpenCog component discovery
- Provides proper error messages for missing dependencies
- Supports both development and installed OpenCog components

## Troubleshooting

### Common Issues

**OpenCog dependencies not found:**
```
Please install the OpenCog ecosystem first:
  See: https://github.com/opencog/cogutil
  Or use the OpenCog Dependency Build workflow
```
Solution: Install cogutil, atomspace, and cogserver first.

**Boost not found:**
```
Could NOT find Boost (missing: system filesystem thread)
```
Solution: `sudo apt-get install -y libboost-all-dev`

**CxxTest not found:**
```
CxxTest missing: needed for unit tests
```
Solution: `sudo apt-get install -y cxxtest`

## Development Workflow

### Adding New Components
1. Create component directory under `agents/cpp/`
2. Add CMakeLists.txt with proper exports
3. Update main CMakeLists.txt to add_subdirectory()
4. Add component to appropriate phase in AGENT-ZERO-GENESIS.md

### Package Configuration
All components export CMake targets using:
- Namespace: `AgentZero::`
- Export name: `AgentZeroCppTargets` 
- Config template: `cmake/AgentZeroCppConfig.cmake.in`

This ensures consistent downstream integration patterns.