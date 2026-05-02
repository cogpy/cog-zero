# Agent-Zero Troubleshooting Guide

Solutions to common issues when working with Agent-Zero.

## Table of Contents

1. [Build Issues](#build-issues)
2. [Runtime Errors](#runtime-errors)
3. [Performance Problems](#performance-problems)
4. [Integration Issues](#integration-issues)
5. [Testing Problems](#testing-problems)
6. [Getting Help](#getting-help)

## Build Issues

### CMake Configuration Fails

#### Issue: "Could not find cogutil"

**Cause**: OpenCog dependencies not installed or not in PKG_CONFIG_PATH

**Solution**:
```bash
# Install cogutil first
cd /path/to/pycog0/build
make cogutil
cd cogutil-build
sudo make install
sudo ldconfig

# Update PKG_CONFIG_PATH
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH

# Try configuration again
cd /path/to/agents/cpp/build
cmake ..
```

#### Issue: "Could NOT find Boost"

**Cause**: Boost libraries not installed

**Solution**:
```bash
# Ubuntu/Debian
sudo apt-get install -y libboost-all-dev

# Verify installation
dpkg -l | grep libboost

# Reconfigure
cmake ..
```

#### Issue: "C++ compiler does not support C++17"

**Cause**: Compiler too old

**Solution**:
```bash
# Update GCC
sudo apt-get install -y g++-9

# Set as default
export CXX=g++-9

# Verify
g++ --version

# Reconfigure
cmake ..
```

### Compilation Errors

#### Issue: "undefined reference to opencog::AtomSpace::..."

**Cause**: Linking issue with OpenCog libraries

**Solution**:
```bash
# Verify OpenCog libraries are installed
ls -la /usr/local/lib/libatomspace*

# Update library cache
sudo ldconfig

# Check if libraries are found
ldconfig -p | grep atomspace

# Add to LD_LIBRARY_PATH if needed
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

#### Issue: "fatal error: opencog/atomspace/AtomSpace.h: No such file"

**Cause**: Include paths not configured correctly

**Solution**:
```bash
# Verify headers are installed
ls -la /usr/local/include/opencog/atomspace/

# Add include path explicitly
cmake -DCMAKE_CXX_FLAGS="-I/usr/local/include" ..
```

### Link-Time Errors

#### Issue: Multiple definition errors

**Cause**: Headers included in multiple translation units without proper guards

**Solution**:
```cpp
// In header file, use:
#pragma once

// Or use traditional guards:
#ifndef AGENTZERO_COMPONENT_H
#define AGENTZERO_COMPONENT_H
// ... declarations ...
#endif
```

#### Issue: "cannot find -lagentzero-core"

**Cause**: Component not built yet

**Solution**:
```bash
# Build dependencies first
make agentzero-core

# Then build your component
make your-component
```

## Runtime Errors

### AtomSpace Errors

#### Issue: "Segmentation fault" when accessing AtomSpace

**Cause**: Invalid Handle or AtomSpace destroyed before use

**Solution**:
```cpp
// ❌ Bad: Dangling reference
Handle getDanglingHandle() {
    AtomSpace as;
    Handle h = as.add_node(CONCEPT_NODE, "test");
    return h;  // 'as' destroyed, Handle invalid
}

// ✅ Good: AtomSpace outlives Handles
class Component {
    AtomSpace& atomspace;  // Reference to long-lived AtomSpace
    
public:
    Component(AtomSpace& as) : atomspace(as) {}
    
    Handle createHandle() {
        return atomspace.add_node(CONCEPT_NODE, "test");
    }
};
```

#### Issue: "Handle::UNDEFINED returned unexpectedly"

**Cause**: Atom lookup failed or invalid operation

**Solution**:
```cpp
// Always check Handle validity
Handle h = atomspace.get_handle(type, name);
if (h == Handle::UNDEFINED) {
    // Handle not found - create or handle error
    logger().warn("Atom not found: {}", name);
    h = atomspace.add_node(type, name);
}
```

### Initialization Errors

#### Issue: "Agent not initialized"

**Cause**: Forgot to call initialize() before use

**Solution**:
```cpp
AgentZeroCore agent(atomspace);
agent.initialize();  // Must call before use
TS_ASSERT(agent.isInitialized());
```

#### Issue: "Component not found: {name}"

**Cause**: Component not registered

**Solution**:
```cpp
// Register component before use
auto component = std::make_shared<MyComponent>(atomspace);
agent.registerComponent("mycomponent", component);

// Now can retrieve
auto retrieved = agent.getComponent<MyComponent>("mycomponent");
```

### Memory Issues

#### Issue: Memory leaks detected

**Cause**: Resources not properly released

**Solution**:
```bash
# Run with Valgrind to identify leaks
valgrind --leak-check=full --show-leak-kinds=all ./your_program

# Look for "definitely lost" and "possibly lost"
# Fix by ensuring proper RAII and smart pointer usage
```

```cpp
// ✅ Good: Use RAII and smart pointers
class ResourceHolder {
    std::unique_ptr<Resource> resource_;
    
public:
    ResourceHolder() : resource_(std::make_unique<Resource>()) {}
    // Destructor automatically cleans up
};
```

#### Issue: Out of memory during execution

**Cause**: Unbounded growth of AtomSpace

**Solution**:
```cpp
// Implement periodic cleanup
void AgentZeroCore::cognitiveStep() {
    // ... normal processing ...
    
    // Periodic cleanup every N steps
    if (stepCount_ % 1000 == 0) {
        cleanupOldAtoms();
    }
}

void cleanupOldAtoms() {
    // Remove atoms with low importance
    auto allAtoms = atomspace.get_atoms_by_type(ATOM);
    for (Handle h : allAtoms) {
        if (h->getSTI() < threshold) {
            atomspace.remove_atom(h);
        }
    }
}
```

## Performance Problems

### Issue: Cognitive loop is too slow

**Diagnosis**:
```cpp
// Add timing to each phase
void AgentZeroCore::cognitiveStep() {
    auto start = std::chrono::high_resolution_clock::now();
    
    perceptionSystem_->process(data);
    logPhaseTime("Perception", start);
    
    start = std::chrono::high_resolution_clock::now();
    attentionSystem_->allocate();
    logPhaseTime("Attention", start);
    
    // ... other phases ...
}
```

**Solutions**:

1. **Optimize Pattern Matching**:
```cpp
// ❌ Slow: Get all atoms then filter
auto all = atomspace.get_atoms_by_type(ATOM);
HandleSeq filtered;
for (Handle h : all) {
    if (matches(h)) filtered.push_back(h);
}

// ✅ Fast: Use specific queries
Handle pattern = buildPattern();
HandleSeq results = satisfying_set(&atomspace, pattern);
```

2. **Reduce AtomSpace Size**:
```cpp
// Implement importance-based forgetting
void forget_low_importance_atoms(float threshold) {
    auto atoms = atomspace.get_atoms_by_type(ATOM);
    for (Handle h : atoms) {
        if (h->getSTI() < threshold) {
            atomspace.remove_atom(h, true);  // recursive removal
        }
    }
}
```

3. **Optimize Attention Allocation**:
```cpp
// Run ECAN less frequently for non-critical applications
if (stepCount_ % attentionInterval_ == 0) {
    attentionSystem_->allocate();
}
```

### Issue: High memory usage

**Diagnosis**:
```bash
# Monitor memory usage
top -p $(pgrep your_agent)

# Or use memory profiler
valgrind --tool=massif ./your_agent
ms_print massif.out.*
```

**Solutions**:

1. **Limit AtomSpace Growth**:
```cpp
void enforceAtomSpaceLimit(size_t maxAtoms) {
    if (atomspace.get_size() > maxAtoms) {
        // Remove least important atoms
        forget_low_importance_atoms(0.1);
    }
}
```

2. **Use Persistent Storage**:
```cpp
// Store infrequently used atoms in RocksDB
#include <opencog/persist/rocks/RocksStorage.h>

RocksStorage storage("agent_db");
storage.store_atomspace();  // Persist to disk
```

## Integration Issues

### OpenCog Component Issues

#### Issue: PLN reasoning not working

**Diagnosis**:
```cpp
// Check if rules are loaded
auto ruleBase = atomspace.get_node(CONCEPT_NODE, "PLNRuleBase");
auto rules = atomspace.get_incoming(ruleBase);
logger().info("Loaded {} rules", rules.size());
```

**Solution**:
```cpp
// Ensure PLN rules are loaded
#include <opencog/pln/rules/...>

void loadPLNRules(AtomSpace& as) {
    // Load standard PLN rules
    load_pln_config(as, "pln_config.scm");
    
    // Or manually add rules
    Handle rule = createDeductionRule(as);
    addRuleToBase(as, rule);
}
```

#### Issue: MOSES not finding good solutions

**Solutions**:

1. **Adjust Parameters**:
```cpp
moses_parameters params;
params.max_evals = 100000;  // Increase evaluations
params.pop_size = 500;       // Larger population
params.max_gens = 100;       // More generations
```

2. **Better Fitness Function**:
```cpp
// Ensure fitness function is smooth and informative
double fitness(const combo_tree& program) {
    // ❌ Bad: Binary (0 or 1)
    return program_works(program) ? 1.0 : 0.0;
    
    // ✅ Good: Gradient of quality
    double partial_correctness = measure_correctness(program);
    double efficiency = measure_efficiency(program);
    return 0.7 * partial_correctness + 0.3 * efficiency;
}
```

### CogServer Integration Issues

#### Issue: Cannot connect to CogServer

**Diagnosis**:
```bash
# Check if CogServer is running
ps aux | grep cogserver

# Try connecting
telnet localhost 17001
```

**Solution**:
```bash
# Start CogServer
cogserver -c cogserver.conf

# Or from code
#include <opencog/cogserver/server/CogServer.h>

CogServer& server = cogserver();
server.serverLoop();
```

#### Issue: Commands not recognized

**Solution**:
```cpp
// Ensure module is loaded
class AgentZeroModule : public Module {
public:
    void init() override {
        // Register commands
        cogserver().registerCommand("agent-step", 
                                   &AgentZeroModule::cmdStep);
    }
};

// Declare module
DECLARE_MODULE(AgentZeroModule);
```

## Testing Problems

### Issue: Tests failing randomly

**Cause**: Tests have shared state or depend on execution order

**Solution**:
```cpp
// Ensure proper setUp/tearDown
class MyTest : public CxxTest::TestSuite {
public:
    void setUp() override {
        // Fresh state for each test
        atomspace = std::make_unique<AtomSpace>();
        agent = std::make_unique<AgentZeroCore>(*atomspace);
    }
    
    void tearDown() override {
        // Clean up after each test
        agent.reset();
        atomspace.reset();
    }
};
```

### Issue: Tests pass locally but fail in CI

**Causes & Solutions**:

1. **Timing Issues**:
```cpp
// ❌ Bad: Hardcoded sleeps
std::this_thread::sleep_for(std::chrono::milliseconds(100));

// ✅ Good: Wait for condition
bool waitForCondition(std::function<bool()> cond, int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (!cond()) {
        if (elapsed(start) > timeoutMs) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}
```

2. **Environment Differences**:
```cpp
// Make tests environment-independent
void testFileOperations() {
    // ❌ Bad: Hardcoded paths
    std::string path = "/home/user/test.txt";
    
    // ✅ Good: Use temp directory
    std::string path = std::filesystem::temp_directory_path() / "test.txt";
}
```

### Issue: Memory leaks in tests

**Solution**:
```bash
# Run tests with Valgrind
ctest -T memcheck

# View results
cat Testing/Temporary/MemoryChecker.*.log

# Fix leaks by ensuring proper cleanup in tearDown()
```

## Getting Help

### Before Asking for Help

1. ✅ Check this troubleshooting guide
2. ✅ Search GitHub issues
3. ✅ Review relevant documentation
4. ✅ Try minimal reproduction case
5. ✅ Collect diagnostic information

### Diagnostic Information to Provide

```bash
# System information
uname -a
lsb_release -a

# Compiler version
g++ --version

# CMake version
cmake --version

# OpenCog versions
pkg-config --modversion cogutil
pkg-config --modversion atomspace

# Build configuration
cd build
cmake -LA | grep -i boost
cmake -LA | grep -i opencog

# Library paths
echo $LD_LIBRARY_PATH
echo $PKG_CONFIG_PATH
ldconfig -p | grep opencog
```

### Creating a Minimal Reproduction

```cpp
// Minimal example showing the problem
#include <opencog/atomspace/AtomSpace.h>
#include <agentzero/AgentZeroCore.h>

int main() {
    opencog::AtomSpace as;
    agentzero::AgentZeroCore agent(as);
    
    // Reproduce issue here
    agent.initialize();
    agent.cognitiveStep();  // Crashes here
    
    return 0;
}
```

### Where to Get Help

1. **GitHub Issues**: https://github.com/OpenCoq/pycog0/issues
   - For bugs and feature requests
   - Include minimal reproduction and diagnostic info

2. **Documentation**: Check all docs in `agents/cpp/docs/`
   - Especially [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md)
   - And [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md)

3. **OpenCog Community**:
   - Wiki: https://wiki.opencog.org/
   - Mailing lists and forums

4. **Code Examples**: Look at working examples in `agents/cpp/examples/`

### Debug Build for Better Error Messages

```bash
# Build with debug symbols and assertions
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-g -O0 -DDEBUG" \
      ..
make -j$(nproc)

# Run with debugger
gdb ./your_program
```

## Quick Reference

### Common Commands

```bash
# Clean rebuild
rm -rf build && mkdir build && cd build && cmake .. && make -j$(nproc)

# Verbose build
make VERBOSE=1

# Install to custom prefix
cmake -DCMAKE_INSTALL_PREFIX=/custom/path ..
sudo make install

# Run specific test
ctest -R TestName -V

# Check linking
ldd ./your_executable | grep opencog
```

### Environment Variables

```bash
# Essential environment setup
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
export CMAKE_PREFIX_PATH=/usr/local:$CMAKE_PREFIX_PATH

# Add to ~/.bashrc for persistence
```

---

*Part of the AGENT-ZERO-GENESIS documentation - Phase 9: Integration & Testing (AZ-DOC-001)*
