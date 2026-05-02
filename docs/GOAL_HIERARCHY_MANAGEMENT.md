# GoalHierarchy Management in TaskManager

## Overview

The TaskManager class provides comprehensive GoalHierarchy management capabilities as part of the AGENT-ZERO-GENESIS project (AZ-PLAN-001). This system allows for hierarchical goal representation, tracking, and achievement calculation using OpenCog's AtomSpace for knowledge representation.

## Key Features

### 1. Hierarchical Goal Representation
Goals are represented as CONCEPT_NODE atoms in AtomSpace, with hierarchical relationships expressed through INHERITANCE_LINK atoms:
- `INHERITANCE_LINK(subgoal, parent_goal)` represents the relationship that `subgoal` is part of `parent_goal`
- Multiple levels of hierarchy are supported (unlimited depth with circular reference protection)
- Each goal has associated metadata stored as EVALUATION_LINK atoms

### 2. Goal Navigation Methods

#### `getSubgoals(const Handle& parent_goal)`
Returns all direct subgoals of a given parent goal.
```cpp
std::vector<Handle> subgoals = task_mgr.getSubgoals(root_goal);
```

#### `getParentGoal(const Handle& subgoal)`
Returns the immediate parent goal of a subgoal, or Handle::UNDEFINED if it's a top-level goal.
```cpp
Handle parent = task_mgr.getParentGoal(leaf_goal);
```

#### `getGoalAncestors(const Handle& goal)`
Returns all ancestor goals from immediate parent to root, in order.
```cpp
std::vector<Handle> ancestors = task_mgr.getGoalAncestors(deep_goal);
```

#### `getLeafGoals(const Handle& root_goal)`
Finds all leaf goals (goals with no subgoals) in the hierarchy starting from root_goal.
```cpp
std::vector<Handle> leaves = task_mgr.getLeafGoals(project_goal);
```

#### `getGoalHierarchyDepth(const Handle& goal)`
Calculates the maximum depth of the goal hierarchy starting from the given goal.
```cpp
int depth = task_mgr.getGoalHierarchyDepth(root_goal);
```

### 3. Hierarchical Goal Achievement

#### `calculateHierarchicalGoalAchievement(const Handle& goal)`
Calculates goal achievement considering both the goal's own progress and the weighted achievement of all subgoals.

**Algorithm:**
1. For leaf goals: Returns the goal's own TruthValue
2. For parent goals: Combines own achievement (30%) with weighted average of subgoal achievements (70%)
3. Confidence is calculated as the minimum of goal confidence and subgoal confidences
4. Results are stored back to the goal's TruthValue

```cpp
TruthValuePtr achievement = task_mgr.calculateHierarchicalGoalAchievement(project_goal);
double achievement_level = achievement->get_mean();
double confidence = achievement->get_confidence();
```

### 4. Priority Propagation

#### `propagateGoalPriority(const Handle& goal, Priority priority)`
Propagates priority from a parent goal to all its subgoals, with slight reduction at each level.

**Features:**
- Priority is stored as EVALUATION_LINK metadata in AtomSpace
- Each level receives priority - 1 (minimum priority = 1)
- Recursive propagation to all descendant goals
- TruthValue strength reflects priority level (priority/20.0)

```cpp
bool propagated = task_mgr.propagateGoalPriority(urgent_goal, TaskManager::Priority::CRITICAL);
```

### 5. Hierarchy Synchronization

#### `synchronizeGoalHierarchy(const Handle& goal)`
Synchronizes goal statuses across the hierarchy, updating parent and child goals based on completion states.

**Features:**
- Highly achieved goals (>0.8) trigger parent synchronization
- Recursively synchronizes all subgoals
- Adds synchronization timestamps as metadata
- Ensures hierarchy consistency

```cpp
bool synchronized = task_mgr.synchronizeGoalHierarchy(completed_milestone);
```

### 6. Goal Removal and Cleanup

#### `removeGoalFromHierarchy(const Handle& goal, bool preserve_orphans)`
Removes a goal and manages orphaned subgoals according to the preserve_orphans flag.

**Options:**
- `preserve_orphans = true`: Reconnects orphaned subgoals to the removed goal's parent
- `preserve_orphans = false`: Recursively removes all subgoals
- Cleans up all associated metadata and relationships
- Updates current goal reference if necessary

