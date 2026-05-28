# Agent Migration Guide

This guide covers live migration of CogZero agents between cluster nodes.

## Overview

Agent migration enables elastic scaling and fault tolerance by moving running agents between nodes without service interruption. The migration process preserves:

- All goals and their priority/status
- All scheduled tasks
- AtomStore contents (atoms with TruthValues and attention)
- Episodic memory entries
- Reasoning rules and state

## Prerequisites

- CogZero v0.4.0 or later
- ClusterManager initialized on both source and target nodes
- Network connectivity between nodes
- Sufficient memory on target node for agent state

## Migration API

### Agent Serialization

```cpp
#include "cog0/Agent.h"

// Create and configure agent
cog0::Agent agent;
agent.setGoal("task-1", "Process data", 1.0);
agent.addPercept("sensor", "temperature=25.5");
agent.runCycles(10);

// Serialize agent state to JSON
std::string serialized = agent.serialize();

// Deserialize on target node
auto restored = cog0::Agent::deserialize(serialized);
```

### Serialization Format

The agent state is serialized as JSON with the following structure:

```json
{
  "name": "agent-name",
  "version": "0.4.0",
  "timestamp": "2024-01-15T10:30:00Z",
  "atoms": [
    {
      "name": "concept-1",
      "type": "ConceptNode",
      "strength": 0.9,
      "confidence": 0.8,
      "sti": 100,
      "lti": 50
    }
  ],
  "goals": [
    {
      "name": "goal-1",
      "description": "Primary objective",
      "priority": 1.0,
      "achieved": false
    }
  ],
  "tasks": [
    {
      "name": "task-1",
      "description": "Background processing",
      "priority": "NORMAL",
      "status": "PENDING"
    }
  ],
  "episodes": [
    {
      "type": "observation",
      "content": "Sensor reading",
      "importance": 0.7,
      "timestamp": 1705315800
    }
  ],
  "rules": [
    {
      "name": "rule-1",
      "condition": "concept-A",
      "conclusion": "concept-B"
    }
  ]
}
```

### ClusterManager Migration Protocol

```cpp
#include "ClusterManager.h"

// On source node
ClusterManager source(atomspace, "cluster-1");
source.initialize();

// Initiate migration to target node
bool success = source.initiateMigration(
    "node-1",      // source node
    "node-2",      // target node  
    "agent-001"    // agent ID
);

// Check migration status
std::string status = source.getMigrationStatus("agent-001");
// Returns: "pending", "in_progress", "completed", "failed"
```

### On Target Node

```cpp
// On target node - receive migrated state
ClusterManager target(atomspace, "cluster-1");
target.initialize();

// Called automatically by cluster protocol, or manually:
bool received = target.receiveMigration(serializedState, "node-1");
```

## Migration Process

### Step 1: Initiate Migration

1. Source node pauses agent execution
2. Agent state is serialized
3. Migration request sent to target node

### Step 2: Transfer State

1. Serialized state transmitted via cluster protocol
2. Target node validates received data
3. Checksum verification ensures integrity

### Step 3: Restore Agent

1. Target node deserializes agent state
2. AtomStore rebuilt with all atoms
3. TaskManager restored with goals/tasks
4. EpisodicMemory populated

### Step 4: Handoff

1. Target node confirms successful restoration
2. Source node terminates original agent
3. Target node starts agent execution

## Error Handling

### Common Errors

| Error | Cause | Resolution |
|-------|-------|------------|
| `MIGRATION_TARGET_UNREACHABLE` | Network issue | Check connectivity |
| `MIGRATION_INSUFFICIENT_MEMORY` | Target node low on RAM | Free resources or choose different target |
| `MIGRATION_STATE_CORRUPT` | Serialization error | Retry migration |
| `MIGRATION_AGENT_NOT_FOUND` | Invalid agent ID | Verify agent exists |

### Recovery

If migration fails mid-transfer:
1. Source node retains original agent (unchanged)
2. Target node discards partial state
3. Retry migration is safe

## Performance Considerations

- **State Size**: Migration time scales with atom count and episode history
- **Network Bandwidth**: Compressed JSON typically 10-100KB for small agents
- **Downtime**: Agent paused for ~100-500ms during serialization

### Optimization Tips

1. **Prune Old Episodes**: Clear old episodic memory before migration
2. **Consolidate Atoms**: Remove unused atoms to reduce state size
3. **Schedule Off-Peak**: Migrate during low-activity periods

## Testing Migration

```cpp
#include "test_runner.h"

TEST(AgentMigration, RoundTrip) {
    cog0::Agent original;
    original.setGoal("test", "Test goal", 1.0);
    original.atomStore().addNode("test-concept", cog0::AtomType::CONCEPT);
    
    // Serialize
    std::string data = original.serialize();
    EXPECT(!data.empty());
    
    // Deserialize
    auto restored = cog0::Agent::deserialize(data);
    EXPECT(restored != nullptr);
    
    // Verify state
    EXPECT(restored->atomStore().size() > 0);
    EXPECT(restored->taskManager().getActiveGoals().size() == 1);
}
```

## Security Considerations

- **Authentication**: Ensure cluster nodes authenticate before accepting migrations
- **Encryption**: Use TLS for migration traffic in production
- **Validation**: Always validate serialized state before deserializing
- **Access Control**: Restrict migration privileges to authorized nodes

## Related Documentation

- [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md) - Production deployment
- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - Common issues
