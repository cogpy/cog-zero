# Agent-Zero Tools Module

## Overview

The Agent-Zero Tools module provides the **CapabilityComposer** component, which enables intelligent composition and coordination of multiple tools and capabilities for complex task execution.

## Task ID: AZ-TOOL-003

**Phase**: 8 - Tool Integration  
**Status**: Implemented  
**Dependencies**: external-tools, ros-behavior-scripting

## CapabilityComposer

### Purpose

CapabilityComposer combines multiple tools and capabilities to accomplish complex tasks that require coordinated execution of multiple simpler capabilities. It provides:

- **Capability Registration**: Register tools and capabilities with their dependencies
- **Automatic Composition**: Automatically plan execution sequences based on task requirements
- **Dependency Resolution**: Resolve and order capabilities based on their dependencies
- **AtomSpace Integration**: Represent capabilities and plans in the AtomSpace
- **Execution Coordination**: Execute composed plans with proper ordering
- **Statistics Tracking**: Track execution success rates and performance metrics

### Key Features

1. **Intelligent Composition**
   - Automatic task decomposition
   - Dependency graph analysis
   - Topological sorting for execution order
   - Parallel execution opportunities (planned)

2. **Capability Management**
   - Dynamic capability registration/unregistration
   - Dependency validation
   - Provider lookup (find capabilities that provide specific outputs)
   - Execution statistics tracking

3. **Plan Management**
   - Automatic plan generation from task requirements
   - Plan validation
   - Plan caching and reuse
   - Success probability estimation

4. **OpenCog Integration**
   - AtomSpace representation of capabilities
   - AtomSpace representation of composition plans
   - Execution history tracking in AtomSpace
   - Integration with other OpenCog cognitive components

### Architecture

```
CapabilityComposer
├── Capability Registry
│   ├── Capability definitions
│   ├── Dependency graph
│   └── Provider index
├── Composition Planner
│   ├── Task requirements analysis
│   ├── Dependency resolution
│   ├── Plan generation
│   └── Plan validation
├── Execution Engine
│   ├── Capability execution
│   ├── Context management
│   └── Statistics tracking
└── AtomSpace Integration
    ├── Capability atoms
    ├── Plan atoms
    └── Execution history
```

### Usage Example

```cpp
#include <opencog/agentzero/tools/CapabilityComposer.h>

// Initialize
auto atomspace = std::make_shared<AtomSpace>();
auto composer = std::make_unique<CapabilityComposer>(atomspace);

// Register capabilities
composer->registerCapability(
    "sensor_read",
    "Sensor Reading",
    "Read data from sensors",
    sensorReadFunction,
    {}  // No dependencies
);

composer->registerCapability(
    "path_planning",
    "Path Planning",
    "Plan movement path",
    pathPlanningFunction,
    {"sensor_read"}  // Depends on sensor_read
);

// Compose and execute a task
CapabilityComposer::TaskRequirements requirements;
requirements.task_description = "Navigate to target";
requirements.required_outputs = {"path_planning"};

CapabilityComposer::ExecutionContext context;
context.atomspace = atomspace;

auto result = composer->composeAndExecute(requirements, context);
```

## Building

The module is built as part of the Agent-Zero C++ framework:

```bash
cd /home/runner/work/pycog0/pycog0
mkdir -p build && cd build
cmake ..
make agentzero-tools
```

## Testing

Run the test suite:

```bash
cd build
make test
# Or run specific test:
./agents/cpp/agentzero-tools/tests/CapabilityComposerSimpleTest
```

## Examples

Run the demonstration:

```bash
cd build
./agents/cpp/agentzero-tools/examples/CapabilityComposerDemo
```

This demonstrates a complex robotic manipulation task with automatic capability composition.

## API Reference

### Main Classes

#### CapabilityComposer

Main class for capability composition and execution.

**Key Methods:**
- `registerCapability()` - Register a new capability
- `composeForTask()` - Generate composition plan for task
- `executePlan()` - Execute a composition plan
- `composeAndExecute()` - Compose and execute in one step
- `getCapabilityStatistics()` - Get execution statistics