```cpp
bool removed = task_mgr.removeGoalFromHierarchy(cancelled_goal, false);
```

## AtomSpace Integration

### Goal Representation
```
CONCEPT_NODE "Goal_project_completion"
  TruthValue: <achievement_level, confidence>
```

### Hierarchy Relationships
```
INHERITANCE_LINK
  CONCEPT_NODE "Goal_subgoal_1"
  CONCEPT_NODE "Goal_parent_goal"
```

### Priority Metadata
```
EVALUATION_LINK
  PREDICATE_NODE "goal_priority"
  CONCEPT_NODE "Goal_important_task"
  NUMBER_NODE "15"
```

### Synchronization Metadata
```
EVALUATION_LINK
  PREDICATE_NODE "hierarchy_synchronized"
  CONCEPT_NODE "Goal_synced_goal"
  NUMBER_NODE "1634567890"  // timestamp
```

## Usage Examples

### Creating a Goal Hierarchy
```cpp
// Create main project goal
Handle project = task_mgr.setGoal("software_project", false);

// Add major milestones
Handle milestone1 = task_mgr.addSubgoal(project, "design_phase");
Handle milestone2 = task_mgr.addSubgoal(project, "implementation_phase");
Handle milestone3 = task_mgr.addSubgoal(project, "testing_phase");

// Add specific tasks
Handle ui_design = task_mgr.addSubgoal(milestone1, "ui_design");
Handle db_design = task_mgr.addSubgoal(milestone1, "database_design");
Handle coding = task_mgr.addSubgoal(milestone2, "core_development");
```

### Managing Goal Achievement
```cpp
// Set individual task achievements
ui_design->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9)); // Complete
db_design->setTruthValue(SimpleTruthValue::createTV(0.8, 0.9)); // Mostly done
coding->setTruthValue(SimpleTruthValue::createTV(0.3, 0.9));    // In progress

// Calculate hierarchical achievements
TruthValuePtr milestone1_achievement = task_mgr.calculateHierarchicalGoalAchievement(milestone1);
TruthValuePtr project_achievement = task_mgr.calculateHierarchicalGoalAchievement(project);

std::cout << "Milestone 1 progress: " << milestone1_achievement->get_mean() << std::endl;
std::cout << "Project progress: " << project_achievement->get_mean() << std::endl;
```

### Priority Management
```cpp
// Set high priority for critical path
task_mgr.propagateGoalPriority(milestone2, TaskManager::Priority::HIGH);

// Synchronize status after major completion
ui_design->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));
task_mgr.synchronizeGoalHierarchy(milestone1);
```

### Hierarchy Analysis
```cpp
// Find all leaf goals that need work
std::vector<Handle> active_tasks = task_mgr.getLeafGoals(project);

// Check project complexity
int project_depth = task_mgr.getGoalHierarchyDepth(project);

// Get status overview
std::string hierarchy_info = task_mgr.getGoalHierarchyInfo(project);
```

## Performance Considerations

- **Depth Limits**: Hierarchy traversal includes circular reference protection (max 50 levels)
- **Caching**: Goal relationships are dynamically queried from AtomSpace (no caching)
- **Memory**: Large hierarchies may require significant AtomSpace memory
- **Complexity**: Most operations are O(n) where n is the number of goals in relevant subtree

## Error Handling

All methods include comprehensive error handling:
- **Invalid Handles**: Methods gracefully handle Handle::UNDEFINED inputs
- **Missing Relationships**: Missing hierarchy links are handled without crashes
- **AtomSpace Errors**: Exceptions are caught and logged appropriately
- **Circular References**: Protection against infinite loops in hierarchy traversal

## Integration with OpenCog Components

The GoalHierarchy management system integrates with:
- **AtomSpace**: Primary storage and query mechanism
- **TruthValue System**: Goal achievement and confidence tracking
- **Logger**: Comprehensive logging of operations and errors
- **CogServer**: Can be queried via CogServer if available
- **PLN/URE**: Goal relationships can be used for logical reasoning

## Future Enhancements

Planned improvements include:
- Goal dependency tracking beyond hierarchy (prerequisite relationships)
- Automatic goal decomposition using PLN reasoning
- Goal conflict detection and resolution
- Performance optimization with hierarchy caching
- Integration with attention mechanism for goal prioritization
- Temporal goal tracking with deadline management