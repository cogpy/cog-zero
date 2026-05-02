# Agent-Zero Core Module

This module implements the core orchestration engine for Agent-Zero integrated with OpenCog.

## Overview

The Agent-Zero Core provides the main cognitive loop and orchestration capabilities that integrate deeply with OpenCog's cognitive architecture. It serves as the foundation for all other Agent-Zero components.

## Key Components

- **AgentZeroCore**: Main agent orchestration class
- **CognitiveLoop**: Implements perception-action-reflection cycle
- **TaskManager**: Manages goal decomposition and execution  
- **KnowledgeIntegrator**: Bridges with AtomSpace knowledge representation
- **SelfModification**: Self-modification and meta-programming capabilities (Phase 10) ✅

### Recent Additions

#### ✅ Self-Modification System (AZ-META-001) - COMPLETE
The self-modification system enables Agent-Zero to analyze, evaluate, and safely modify its own code, strategies, and behaviors.

**Key Features**:
- Code analysis and introspection
- Multi-type modification proposals (6 types)
- Four-tier safety framework
- Rollback and checkpoint support
- AtomSpace integration
- Learning from results

**Documentation**: See [docs/SELF_MODIFICATION.md](docs/SELF_MODIFICATION.md) for complete usage guide.

**Status**: Production-ready, pending OpenCog dependency availability in CI.

## Dependencies

- cogutil
- atomspace  
- cogserver

## Build Instructions

This module is built as part of the overall Agent-Zero system. See the root CMakeLists.txt for build configuration.

## Integration Points

### AtomSpace Integration
- All agent state represented as Atoms
- Knowledge structures use standard AtomSpace types
- Goal hierarchies represented as structured Atoms

### CogServer Integration  
- Agent runs as CogServer module
- Exposes agent state through CogServer commands
- Supports real-time monitoring and debugging

## Development Status

🚧 **Under Development** - This is part of the Agent-Zero-Genesis initiative.

See [AGENT-ZERO-GENESIS.md](../../../AGENT-ZERO-GENESIS.md) for the complete development roadmap.