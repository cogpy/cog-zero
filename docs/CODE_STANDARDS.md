# Agent-Zero Code Standards

Coding standards and best practices for Agent-Zero development.

## General Principles

1. **Clarity**: Code should be self-explanatory
2. **Consistency**: Follow established patterns
3. **Correctness**: Prioritize correctness over cleverness
4. **Performance**: Optimize where it matters
5. **Maintainability**: Write code others can understand

## C++ Standards

### Language Version

- **C++17** is the minimum required standard
- Prefer modern C++ features over legacy patterns
- Use standard library over custom implementations

### Naming Conventions

```cpp
// Classes and Structs: PascalCase
class AgentZeroCore { };
struct CognitiveState { };

// Functions and Methods: camelCase
void cognitiveStep();
Handle addGoal(const std::string& description);

// Variables: camelCase
int goalCount = 0;
float priorityThreshold = 0.5f;

// Constants: UPPER_SNAKE_CASE
constexpr int MAX_ITERATIONS = 1000;
const char* DEFAULT_CONFIG_PATH = "/etc/agentzero/config.yaml";

// Enum classes: PascalCase for type, UPPER_CASE for values
enum class GoalStatus {
    PENDING,
    ACTIVE,
    ACHIEVED,
    FAILED
};

// Private members: camelCase with trailing underscore
class Example {
private:
    int count_;
    std::string name_;
    std::unique_ptr<Resource> resource_;
};

// Namespaces: lowercase
namespace agentzero {
namespace detail {
namespace internal {
}}}
```

### File Organization

```cpp
// header_file.h
#pragma once  // Preferred over include guards

// 1. System includes
#include <memory>
#include <string>

// 2. Third-party includes
#include <boost/filesystem.hpp>
#include <opencog/atomspace/AtomSpace.h>

// 3. Project includes
#include <agentzero/Component.h>

namespace agentzero {

// Forward declarations
class Implementation;

/**
 * @brief Brief description.
 * 
 * Detailed description.
 */
class MyClass {
public:
    // Public types
    using Callback = std::function<void(Result)>;
    
    // Constructors
    explicit MyClass(int value);
    
    // Destructor
    ~MyClass();
    
    // Public methods
    void publicMethod();
    
private:
    // Private methods
    void privateMethod();
    
    // Private members
    int value_;
    std::unique_ptr<Implementation> impl_;
};

} // namespace agentzero
```

### Modern C++ Features

#### Smart Pointers

```cpp
// ✅ Preferred: Use smart pointers
std::unique_ptr<Resource> resource = std::make_unique<Resource>();
std::shared_ptr<Cache> cache = std::make_shared<Cache>();

// ❌ Avoid: Raw pointers for ownership
Resource* resource = new Resource();  // Manual management
```

#### RAII

```cpp
// ✅ Good: RAII for resource management
class FileHandle {
    FILE* file_;
public:
    FileHandle(const char* path) : file_(fopen(path, "r")) {}
    ~FileHandle() { if (file_) fclose(file_); }
    
    // Delete copy, allow move
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    FileHandle(FileHandle&&) = default;
    FileHandle& operator=(FileHandle&&) = default;
};
```

#### Auto and Type Inference

```cpp
// ✅ Use auto for obvious types
auto handle = atomspace.add_node(CONCEPT_NODE, "test");
auto it = container.begin();

// ❌ Don't use auto when type is not obvious
auto result = complexFunction();  // What type is this?

// ✅ Better: Explicit type for clarity
Handle result = complexFunction();
```

#### Range-based Loops

```cpp
// ✅ Preferred: Range-based for
for (const Handle& h : handles) {
    process(h);
}

// ❌ Avoid: Traditional loops unless needed
for (size_t i = 0; i < handles.size(); i++) {
    process(handles[i]);
}
```

#### Lambdas

```cpp
// ✅ Use lambdas for callbacks
processAsync([this](Result r) {
    handleResult(r);
});

// Capture guidelines:
// [=] - Capture by value (creates copies)
// [&] - Capture by reference (be careful with lifetimes!)
// [this] - Capture this pointer
// [&x, y] - Specific captures
```

### Error Handling