#### Capability

Structure representing a single capability or tool.

**Fields:**
- `capability_id` - Unique identifier
- `name` - Human-readable name
- `description` - What the capability does
- `required_capabilities` - Dependencies
- `provided_capabilities` - Outputs this provides
- `execute` - Execution function

#### CompositionPlan

Structure representing an execution plan.

**Fields:**
- `plan_id` - Unique identifier
- `capability_sequence` - Ordered execution sequence
- `dependency_graph` - Capability dependencies
- `estimated_success_probability` - Success estimate
- `is_valid` - Whether plan is valid

#### ExecutionContext

Context for capability execution.

**Fields:**
- `atomspace` - Shared AtomSpace
- `input_parameters` - Input data
- `output_results` - Output data
- `execution_log` - Execution history

## Integration with OpenCog Components

### external-tools
The CapabilityComposer can integrate with external tools by wrapping them as capabilities. External visualization, monitoring, and diagnostic tools can be registered as capabilities and composed into complex workflows.

### ros-behavior-scripting
ROS behavior scripts can be wrapped as capabilities, enabling composition of complex robotic behaviors. The dependency resolution ensures proper sequencing of sensor inputs, planning, and motor outputs.

### AtomSpace
All capabilities and plans are represented in the AtomSpace, enabling:
- Querying capabilities with the pattern matcher
- Learning from execution history
- Integration with PLN reasoning
- Persistence across sessions

## Performance Characteristics

- **Capability Registration**: O(1)
- **Dependency Resolution**: O(V + E) where V = capabilities, E = dependencies
- **Plan Composition**: O(V + E) + O(V log V) for topological sort
- **Plan Execution**: O(n) where n = number of capabilities in plan

## Configuration Options

```cpp
composer->setMaxCachedPlans(100);              // Maximum cached plans
composer->setCompositionTimeout(30.0);         // Composition timeout (seconds)
composer->enableAutomaticComposition(true);    // Auto-compose when needed
composer->enableParallelExecution(false);      // Parallel execution (future)
composer->setMaxCompositionDepth(10);          // Max dependency depth
```

## Future Enhancements

- [ ] Parallel execution of independent capabilities
- [ ] Cost-based composition optimization
- [ ] Learning from execution history
- [ ] Integration with MOSES for capability optimization
- [ ] Real-time capability discovery
- [ ] Distributed capability execution
- [ ] Capability versioning and updates

## References

- AGENT-ZERO-GENESIS.md - Overall project architecture
- AZ-TOOL-001: ToolRegistry (companion task)
- AZ-TOOL-002: ToolWrapper (companion task)
- AZ-TOOL-003: CapabilityComposer (companion task)
- AZ-RESOURCE-001: ResourceManager (implemented)

## ResourceManager - AZ-RESOURCE-001

### Overview

The ResourceManager component provides comprehensive resource optimization and management for Agent-Zero. It manages computational and physical resources including CPU, memory, disk, network, GPU, AtomSpace capacity, and tool instances.

### Key Features

- **Multi-Resource Management**: CPU, memory, disk, network, GPU, AtomSpace, and custom resources
- **Resource Pooling**: Create and manage multiple pools per resource type
- **Allocation Strategies**: Multiple optimization strategies (First-Fit, Best-Fit, Worst-Fit, Balanced, Priority-Based, Adaptive)
- **Time-Based Allocation**: Support for temporary allocations with automatic expiration
- **Resource Tracking**: Comprehensive statistics and usage monitoring
- **AtomSpace Integration**: Full integration with OpenCog's knowledge representation
- **Thread-Safe**: Safe for concurrent access from multiple threads
- **Auto-Cleanup**: Automatic cleanup of expired allocations

### Usage Example

