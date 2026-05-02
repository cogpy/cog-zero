# Agent-Zero Quick Start Guide

This guide will help you get Agent-Zero running with OpenCog in under 30 minutes.

## Prerequisites

### System Requirements
- **OS**: Ubuntu 20.04+ (or compatible Linux distribution)
- **CPU**: 4+ cores recommended
- **RAM**: 8GB minimum, 16GB recommended
- **Disk**: 10GB free space

### Required Software
- CMake 3.16+
- C++17 compliant compiler (GCC 9+, Clang 10+)
- Boost libraries 1.70+
- Git

## Installation

### Step 1: Install System Dependencies

```bash
# Update package manager
sudo apt-get update

# Install build essentials
sudo apt-get install -y build-essential cmake git

# Install Boost libraries
sudo apt-get install -y libboost-all-dev

# Install testing framework (optional but recommended)
sudo apt-get install -y cxxtest
```

### Step 2: Install OpenCog Dependencies

Agent-Zero requires the following OpenCog components:
- **cogutil**: Core utilities
- **atomspace**: Knowledge representation
- **cogserver**: Cognitive server

#### Option A: Quick Install (Recommended for Testing)

```bash
# Clone the unified OpenCog repository
git clone https://github.com/OpenCoq/pycog0.git
cd pycog0

# Use the automated dependency build
mkdir build && cd build
cmake ..
make cogutil
cd cogutil-build && sudo make install && sudo ldconfig
cd ..

make atomspace
cd atomspace-build && sudo make install && sudo ldconfig
cd ..

make cogserver
cd cogserver-build && sudo make install && sudo ldconfig
```

#### Option B: GitPod (Zero-Configuration Cloud Development)

Click the GitPod button in the main README to launch a complete development environment in your browser.

### Step 3: Build Agent-Zero

```bash
# Navigate to the Agent-Zero C++ directory
cd /path/to/pycog0/agents/cpp

# Create and enter build directory
mkdir build && cd build

# Configure the build
cmake ..

# Build Agent-Zero components
make -j$(nproc)

# Run tests (optional)
make test
```

## Quick Verification

### 1. Verify OpenCog Installation

```bash
# Check cogutil
pkg-config --modversion cogutil

# Check atomspace
pkg-config --modversion atomspace

# Check cogserver
pkg-config --modversion cogserver
```

Expected output: Version numbers for each component (e.g., `2.0.3`)

### 2. Run a Simple Demo

```bash
# From the build directory
cd agents/cpp/build

# Run the basic cognitive loop demo
./examples/CognitiveLoopIntegrationDemo
```

Expected output:
```
Initializing Agent-Zero Core...
Loading AtomSpace...
Starting Cognitive Loop...
[Iteration 1] Processing...
[Iteration 2] Processing...
...
Demo completed successfully!
```

## First Steps with Agent-Zero

### Example 1: Creating and Using an Agent

Create a file `my_first_agent.cpp`:

```cpp
#include <opencog/atomspace/AtomSpace.h>
#include <agentzero/AgentZeroCore.h>

using namespace opencog;
using namespace agentzero;

int main() {
    // Initialize AtomSpace
    AtomSpace atomspace;
    
    // Create Agent-Zero core
    AgentZeroCore agent(atomspace);
    
    // Initialize the agent
    agent.initialize();
    
    // Add a simple goal
    Handle goal = agent.addGoal("Learn about the world");
    
    // Run cognitive loop for 10 iterations
    for (int i = 0; i < 10; i++) {
        agent.cognitiveStep();
    }
    
    // Check goal status
    auto status = agent.getGoalStatus(goal);
    std::cout << "Goal status: " << status << std::endl;
    
    return 0;
}
```

Build and run:
```bash
g++ -std=c++17 my_first_agent.cpp -o my_agent \
    -I/usr/local/include \
    -L/usr/local/lib \
    -lagentzero-core -latomspace -lcogutil \
    -lboost_system -lboost_filesystem
    
./my_agent
```