```cpp
// Use exceptions for exceptional conditions
class AgentZeroException : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class InvalidArgumentException : public AgentZeroException {};
class ResourceException : public AgentZeroException {};

// Function that can throw
void processInput(const Data& input) {
    if (!isValid(input)) {
        throw InvalidArgumentException("Invalid input data");
    }
    // ... processing
}

// Document exceptions
/**
 * @throws InvalidArgumentException if input invalid
 * @throws ResourceException if resources unavailable  
 */
void criticalOperation(const Input& in);

// Catch specific exceptions
try {
    criticalOperation(input);
} catch (const InvalidArgumentException& e) {
    logger().error("Invalid input: {}", e.what());
    return false;
} catch (const ResourceException& e) {
    logger().error("Resource error: {}", e.what());
    retry();
} catch (const std::exception& e) {
    logger().error("Unexpected error: {}", e.what());
    throw;
}
```

### Documentation

#### Header Documentation

```cpp
/**
 * @file AgentZeroCore.h
 * @brief Core orchestration for Agent-Zero cognitive architecture.
 * @author Agent-Zero Team
 * @date 2024
 */

/**
 * @class AgentZeroCore
 * @brief Main class for Agent-Zero cognitive architecture.
 * 
 * AgentZeroCore manages the cognitive loop, integrates with OpenCog's
 * AtomSpace, and coordinates all cognitive components.
 * 
 * Thread Safety: This class is thread-safe when accessing AtomSpace.
 * 
 * Example:
 * @code
 * AtomSpace atomspace;
 * AgentZeroCore agent(atomspace);
 * agent.initialize();
 * agent.addGoal("Learn skill", 0.8f);
 * agent.runCognitiveLoop(100);
 * @endcode
 * 
 * @see CognitiveLoop, TaskManager
 */
class AgentZeroCore {
public:
    /**
     * @brief Constructs Agent-Zero with an AtomSpace.
     * @param atomspace Reference to AtomSpace for knowledge representation
     * @note The AtomSpace must outlive the AgentZeroCore instance
     */
    explicit AgentZeroCore(opencog::AtomSpace& atomspace);
    
    /**
     * @brief Adds a goal to the agent's hierarchy.
     * 
     * Creates a goal atom in the AtomSpace with the specified priority.
     * The goal will be considered for execution in subsequent cognitive steps.
     * 
     * @param description Human-readable goal description
     * @param priority Goal priority (0.0-1.0), default 0.5
     * @return Handle to the created goal atom
     * @throws InvalidArgumentException if priority out of range
     * @throws ResourceException if AtomSpace full
     */
    opencog::Handle addGoal(const std::string& description,
                           float priority = 0.5f);
};
```

#### Implementation Comments

```cpp
void AgentZeroCore::cognitiveStep() {
    // Phase 1: Perception - Process sensory input
    // Create percept atoms in AtomSpace
    auto percepts = perceptionSystem_->process(getSensorData());
    
    // Phase 2: Attention - ECAN spreads importance
    // TODO(dev): Optimize for large AtomSpaces (>1M atoms)
    // See issue #123
    attentionSystem_->allocate();
    
    // Phase 3: Reasoning - PLN inference
    // Apply rules from configured rule base
    auto inferences = reasoningEngine_->infer(getFocusedAtoms());
    
    // FIXME(dev): Handle case where no inferences produced
    // Currently assumes at least one inference
    if (inferences.empty()) {
        logger().warn("No inferences in cognitive step {}", stepCount_);
    }
}
```

### Code Formatting

Use clang-format with this configuration (`.clang-format`):

```yaml
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 100
AllowShortFunctionsOnASingleLine: Empty
AllowShortIfStatementsOnASingleLine: Never
AlignAfterOpenBracket: Align
BinPackArguments: false
BinPackParameters: false
```

Format code:
```bash
clang-format -i src/*.cpp include/**/*.h
```

## OpenCog Integration Standards

### AtomSpace Usage

```cpp
// ✅ Good: Use AtomSpace for all state
class Component {
    AtomSpace& atomspace_;
    Handle state_;
    
public:
    Component(AtomSpace& as) : atomspace_(as) {
        state_ = atomspace_.add_node(CONCEPT_NODE, "ComponentState");
    }
};

// ❌ Bad: Parallel C++ state
class Component {
    std::map<std::string, Value> state_;  // Bypasses AtomSpace
};
```