```cpp
#include <opencog/agentzero/tools/ResourceManager.h>

// Create ResourceManager with AtomSpace
auto atomspace = std::make_shared<AtomSpace>();
auto manager = std::make_unique<ResourceManager>(atomspace);

// Create resource pools
manager->createResourcePool(ResourceType::CPU, "CPU_Pool", 100.0);
manager->createResourcePool(ResourceType::MEMORY, "Memory_Pool", 16384.0);

// Allocate resources
auto cpu_alloc = manager->allocateResource("task_1", ResourceType::CPU, 25.0, 60.0);
auto mem_alloc = manager->allocateResource("task_1", ResourceType::MEMORY, 2048.0);

// Check resource availability
bool has_cpu = manager->hasAvailableResources(ResourceType::CPU, 50.0);
double cpu_usage = manager->getResourceUsage(ResourceType::CPU);

// Deallocate when done
manager->deallocateResource(cpu_alloc->getAllocationId());
manager->deallocateResourcesForRequester("task_1");

// Get statistics
std::string stats = manager->getStatistics();
std::cout << stats << std::endl;
```

### Resource Types

- **CPU**: CPU processing units
- **MEMORY**: RAM in megabytes
- **DISK**: Disk storage in megabytes
- **NETWORK**: Network bandwidth in Mbps
- **GPU**: GPU processing units
- **ATOMSPACE**: AtomSpace node/link capacity
- **TOOL_INSTANCE**: Tool execution instance slots
- **CUSTOM**: Custom resource types

### Optimization Strategies

1. **FIRST_FIT**: Allocate from first pool with sufficient capacity (fastest)
2. **BEST_FIT**: Allocate from pool with least waste (most efficient)
3. **WORST_FIT**: Allocate from pool with most space (best for large allocations)
4. **BALANCED**: Balance allocation across all pools (default, most fair)
5. **PRIORITY_BASED**: Allocate based on requester priority (extensible)
6. **ADAPTIVE**: Adaptive strategy based on usage patterns (learning-based)

### Building

ResourceManager is built as part of the agentzero-tools library:

```bash
cd /home/runner/work/pycog0/pycog0
mkdir -p build && cd build
cmake ..
make agentzero-tools
```

### Testing

Run the ResourceManager tests:

```bash
cd build
make test
# Or run specific test:
./agents/cpp/agentzero-tools/tests/ResourceManagerSimpleTest
```

### Examples

Run the ResourceManager demonstration:

```bash
cd build
./agents/cpp/agentzero-tools/examples/ResourceManagerDemo
```

This demonstrates:
- Basic resource management
- Multiple optimization strategies
- Time-based allocation with expiration
- AtomSpace integration
- Complex multi-task scenarios

### Performance Characteristics

- **Pool Creation**: O(1)
- **Allocation**: O(n) where n = number of pools for resource type
- **Deallocation**: O(m) where m = active allocations in pool
- **Resource Query**: O(n) where n = pools for resource type
- **Cleanup**: O(m) where m = total active allocations

### Configuration

```cpp
// Set optimization strategy
manager->setOptimizationStrategy(OptimizationStrategy::BALANCED);

// Configure auto-cleanup
manager->setAutoCleanupEnabled(true);
manager->setCleanupInterval(60.0);  // seconds

// Set pool thresholds
auto pool = manager->getResourcePool("CPU_Pool");
pool->setThresholds(0.75, 0.90);  // warning at 75%, critical at 90%
```

### Integration with OpenCog

ResourceManager integrates seamlessly with OpenCog components:

- **AtomSpace**: Resource pools and allocations represented as atoms
- **CogServer**: Can be exposed via CogServer commands
- **PLN**: Resource allocation can be reasoned about
- **URE**: Rules can be created for dynamic resource optimization

## License

AGPL-3.0-or-later

## Authors

OpenCog Foundation (2024)
The Agent-Zero Tools module provides a unified interface for integrating external tools with the Agent-Zero cognitive architecture and OpenCog AtomSpace. This module implements **AZ-TOOL-002: ToolWrapper unified interface** from Phase 8 of the AGENT-ZERO-GENESIS project.

