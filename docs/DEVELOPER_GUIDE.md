# Agent-Zero Developer Guide

Comprehensive guide for developers working on Agent-Zero.

## Table of Contents

1. [Development Environment](#development-environment)
2. [Project Structure](#project-structure)
3. [Coding Standards](#coding-standards)
4. [Development Workflow](#development-workflow)
5. [Building and Testing](#building-and-testing)
6. [Contributing](#contributing)
7. [Documentation](#documentation)

## Development Environment

### Prerequisites

- **OS**: Ubuntu 20.04+ or compatible Linux
- **Compiler**: GCC 9+ or Clang 10+ (C++17 support)
- **CMake**: 3.16 or newer
- **Git**: For version control
- **IDE**: VS Code, CLion, or your preferred C++ IDE

### Setting Up

```bash
# Clone repository
git clone https://github.com/OpenCoq/pycog0.git
cd pycog0

# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake git \
    libboost-all-dev cxxtest doxygen

# Build OpenCog dependencies
mkdir build && cd build
cmake ..
make cogutil atomspace cogserver
cd ../agents/cpp

# Configure Agent-Zero
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### IDE Setup

#### VS Code

Install recommended extensions:
- C/C++ (Microsoft)
- CMake Tools
- GitLens
- Doxygen Documentation Generator

Configuration (`.vscode/settings.json`):
```json
{
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "C_Cpp.default.cppStandard": "c++17",
    "C_Cpp.default.includePath": [
        "${workspaceFolder}/agents/cpp/**",
        "/usr/local/include"
    ]
}
```

#### CLion

1. Open `agents/cpp` as CMake project
2. Set CMake options: `-DCMAKE_BUILD_TYPE=Debug`
3. Configure include paths to OpenCog installations

## Project Structure

```
agents/cpp/
├── CMakeLists.txt              # Main build configuration
├── README.md                   # Project overview
├── BUILD_SYSTEM.md            # Build system documentation
│
├── cmake/                      # CMake modules
│   └── AgentZeroCppConfig.cmake.in
│
├── docs/                       # Documentation
│   ├── INDEX.md
│   ├── ARCHITECTURE.md
│   ├── API_REFERENCE.md
│   └── ...
│
├── agentzero-core/            # Core orchestration (Phase 1)
│   ├── include/               # Public headers
│   │   └── agentzero/
│   │       ├── AgentZeroCore.h
│   │       └── CognitiveLoop.h
│   ├── src/                   # Implementation
│   │   ├── AgentZeroCore.cpp
│   │   └── CognitiveLoop.cpp
│   ├── tests/                 # Unit tests
│   └── CMakeLists.txt
│
├── agentzero-perception/      # Perception system (Phase 2)
├── agentzero-knowledge/       # Knowledge & reasoning (Phase 3)
├── agentzero-planning/        # Planning system (Phase 4)
├── agentzero-learning/        # Learning system (Phase 5)
├── agentzero-communication/   # Communication (Phase 6)
├── agentzero-memory/          # Memory management (Phase 7)
├── agentzero-tools/           # Tool integration (Phase 8)
│
├── examples/                  # Example applications
│   ├── CognitiveLoopIntegrationDemo.cpp
│   └── ...
│
└── tests/                     # Integration tests
    └── CMakeLists.txt
```

### Module Organization

Each module follows this structure:
```
agentzero-{module}/
├── CMakeLists.txt           # Module build config
├── README.md                # Module documentation
├── include/agentzero/       # Public interface
│   └── {Component}.h
├── src/                     # Implementation
│   └── {Component}.cpp
├── tests/                   # Unit tests
│   └── {Component}Test.cpp
└── examples/                # Usage examples
    └── {Component}Demo.cpp
```

## Coding Standards

### C++ Style Guide

Follow modern C++17 best practices:

#### Naming Conventions

```cpp
// Classes: PascalCase
class AgentZeroCore {};
class PerceptualProcessor {};

// Functions: camelCase
void cognitiveStep();
Handle addGoal(const std::string& description);

// Variables: camelCase
int goalCount;
float priorityThreshold;

// Constants: UPPER_SNAKE_CASE
constexpr int MAX_ITERATIONS = 1000;
const std::string DEFAULT_CONFIG = "config.yaml";

// Private members: camelCase with trailing underscore
class Example {
private:
    int count_;
    std::string name_;
};

// Namespaces: lowercase
namespace agentzero {
namespace detail {
}
}
```

#### Header Guards

Use `#pragma once`:
```cpp
#pragma once

#include <opencog/atomspace/AtomSpace.h>

namespace agentzero {
// ... declarations
}
```

#### Includes

Order includes logically:
```cpp
// 1. Related header (for .cpp files)
#include "AgentZeroCore.h"

// 2. C system headers
#include <cassert>
#include <cstring>

// 3. C++ standard library
#include <memory>
#include <string>
#include <vector>

// 4. Other libraries (Boost, OpenCog)
#include <boost/filesystem.hpp>
#include <opencog/atomspace/AtomSpace.h>

// 5. Project headers
#include <agentzero/CognitiveLoop.h>
```

#### Memory Management

Prefer RAII and smart pointers:
```cpp
// ✅ Good: Use smart pointers
std::unique_ptr<Component> component = std::make_unique<Component>();
std::shared_ptr<Resource> resource = std::make_shared<Resource>();

// ✅ Good: RAII for resources
class ResourceHolder {
    Resource resource_;
public:
    ResourceHolder() : resource_(acquireResource()) {}
    ~ResourceHolder() { releaseResource(resource_); }
};

// ❌ Avoid: Raw new/delete
Component* comp = new Component();  // Manual memory management
delete comp;
```

#### Error Handling

Use exceptions for error conditions:
```cpp
// ✅ Good: Throw specific exceptions
if (!isValid(input)) {
    throw InvalidArgumentException("Input validation failed");
}

// ✅ Good: Document exceptions
/**
 * Process input data.
 * @throws InvalidArgumentException if input is invalid
 * @throws ResourceException if resources unavailable
 */
void process(const Data& input);

// Catch specific exceptions
try {
    component->process();
} catch (const InvalidArgumentException& e) {
    // Handle specific error
    logger().error("Invalid argument: {}", e.what());
} catch (const std::exception& e) {
    // Handle other errors
    logger().error("Unexpected error: {}", e.what());
}
```

### Documentation Standards

#### Public API Documentation

Use Doxygen-style comments:
```cpp
/**
 * @brief Core orchestration class for Agent-Zero cognitive architecture.
 * 
 * AgentZeroCore manages the cognitive loop, goal hierarchies, and
 * integration with OpenCog's AtomSpace for knowledge representation.
 * 
 * @note This class is thread-safe when accessing AtomSpace.
 * 
 * Example usage:
 * @code
 * AtomSpace atomspace;
 * AgentZeroCore agent(atomspace);
 * agent.initialize();
 * agent.addGoal("Learn skill", 0.8f);
 * agent.runCognitiveLoop(100);
 * @endcode
 */
class AgentZeroCore {
public:
    /**
     * @brief Add a goal to the agent's goal hierarchy.
     * 
     * @param description Human-readable goal description
     * @param priority Priority value (0.0 to 1.0), default 0.5
     * @return Handle to the created goal atom
     * @throws InvalidArgumentException if priority is out of range
     */
    opencog::Handle addGoal(const std::string& description, 
                           float priority = 0.5f);
};
```

#### Implementation Comments

```cpp
void AgentZeroCore::cognitiveStep() {
    // Phase 1: Perception - Process sensory input and create atoms
    auto percepts = perceptionSystem_->process(getSensorData());
    
    // Phase 2: Attention - Allocate resources using ECAN
    // This spreads importance values throughout the AtomSpace
    attentionSystem_->allocate();
    
    // TODO: Optimize attention allocation for large knowledge bases
    // See issue #123 for performance analysis
    
    // Phase 3: Reasoning - PLN-based inference on focused atoms
    reasoningEngine_->infer(getFocusedAtoms());
}
```

## Development Workflow

### Branch Strategy

- `main`: Stable production code
- `develop`: Integration branch for features
- `feature/AZ-{TASK}-{description}`: Feature branches
- `bugfix/{issue}-{description}`: Bug fix branches

### Feature Development

```bash
# Create feature branch
git checkout develop
git pull origin develop
git checkout -b feature/AZ-CORE-005-add-meta-planning

# Make changes
# ... edit code ...

# Build and test
cd build
cmake ..
make -j$(nproc)
make test

# Commit with descriptive messages
git add .
git commit -m "AZ-CORE-005: Implement meta-planning functionality

- Add MetaPlanner class with self-optimization
- Integrate with cognitive loop
- Add unit tests for meta-planning
- Update documentation

Closes #42"

# Push and create PR
git push origin feature/AZ-CORE-005-add-meta-planning
```

### Commit Messages

Follow conventional commit format:
```
<type>(<scope>): <subject>

<body>

<footer>
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation only
- `style`: Code style changes
- `refactor`: Code refactoring
- `perf`: Performance improvement
- `test`: Adding tests
- `chore`: Build/tooling changes

Example:
```
feat(planning): Add temporal constraint handling

Implement support for temporal constraints in planning engine:
- Add TemporalConstraint class
- Integrate with action scheduler
- Update plan generation logic

Fixes #123
```

## Building and Testing

### Debug Build

```bash
cd agents/cpp
mkdir -p build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Release Build

```bash
mkdir -p build-release && cd build-release
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-O3 -march=native" \
      ..
make -j$(nproc)
```

### Running Tests

```bash
# All tests
make test

# Verbose output
ctest -V

# Specific test
ctest -R AgentZeroCoreTest

# With memory checking (if valgrind installed)
ctest -T memcheck
```

### Code Coverage

```bash
# Build with coverage
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="--coverage" \
      ..
make -j$(nproc)
make test

# Generate coverage report
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
lcov --list coverage.info
```

### Static Analysis

```bash
# clang-tidy
clang-tidy src/*.cpp -- -I include -I /usr/local/include

# cppcheck
cppcheck --enable=all --inconclusive src/
```

## Contributing

### Pull Request Process

1. **Create Issue First**: Describe the feature or bug
2. **Fork and Branch**: Create feature branch from `develop`
3. **Implement**: Follow coding standards
4. **Test**: Add/update tests, ensure all pass
5. **Document**: Update relevant documentation
6. **Submit PR**: Reference issue number, describe changes
7. **Code Review**: Address reviewer feedback
8. **Merge**: Maintainer merges when approved

### Code Review Checklist

- [ ] Code follows style guide
- [ ] All tests pass
- [ ] New tests added for new functionality
- [ ] Documentation updated
- [ ] No compiler warnings
- [ ] Thread-safe if applicable
- [ ] Error handling appropriate
- [ ] Performance acceptable

## Documentation

### Updating Documentation

When adding features:
1. Update module README.md
2. Add/update API documentation
3. Add usage examples
4. Update relevant guides

### Building Doxygen Documentation

```bash
# Install Doxygen
sudo apt-get install -y doxygen graphviz

# Build docs
cd agents/cpp/build
cmake -DBUILD_DOCS=ON ..
make docs

# View documentation
firefox docs/html/index.html
```

### Documentation Style

- **Clear and Concise**: Get to the point
- **Examples**: Show, don't just tell
- **Complete**: Cover all parameters and return values
- **Updated**: Keep in sync with code changes

## Debugging Tips

### Using GDB

```bash
# Build with debug symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Run with GDB
gdb ./examples/CognitiveLoopIntegrationDemo

# Common commands
(gdb) break AgentZeroCore::cognitiveStep
(gdb) run
(gdb) step
(gdb) print atomspace
(gdb) backtrace
```

### Logging

Use the OpenCog logger:
```cpp
#include <opencog/util/Logger.h>

logger().info("Agent initialized");
logger().debug("Processing {} atoms", atoms.size());
logger().warn("Goal priority out of bounds: {}", priority);
logger().error("Failed to load configuration: {}", error);
```

### Performance Profiling

```bash
# Using perf
perf record ./my_agent
perf report

# Using valgrind callgrind
valgrind --tool=callgrind ./my_agent
kcachegrind callgrind.out.*
```

## Quick Reference

### Common Tasks

**Add new component:**
1. Create directory structure
2. Add CMakeLists.txt
3. Create header/source files
4. Add tests
5. Update parent CMakeLists.txt

**Add new test:**
```cpp
#include <cxxtest/TestSuite.h>
#include <agentzero/MyComponent.h>

class MyComponentTest : public CxxTest::TestSuite {
public:
    void testBasicFunctionality() {
        MyComponent comp;
        TS_ASSERT(comp.isValid());
        TS_ASSERT_EQUALS(comp.getValue(), 42);
    }
};
```

**Add example:**
```cpp
#include <agentzero/AgentZeroCore.h>

int main() {
    // Example demonstrating feature X
    // ...
    return 0;
}
```

## Getting Help

- **Documentation**: Check docs/ directory
- **Examples**: See examples/ directory  
- **Issues**: Search/create GitHub issues
- **OpenCog Community**: https://wiki.opencog.org/

---

*Part of the AGENT-ZERO-GENESIS documentation - Phase 9: Integration & Testing (AZ-DOC-001)*