### Example 2: Perception and Action

```cpp
#include <agentzero/PerceptualProcessor.h>
#include <agentzero/ActionScheduler.h>

using namespace agentzero;

int main() {
    AtomSpace atomspace;
    AgentZeroCore agent(atomspace);
    
    // Set up perception
    PerceptualProcessor perceiver(atomspace);
    
    // Process sensory input
    SensoryData input = getSensorData(); // Your sensor interface
    perceiver.process(input);
    
    // Agent processes percepts and decides actions
    agent.cognitiveStep();
    
    // Execute scheduled actions
    ActionScheduler scheduler;
    scheduler.executeReady();
    
    return 0;
}
```

## Common Tasks

### Adding a New Goal

```cpp
// Simple goal
Handle goal = agent.addGoal("Complete task X");

// Goal with priority
Handle priorityGoal = agent.addGoal("Urgent task", 0.9);

// Hierarchical goal
Handle parent = agent.addGoal("Master skill Y");
Handle child = agent.addSubGoal(parent, "Learn component A");
```

### Querying the Knowledge Base

```cpp
// Get all concepts
HandleSeq concepts = atomspace.get_atoms_by_type(CONCEPT_NODE);

// Pattern matching
Handle pattern = /* define your pattern */;
HandleSeq matches = agent.findMatches(pattern);

// Reasoning with PLN
auto result = agent.reason(query, maxSteps);
```

### Learning from Experience

```cpp
#include <agentzero/ExperienceManager.h>

ExperienceManager memory(atomspace);

// Record an experience
Experience exp = {
    .context = getCurrentContext(),
    .action = lastAction,
    .outcome = observedResult,
    .reward = calculateReward()
};
memory.store(exp);

// Learn from experiences
memory.consolidate();
agent.improvePolicy();
```

## Next Steps

Now that you have Agent-Zero running:

1. **Explore Examples**: Check `agents/cpp/examples/` for more demos
2. **Read Architecture**: Understand the design in [ARCHITECTURE.md](ARCHITECTURE.md)
3. **API Documentation**: Dive into [API_REFERENCE.md](API_REFERENCE.md)
4. **Integration Patterns**: Learn advanced techniques in [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md)
5. **Write Tests**: Follow [TESTING_GUIDE.md](TESTING_GUIDE.md)

## Troubleshooting

### Build Errors

**"Could not find cogutil"**
```bash
# Make sure cogutil is installed
cd /path/to/pycog0/build
make cogutil
cd cogutil-build && sudo make install && sudo ldconfig
```

**"Boost not found"**
```bash
sudo apt-get install -y libboost-all-dev
```

**"C++17 features not available"**
```bash
# Update your compiler
sudo apt-get install -y g++-9
export CXX=g++-9
```

### Runtime Errors

**"AtomSpace initialization failed"**
- Check that atomspace is properly installed
- Verify library paths: `echo $LD_LIBRARY_PATH`
- Run: `sudo ldconfig`

**"Symbol not found errors"**
- Rebuild all components in order: cogutil → atomspace → cogserver → agents
- Ensure consistent compiler flags across all builds

### Getting Help

- **Documentation**: See [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
- **GitHub Issues**: https://github.com/OpenCoq/pycog0/issues
- **OpenCog Community**: https://wiki.opencog.org/

## Performance Tips

For production use, build with optimizations:

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

Enable specific optimizations:
```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-O3 -march=native" \
      ..
```

## What You've Learned

✅ Installed OpenCog dependencies  
✅ Built Agent-Zero from source  
✅ Ran your first cognitive agent  
✅ Created goals and executed cognitive loops  
✅ Understood basic perception-action cycles  

Ready to build more complex agents? Continue with the [Developer Guide](DEVELOPER_GUIDE.md)!

---

*Part of the AGENT-ZERO-GENESIS documentation - Phase 9: Integration & Testing (AZ-DOC-001)*