## Features

- **Unified Interface**: Single interface for multiple tool types (REST API, ROS, Python scripts, shell commands, AtomSpace queries, custom tools)
- **AtomSpace Integration**: Full integration with OpenCog's knowledge representation system
- **Execution Modes**: Support for both synchronous and asynchronous execution
- **Error Handling**: Comprehensive error handling with detailed error messages and status tracking
- **Performance Tracking**: Built-in execution statistics and performance monitoring
- **Resource Management**: Timeout management and resource constraint enforcement
- **Extensibility**: Easy to extend with custom tool types and executors

## Architecture

### Core Components

#### 1. ToolWrapper
Main class providing the unified interface for tool integration.

**Key Features:**
- Tool identification and configuration
- Multiple tool type support
- AtomSpace representation
- Execution statistics tracking
- Custom executor support

#### 2. ToolExecutionContext
Encapsulates the execution context for a tool.

**Provides:**
- Input parameters
- Configuration settings
- AtomSpace access
- Input atoms for processing
- Timeout and execution mode settings

#### 3. ToolResult
Encapsulates the result of tool execution.

**Contains:**
- Execution status
- Output data (string, atoms, structured data)
- Error information
- Execution metadata
- Performance metrics

### Supported Tool Types

1. **EXTERNAL_REST_API**: External tools accessible via REST API
2. **ROS_BEHAVIOR**: ROS behavior scripting and robot control
3. **PYTHON_SCRIPT**: Python script execution
4. **SHELL_COMMAND**: Shell command execution
5. **ATOMSPACE_QUERY**: Direct AtomSpace queries
6. **CUSTOM**: Custom tool implementations with user-defined executors

## Usage Examples

### Example 1: Custom Tool with Lambda Executor

```cpp
#include <opencog/agentzero/tools/ToolWrapper.h>

// Create AtomSpace
AtomSpacePtr atomspace = createAtomSpace();

// Create custom tool
auto tool = std::make_shared<ToolWrapper>("sentiment_analyzer", ToolType::CUSTOM, atomspace);
tool->setDescription("Analyzes sentiment of text");

// Set custom executor
tool->setCustomExecutor([](const ToolExecutionContext& context) {
    ToolResult result(ToolStatus::COMPLETED);
    std::string text = context.getParameter("text");
    
    // Process text...
    result.setOutput("Sentiment: positive");
    result.setMetadata("score", "0.8");
    
    return result;
});

// Execute tool
ToolExecutionContext context(atomspace);
context.setParameter("text", "This is great!");
ToolResult result = tool->execute(context);
```

### Example 2: REST API Tool

```cpp
// Create REST API tool
auto tool = std::make_shared<ToolWrapper>("face_detector", ToolType::EXTERNAL_REST_API, atomspace);
tool->setToolEndpoint("http://localhost:5000/api/detect_faces");
tool->setToolConfig("api_key", "my_key");

// Add required parameters
tool->addRequiredParameter("image_url");

// Execute
ToolExecutionContext context(atomspace);
context.setParameter("image_url", "http://example.com/image.jpg");
context.setTimeout(10000.0);
ToolResult result = tool->execute(context);
```

### Example 3: ROS Behavior Tool

```cpp
// Create ROS behavior tool
auto tool = std::make_shared<ToolWrapper>("robot_movement", ToolType::ROS_BEHAVIOR, atomspace);
tool->setToolEndpoint("/robot/move_to");

// Execute movement command
ToolExecutionContext context(atomspace);
context.setParameter("x", "1.5");
context.setParameter("y", "2.0");
ToolResult result = tool->execute(context);
```

### Example 4: AtomSpace Query Tool

```cpp
// Create query tool
auto tool = std::make_shared<ToolWrapper>("location_query", ToolType::ATOMSPACE_QUERY, atomspace);

// Add input atoms for query
ToolExecutionContext context(atomspace);
context.addInputAtom(person_atom);
context.addInputAtom(location_atom);

// Execute query
ToolResult result = tool->execute(context);

// Access AtomSpace results
const HandleSeq& results = result.getAtomSpaceResults();
```

