"""
OpenCog Agent-Zero Python Bridge

This package provides Python bindings to the Agent-Zero C++ components
integrated with OpenCog's cognitive architecture.

Main Components:
- AgentZeroCore: Main orchestration engine
- CognitiveLoop: Perception-action-reflection cycle
- TaskManager: Goal decomposition and task execution
- KnowledgeIntegrator: AtomSpace knowledge operations

Example:
    >>> from opencog.atomspace import AtomSpace
    >>> from opencog.agentzero import AgentZeroCore
    >>> 
    >>> atomspace = AtomSpace()
    >>> agent = AgentZeroCore(atomspace)
    >>> agent.initialize()
    >>> agent.step()
"""

__version__ = '0.1.0'
__author__ = 'OpenCog Community'

# Import Cython bindings
try:
    from .agentzero_core import AgentZeroCore as _AgentZeroCore
    from .cognitive_loop import CognitiveLoop as _CognitiveLoop
    from .task_manager import TaskManager as _TaskManager
    from .knowledge_integrator import KnowledgeIntegrator as _KnowledgeIntegrator
    
    # Re-export for convenience
    AgentZeroCore = _AgentZeroCore
    CognitiveLoop = _CognitiveLoop
    TaskManager = _TaskManager
    KnowledgeIntegrator = _KnowledgeIntegrator
    
except ImportError as e:
    import warnings
    warnings.warn(
        f"Failed to import Cython bindings: {e}\n"
        "Agent-Zero Python bridge may not be fully functional. "
        "Make sure the C++ libraries are built and installed.",
        ImportWarning
    )
    
    # Provide stub classes for documentation and testing
    class AgentZeroCore:
        """Stub class - C++ bindings not available"""
        def __init__(self, atomspace):
            self.atomspace = atomspace
        def initialize(self):
            return True
        def step(self):
            return True
    
    class CognitiveLoop:
        """Stub class - C++ bindings not available"""
        def __init__(self, atomspace):
            self.atomspace = atomspace
        def start(self):
            return True
        def stop(self):
            return True
    
    class TaskManager:
        """Stub class - C++ bindings not available"""
        def __init__(self, atomspace):
            self.atomspace = atomspace
        def execute_next_task(self):
            return False
    
    class KnowledgeIntegrator:
        """Stub class - C++ bindings not available"""
        def __init__(self, atomspace):
            self.atomspace = atomspace
        def query_pattern(self, pattern):
            return []

# Import high-level Python API (always available)
from .exceptions import AgentZeroError, AgentZeroRuntimeError, AgentZeroConfigError
from .utils import create_goal, create_task, query_knowledge

# Try to import agent_zero which depends on Cython modules
try:
    from .agent_zero import AgentZero
except (ImportError, ModuleNotFoundError) as e:
    # AgentZero depends on Cython modules, provide warning
    import warnings
    warnings.warn(
        f"AgentZero high-level API not available: {e}\n"
        "This is expected if C++ libraries are not built yet.",
        ImportWarning
    )
    AgentZero = None

__all__ = [
    'AgentZeroCore',
    'CognitiveLoop', 
    'TaskManager',
    'KnowledgeIntegrator',
    'AgentZero',
    'AgentZeroError',
    'AgentZeroRuntimeError',
    'AgentZeroConfigError',
    'create_goal',
    'create_task',
    'query_knowledge',
    '__version__',
]
