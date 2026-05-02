# Agent-Zero Planning Module (AZ-SPATIAL-001)

This module implements spatial-temporal integration for Agent-Zero temporal planning, bridging OpenCog's spacetime component with the Agent-Zero action scheduling system.

## Overview

The planning module provides comprehensive spatial-temporal reasoning capabilities for action scheduling, including:

- **Spatial-Temporal Data Management**: Track atoms in 3D space over time
- **Timeline-Based Reasoning**: Query temporal sequences and optimize scheduling
- **Trajectory Planning**: Plan collision-free paths through space and time
- **Constraint Validation**: Ensure spatial and temporal constraints are satisfied
- **Optimal Scheduling**: Find optimal time windows based on spatial preferences

## Architecture

### Core Components

#### SpaceTimeIntegrator
The main bridge between OpenCog's spacetime component and Agent-Zero planning:
- Manages spatial-temporal data using TimeOctomap
- Provides query capabilities for locations, timelines, and relationships
- Handles trajectory planning and validation
- Integrates with AtomSpace for knowledge representation

#### ActionScheduler Extensions
Enhanced scheduling capabilities:
- `scheduleActionWithSpatialConstraints()` - Schedule with location requirements
- `scheduleActionWithTrajectory()` - Schedule with planned movement paths
- `scheduleActionWithOptimalPlanning()` - Find optimal time/location combinations

### Integration Points

1. **AtomSpace Integration**: All spatial-temporal data is represented as Atoms
2. **OpenCog Spacetime**: Uses TimeOctomap for efficient spatial-temporal storage
3. **Action Scheduling**: Extends existing ActionScheduler with spatial awareness
4. **OpenCog Patterns**: Follows established OpenCog architectural conventions

## Quick Start

### Building

```bash
# Ensure dependencies are installed
sudo apt-get install liboctomap-dev

# Build core dependencies first
cd /tmp/opencog-build
make cogutil
make atomspace  
make spacetime

# Install dependencies
cd cogutil-build && sudo make install && sudo ldconfig
cd ../atomspace-build && sudo make install && sudo ldconfig
cd ../spacetime-build && sudo make install && sudo ldconfig

# Build the planning module
make agents  # This includes agentzero-planning
```

### Basic Usage

```cpp
#include <opencog/atomspace/AtomSpace.h>
#include "opencog/agentzero/SpaceTimeIntegrator.h"
#include "opencog/agentzero/ActionScheduler.h"

// Create AtomSpace and integrator
AtomSpacePtr atomspace = createAtomSpace();
auto integrator = std::make_unique<SpaceTimeIntegrator>(atomspace);

// Configure spatial-temporal features
SpaceTimeIntegrator::Configuration config;
config.spatial_resolution = 0.1;  // 10cm resolution
config.time_resolution = std::chrono::milliseconds(100);
integrator->configure(config);

// Create action scheduler with spatial-temporal capabilities
ActionScheduler scheduler(nullptr, atomspace);
scheduler.configureSpatialTemporalFeatures(true, 0.1, 100);

// Schedule action with spatial constraints
Handle robot_action = atomspace->add_node(CONCEPT_NODE, "MoveRobot");
std::vector<octomap::point3d> locations = {
    octomap::point3d(1.0, 2.0, 0.0)
};
auto start_time = std::chrono::steady_clock::now() + std::chrono::seconds(5);

auto result = scheduler.scheduleActionWithSpatialConstraints(
    robot_action, locations, start_time, 0.1, 10);
```

## API Reference

### SpaceTimeIntegrator

#### Configuration
```cpp
struct Configuration {
    double spatial_resolution = 0.1;           // Spatial resolution in meters
    std::chrono::milliseconds time_resolution{100}; // Time resolution
    unsigned int time_units = 1000;           // Number of time units to track
    bool enable_spatial_constraints = true;   // Enable spatial constraint checking
    bool enable_trajectory_planning = true;   // Enable trajectory planning
    bool enable_timeline_reasoning = true;    // Enable timeline-based reasoning
};
```