## Integration with OpenCog

### AtomSpace Representation

Each ToolWrapper instance creates its representation in the AtomSpace:

```scheme
(ConceptNode "Tool_face_detector")
(InheritanceLink
    (ConceptNode "Tool_face_detector")
    (ConceptNode "ToolType"))
```

### Execution Tracking

Tool executions are recorded in the AtomSpace:

```scheme
(ConceptNode "Execution_face_detector_1")
(EvaluationLink
    (ConceptNode "Tool_face_detector")
    (ConceptNode "Execution_face_detector_1"))
```

## Dependencies

### Required
- **cogutil**: OpenCog utilities library
- **atomspace**: OpenCog AtomSpace library
- **Boost**: C++ Boost libraries (system, filesystem, thread)

### Optional (for specific tool types)
- **libcurl**: For REST API tools
- **ROS**: For ROS behavior tools
- **Python**: For Python script execution

## Building

### CMake Configuration

```bash
mkdir build && cd build
cmake -DBUILD_TESTING=ON -DBUILD_EXAMPLES=ON ..
make
```

### Build Options

- `BUILD_TESTING`: Build unit tests (default: ON)
- `BUILD_EXAMPLES`: Build example applications (default: ON)

### Running Tests

```bash
cd build
make test
# or
ctest --verbose
```

### Running Examples

```bash
./bin/examples/ToolWrapperDemo
```

## Performance

### Execution Statistics

ToolWrapper automatically tracks:
- Total execution count
- Success/failure counts
- Success rate
- Average execution time
- Total execution time

Access statistics:

```cpp
std::cout << "Executions: " << tool->getExecutionCount() << std::endl;
std::cout << "Success Rate: " << tool->getSuccessRate() << std::endl;
std::cout << "Avg Time: " << tool->getAverageExecutionTime() << "ms" << std::endl;
std::cout << "Statistics JSON: " << tool->getStatistics() << std::endl;
```

### Resource Management

- **Timeout Control**: Set maximum execution time
- **Async Execution**: Non-blocking tool execution (coming soon)
- **Memory Efficiency**: Minimal overhead per tool instance

## Error Handling

### Status Codes

- `NOT_STARTED`: Tool has not been executed
- `RUNNING`: Tool is currently executing
- `COMPLETED`: Execution completed successfully
- `FAILED`: Execution failed
- `TIMEOUT`: Execution exceeded timeout limit
- `CANCELLED`: Execution was cancelled

### Error Information

```cpp
ToolResult result = tool->execute(context);
if (!result.isSuccess()) {
    std::cerr << "Error: " << result.getErrorMessage() << std::endl;
    std::cerr << "Status: " << result.toString() << std::endl;
}
```

## Extensibility

### Creating Custom Tool Types

1. Extend ToolType enum (if needed)
2. Implement executor function
3. Register with ToolWrapper

```cpp
// Create custom tool
auto tool = std::make_shared<ToolWrapper>("my_tool", ToolType::CUSTOM);

// Set custom executor
tool->setCustomExecutor([](const ToolExecutionContext& context) {
    // Your implementation here
    ToolResult result(ToolStatus::COMPLETED);
    result.setOutput("Custom processing complete");
    return result;
});
```

## Integration with external-tools and ros-behavior-scripting

### External Tools Integration

The ToolWrapper provides a unified interface for tools in the `external-tools` repository:

- AtomSpace visualization tools
- Performance monitoring tools
- Diagnostic tools
- REST API-based tools

### ROS Behavior Scripting Integration

Integration with `ros-behavior-scripting` enables:

- Sensory input processing (vision, audio)
- Motor control and movement
- Behavior coordination
- ROS topic/service communication

## Testing

### Unit Tests

Comprehensive unit tests cover:
- ToolResult functionality
- ToolExecutionContext functionality
- ToolWrapper core features
- AtomSpace integration
- Error handling
- Statistics tracking