### Handle Lifetime

```cpp
// ✅ Good: AtomSpace owns atoms
Handle getHandle(AtomSpace& as) {
    Handle h = as.add_node(CONCEPT_NODE, "test");
    return h;  // Safe - AtomSpace keeps atom alive
}

// ❌ Bad: Dangling reference
Handle getDanglingHandle() {
    AtomSpace as;  // Local AtomSpace
    Handle h = as.add_node(CONCEPT_NODE, "test");
    return h;  // 'as' destroyed, Handle invalid!
}
```

### Truth Values

```cpp
// Always use TruthValue pointers
#include <opencog/truthvalue/SimpleTruthValue.h>

void setTruthValue(Handle h, double strength, double confidence) {
    TruthValuePtr tv = SimpleTruthValue::createTV(strength, confidence);
    h->setTruthValue(tv);
}

// Read truth values
TruthValuePtr tv = handle->getTruthValue();
double strength = tv->get_mean();
double confidence = tv->get_confidence();
```

## Performance Guidelines

### Memory Management

```cpp
// Profile memory usage
void profileMemory() {
    size_t before = getCurrentMemoryUsage();
    
    // Operation
    performOperation();
    
    size_t after = getCurrentMemoryUsage();
    logger().info("Memory increase: {} MB", (after - before) / 1024 / 1024);
}

// Implement cleanup
void cleanup() {
    // Remove low-importance atoms
    auto atoms = atomspace_.get_atoms_by_type(ATOM);
    for (Handle h : atoms) {
        if (h->getSTI() < threshold_) {
            atomspace_.remove_atom(h, true);
        }
    }
}
```

### Optimization

```cpp
// ✅ Good: Reserve capacity
std::vector<Handle> results;
results.reserve(estimatedSize);

// ✅ Good: Minimize copies
void process(const LargeData& data);  // Pass by const ref

// ✅ Good: Use move semantics
Resource createResource() {
    Resource r;
    // ... initialize r
    return r;  // Automatic move
}

// ✅ Good: Avoid unnecessary allocations in loops
std::string buffer;
for (const auto& item : items) {
    buffer.clear();  // Reuse buffer
    buffer = processItem(item);
}
```

## Testing Standards

### Test Structure

```cpp
#include <cxxtest/TestSuite.h>

class ComponentTest : public CxxTest::TestSuite {
private:
    std::unique_ptr<AtomSpace> atomspace_;
    std::unique_ptr<Component> component_;
    
public:
    void setUp() override {
        atomspace_ = std::make_unique<AtomSpace>();
        component_ = std::make_unique<Component>(*atomspace_);
    }
    
    void tearDown() override {
        component_.reset();
        atomspace_.reset();
    }
    
    void testBasicFunctionality() {
        // Arrange
        Handle input = atomspace_->add_node(CONCEPT_NODE, "test");
        
        // Act
        Handle result = component_->process(input);
        
        // Assert
        TS_ASSERT(result != Handle::UNDEFINED);
        TS_ASSERT_EQUALS(result->get_name(), "processed_test");
    }
};
```

### Test Naming

```cpp
// Pattern: test{What}_{When}_{Expected}
void testAddGoal_WithValidPriority_CreatesGoalAtom()
void testCognitiveStep_WithNoPercepts_CompletesSuccessfully()
void testRemoveGoal_WithInvalidHandle_ThrowsException()
```

## Code Review Checklist

- [ ] Follows naming conventions
- [ ] Properly documented (Doxygen)
- [ ] No compiler warnings
- [ ] Memory safe (no leaks, proper RAII)
- [ ] Thread safe where applicable
- [ ] Exception safe
- [ ] Tests included
- [ ] Performance acceptable
- [ ] Follows OpenCog patterns

## Additional Resources

- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- [OpenCog Coding Standards](https://wiki.opencog.org/w/Coding_standards)

---

*Part of the AGENT-ZERO-GENESIS documentation - Phase 9: Integration & Testing (AZ-DOC-001)*