#### Spatial-Temporal Data Management
```cpp
// Insert atom at specific location and time
bool insertAtomAtLocation(const Handle& atom, 
                         const octomap::point3d& location, 
                         const std::chrono::system_clock::time_point& time);

// Get atoms at a location at specific time                         
std::vector<Handle> getAtomsAtLocation(const octomap::point3d& location,
                                      const std::chrono::system_clock::time_point& time);

// Get all locations of an atom at specific time
std::vector<octomap::point3d> getAtomLocations(const Handle& atom,
                                              const std::chrono::system_clock::time_point& time);
```

#### Timeline Queries
```cpp
// Get timeline of when atom appears in the map
std::vector<std::chrono::system_clock::time_point> getAtomTimeline(const Handle& atom);

// Get distance between two atoms at specific time
double getDistanceBetween(const Handle& atom1, const Handle& atom2,
                         const std::chrono::system_clock::time_point& time);

// Get spatial relationships (left/right, above/below, ahead/behind)                         
SpatialRelationResult getSpatialRelations(const Handle& observer,
                                         const Handle& target,
                                         const Handle& reference,
                                         const std::chrono::system_clock::time_point& time);
```

#### Trajectory Planning
```cpp
// Plan trajectory between two points
bool planTrajectory(const Handle& atom,
                   const octomap::point3d& start_location,
                   const octomap::point3d& goal_location,
                   const std::chrono::system_clock::time_point& start_time,
                   const std::chrono::system_clock::time_point& end_time,
                   Trajectory& result_trajectory);

// Find optimal time window for action with spatial requirements                   
TemporalPlanningResult findOptimalTimeWindow(const Handle& action_atom,
                                           const std::vector<SpatialConstraint>& spatial_requirements,
                                           const std::chrono::system_clock::time_point& earliest_start,
                                           const std::chrono::system_clock::time_point& latest_end);
```

### ActionScheduler Extensions

#### Spatial-Temporal Scheduling
```cpp
// Schedule action with spatial constraints
ScheduleResult scheduleActionWithSpatialConstraints(const Handle& action_atom,
                                                   const std::vector<octomap::point3d>& locations,
                                                   const std::chrono::steady_clock::time_point& scheduled_time,
                                                   double spatial_tolerance = 0.1,
                                                   int priority = 5);

// Schedule action with trajectory
ScheduleResult scheduleActionWithTrajectory(const Handle& action_atom,
                                           const std::vector<octomap::point3d>& trajectory_points,
                                           const std::chrono::steady_clock::time_point& start_time,
                                           int priority = 5);

// Schedule with optimal planning                                           
ScheduleResult scheduleActionWithOptimalPlanning(const Handle& action_atom,
                                                const std::vector<octomap::point3d>& preferred_locations,
                                                const std::chrono::steady_clock::time_point& earliest_start,
                                                const std::chrono::steady_clock::time_point& latest_end,
                                                int priority = 5);
```

## Examples

### Spatial Constraint Scheduling
```cpp
// Create robot and goal atoms
Handle robot = atomspace->add_node(CONCEPT_NODE, "Robot");
Handle goal = atomspace->add_node(CONCEPT_NODE, "Goal");

// Define locations
octomap::point3d robot_location(0.0, 0.0, 0.0);
octomap::point3d goal_location(5.0, 5.0, 0.0);

// Insert into spatial-temporal map
integrator->insertAtomAtCurrentTime(robot, robot_location);
integrator->insertAtomAtCurrentTime(goal, goal_location);

// Schedule movement action with spatial constraints
Handle move_action = atomspace->add_node(CONCEPT_NODE, "MoveTo");
std::vector<octomap::point3d> path = {goal_location};
auto schedule_time = std::chrono::steady_clock::now() + std::chrono::seconds(2);

auto result = scheduler.scheduleActionWithSpatialConstraints(
    move_action, path, schedule_time, 0.1, 10);
```