Run tests:
```bash
cd build
make test
```

### Integration Tests

Integration tests verify:
- OpenCog component compatibility
- AtomSpace operations
- Multi-tool coordination
- Performance benchmarks

## Documentation

### API Documentation

Generate API documentation with Doxygen:

```bash
cd build
cmake -DBUILD_DOCS=ON ..
make docs
```

### Code Examples

See `examples/` directory for complete working examples:
- `ToolWrapperDemo.cpp`: Comprehensive demonstration of all features

## Future Enhancements

### Planned Features (Phase 8 continuation)

- **AZ-TOOL-001**: ToolRegistry catalog for managing multiple tools
- **AZ-TOOL-003**: CapabilityComposer for combining tools
- **AZ-RESOURCE-001**: ResourceManager for optimization

### Implementation Roadmap

1. **Async Execution**: Full asynchronous execution support with callbacks
2. **Tool Discovery**: Automatic tool discovery and registration
3. **Capability Composition**: Combine multiple tools for complex tasks
4. **Performance Optimization**: Advanced caching and resource management
5. **Security**: Enhanced security for shell command and script execution

## Contributing

When contributing to the Agent-Zero Tools module:

1. Follow OpenCog coding standards
2. Add comprehensive tests for new features
3. Update documentation
4. Ensure AtomSpace integration
5. Maintain backward compatibility

## License

Copyright (C) 2024 OpenCog Foundation
SPDX-License-Identifier: AGPL-3.0-or-later

## Support

For questions or issues:
- OpenCog GitHub: https://github.com/opencog
- OpenCog Wiki: https://wiki.opencog.org
- Agent-Zero Genesis: See AGENT-ZERO-GENESIS.md

## References

- [AGENT-ZERO-GENESIS.md](../../AGENT-ZERO-GENESIS.md): Master blueprint
- [OpenCog AtomSpace](https://github.com/opencog/atomspace): Knowledge representation
- [external-tools](https://github.com/opencog/external-tools): External tool repository
- [ros-behavior-scripting](https://github.com/opencog/ros-behavior-scripting): ROS integration
This module implements the Tool Integration Framework for Agent-Zero integrated with OpenCog.

## Overview

The Agent-Zero Tools module provides a comprehensive catalog system for managing external tools and capabilities that Agent-Zero can utilize. It serves as the integration point for external-tools and ros-behavior-scripting components.

## Key Components

- **ToolRegistry**: Catalog of available tools and capabilities
  - Tool registration and discovery
  - Capability-based tool matching
  - Tool composition for complex tasks
  - Dynamic tool availability tracking
  - AtomSpace integration for tool metadata

## Features

### Tool Management
- Register and unregister tools dynamically
- Track tool status (available, busy, unavailable, error)
- Monitor tool reliability and usage statistics
- Support for tool categories (visualization, analysis, robotics, etc.)

### Capability Matching
- Query tools by capabilities (read-only, async execution, batch processing, etc.)
- Search tools by keywords
- Filter tools by category
- Validate tool dependencies

### Tool Composition
- Compose multiple tools into execution chains
- Validate tool compatibility
- Execute sequential tool pipelines
- Support for complex task decomposition

### Integration Points
- **external-tools**: Visualization, import/export, and utility tools
- **ros-behavior-scripting**: Robotics perception and motor control tools
- **AtomSpace**: Tool metadata representation and reasoning

## Dependencies

- cogutil
- atomspace
- external-tools (optional, for tool discovery)
- ros-behavior-scripting (optional, for robotics tools)

## Build Instructions

This module is built as part of the overall Agent-Zero system. To build separately:

```bash
mkdir build && cd build
cmake ..
make
make test  # Run tests (requires GTest)
make install
```

## Usage Example

```cpp
#include <opencog/agentzero/ToolRegistry.h>

// Create tool registry
AtomSpacePtr as = createAtomSpace();
ToolRegistry registry(as);

// Register a custom tool
ToolRegistry::ToolMetadata metadata;
metadata.name = "MyCustomTool";
metadata.description = "A custom analysis tool";
metadata.category = ToolRegistry::ToolCategory::ANALYSIS;
metadata.capabilities = {
    ToolRegistry::ToolCapability::READ_WRITE,
    ToolRegistry::ToolCapability::BATCH_PROCESSING
};

auto executor = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
    // Tool implementation
    return as->add_node(CONCEPT_NODE, "ToolResult");
};

Handle tool_atom = registry.registerTool(metadata, executor);

// Discover tools
auto all_tools = registry.getAllTools();
auto viz_tools = registry.getToolsByCategory(ToolRegistry::ToolCategory::VISUALIZATION);

// Search and execute
auto results = registry.searchTools("analyze");
if (!results.empty()) {
    HandleSeq args;
    Handle result = registry.executeTool(results[0], args);
}

// Tool composition
std::vector<std::string> chain = {"Tool1", "Tool2", "Tool3"};
HandleSeq initial_input;
Handle final_result = registry.executeToolChain(chain, initial_input);

// Get statistics
auto stats = registry.getToolStatistics();
for (const auto& [name, stat] : stats) {
    std::cout << name << ": " << stat.first << " uses, "
              << stat.second << " reliability\n";
}
```

## Tool Categories

- **VISUALIZATION**: AtomSpace visualization and display tools
- **ANALYSIS**: Data analysis and statistical tools
- **IMPORT_EXPORT**: Data import/export utilities
- **ROBOTICS**: Robotics integration and control
- **PERCEPTION**: Sensory input processing
- **MOTOR_CONTROL**: Motor and movement control
- **COMMUNICATION**: Communication interfaces
- **UTILITY**: General utility tools
- **CUSTOM**: User-defined tool types

## Tool Capabilities

- **READ_ONLY**: Tool only reads data
- **READ_WRITE**: Tool can modify data
- **ASYNC_EXECUTION**: Supports asynchronous execution
- **BATCH_PROCESSING**: Can process multiple items
- **REAL_TIME**: Operates in real-time
- **REQUIRES_ROS**: Requires ROS environment
- **NETWORK_ACCESS**: Requires network connectivity

## Integration with OpenCog

### AtomSpace Representation
- Tools represented as CONCEPT_NODE atoms
- Tool categories linked via MEMBER_LINK
- Tool reliability stored in TruthValues
- Tool relationships expressed through Links

### Tool Discovery
- Automatic discovery of external-tools components
- ROS tool detection when ROS is available
- Dynamic tool registration at runtime
- Dependency checking for tool availability

## Testing

Comprehensive unit tests are provided using Google Test:

```bash
make test
# Or run specific tests
./tests/ToolRegistryTest
```

Tests cover:
- Tool registration and unregistration
- Tool discovery and search
- Capability matching
- Tool execution and composition
- Reliability tracking
- Statistics management
- AtomSpace integration

## Development Status

✅ **Implemented** - ToolRegistry catalog system is complete and functional.

See [AGENT-ZERO-GENESIS.md](../../../AGENT-ZERO-GENESIS.md) for the complete development roadmap.

## Performance

- Tool lookup: O(1) for registered tools
- Category search: O(n) where n is tools in category
- Capability matching: O(n*m) where n is tools, m is capabilities
- Tool execution: Depends on tool implementation
- Reliability updates: O(1) with exponential moving average

## Future Enhancements

- PLN-based tool composition reasoning
- Machine learning for tool selection optimization
- Distributed tool execution
- Tool versioning and compatibility management
- Advanced dependency resolution
- Tool marketplace integration

## Related Components

- **agentzero-core**: Main agent orchestration
- **external-tools**: Visualization and utility tools
- **ros-behavior-scripting**: ROS integration tools
- **cogserver**: Server for tool management interface

## License

AGPL-3.0-or-later - See LICENSE file for details

## Contributors

OpenCog Foundation - Agent-Zero-Genesis Project