### Trajectory Planning
```cpp
// Plan trajectory from current position to goal
SpaceTimeIntegrator::Trajectory trajectory;
auto start_time = std::chrono::system_clock::now();
auto end_time = start_time + std::chrono::minutes(2);

bool success = integrator->planTrajectory(
    robot, robot_location, goal_location, start_time, end_time, trajectory);

if (success) {
    // Schedule action with planned trajectory
    std::vector<octomap::point3d> waypoints;
    for (const auto& point : trajectory) {
        waypoints.push_back(point.location);
    }
    
    scheduler.scheduleActionWithTrajectory(move_action, waypoints, 
                                         std::chrono::steady_clock::now(), 10);
}
```

### Optimal Time Window Finding
```cpp
// Define preferred locations in order of preference
std::vector<octomap::point3d> preferred_locations = {
    octomap::point3d(4.0, 4.0, 0.0),  // Most preferred
    octomap::point3d(3.0, 4.0, 0.0),  // Second choice
    octomap::point3d(4.0, 3.0, 0.0)   // Third choice
};

// Find optimal time window
auto earliest = std::chrono::steady_clock::now() + std::chrono::minutes(1);
auto latest = earliest + std::chrono::minutes(10);

auto result = scheduler.scheduleActionWithOptimalPlanning(
    move_action, preferred_locations, earliest, latest, 10);
```

## Testing

### Unit Tests
```bash
cd /tmp/opencog-build/agents-build/cpp/agentzero-planning
make test_agentzero_planning
./tests/test_agentzero_planning
```

### Example Program
```bash
cd /tmp/opencog-build/agents-build/cpp/agentzero-planning
./examples/spatial_temporal_example
```

## Performance Characteristics

- **Response Time**: < 100ms for routine spatial-temporal queries
- **Memory Usage**: Linear scaling with spatial resolution and time window
- **Spatial Resolution**: Configurable, default 0.1m (10cm)
- **Time Resolution**: Configurable, default 100ms
- **Scalability**: Supports thousands of tracked atoms across time

## Integration Notes

### OpenCog Compatibility
- Uses standard AtomSpace types for all data representation
- Follows OpenCog architectural patterns and conventions
- Compatible with PLN, URE, and other OpenCog reasoning systems
- Truth values used to represent confidence in spatial-temporal data

### Memory Management
- Uses circular buffer for time data to bound memory usage
- Automatic cleanup of old temporal data
- Efficient spatial indexing using octree structure
- RAII patterns for proper resource cleanup

### Error Handling
- Robust validation of spatial and temporal parameters
- Graceful degradation when spacetime integration is disabled
- Comprehensive error reporting through OpenCog logging system
- Exception safety with proper cleanup on failures

## Known Limitations

1. **Circular Dependencies**: Full SpaceTimeIntegrator integration with ActionScheduler requires careful build ordering
2. **Performance**: Large spatial resolutions or long time windows may impact performance
3. **Memory Usage**: Temporal data accumulates over time and requires periodic cleanup
4. **Thread Safety**: Current implementation assumes single-threaded usage

## Future Enhancements

1. **Multi-Threading**: Add thread-safe spatial-temporal operations
2. **Distributed Planning**: Support for distributed spatial-temporal reasoning
3. **Advanced Algorithms**: Implement A* and other sophisticated planning algorithms
4. **Visualization**: Add tools for visualizing spatial-temporal data
5. **Persistence**: Add support for saving/loading spatial-temporal maps

## Contributing

When contributing to the planning module:

1. Follow OpenCog coding standards and conventions
2. Add comprehensive unit tests for new functionality
3. Update documentation for any API changes
4. Ensure compatibility with existing OpenCog components
5. Test performance impact with realistic data sizes

## License

AGPL-3.0-or-later (same as OpenCog project)