"""
Agent-Zero High-Level Python API

Provides a Pythonic interface to Agent-Zero functionality.
"""

try:
    from opencog.atomspace import AtomSpace, ConceptNode, PredicateNode
    ATOMSPACE_AVAILABLE = True
except ImportError:
    ATOMSPACE_AVAILABLE = False
    # Stub implementations for when atomspace is not available
    class AtomSpace:
        pass
    class ConceptNode:
        def __init__(self, name):
            self.name = name
    class PredicateNode:
        def __init__(self, name):
            self.name = name

from .agentzero_core import AgentZeroCore
from .cognitive_loop import CognitiveLoop
from .task_manager import TaskManager
from .knowledge_integrator import KnowledgeIntegrator
from .exceptions import AgentZeroInitializationError, AgentZeroRuntimeError


class AgentZero:
    """
    High-level Python interface to Agent-Zero.
    
    This class provides a Pythonic wrapper around the C++ Agent-Zero components,
    making it easier to use from Python code.
    
    Example:
        >>> agent = AgentZero()
        >>> agent.add_goal("Learn to code")
        >>> agent.run(cycles=10)
        >>> print(agent.status)
    """
    
    def __init__(self, atomspace=None, auto_start=False):
        """
        Initialize Agent-Zero.
        
        Args:
            atomspace: Optional AtomSpace instance. If None, creates a new one.
            auto_start: If True, automatically starts the cognitive loop.
        
        Raises:
            AgentZeroInitializationError: If initialization fails
        """
        # Create or use provided atomspace
        self.atomspace = atomspace if atomspace else AtomSpace()
        
        # Initialize core components
        try:
            self.core = AgentZeroCore(self.atomspace)
            self.loop = CognitiveLoop(self.atomspace)
            self.task_manager = TaskManager(self.atomspace)
            self.knowledge = KnowledgeIntegrator(self.atomspace)
        except Exception as e:
            raise AgentZeroInitializationError(f"Failed to initialize Agent-Zero: {e}")
        
        # Initialize the core
        if not self.core.initialize():
            raise AgentZeroInitializationError("Core initialization failed")
        
        self._running = False
        
        # Auto-start if requested
        if auto_start:
            self.start()
    
    def start(self):
        """
        Start the cognitive loop.
        
        Raises:
            AgentZeroRuntimeError: If start fails
        """
        if not self.loop.start():
            raise AgentZeroRuntimeError("Failed to start cognitive loop")
        self._running = True
    
    def stop(self):
        """Stop the cognitive loop."""
        self.loop.stop()
        self._running = False
    
    def step(self):
        """
        Execute a single cognitive cycle step.
        
        Returns:
            bool: True if step succeeded, False otherwise
        """
        return self.core.step()
    
    def run(self, cycles=None, duration=None):
        """
        Run the agent for a specified number of cycles or duration.
        
        Args:
            cycles: Number of cycles to run (mutually exclusive with duration)
            duration: Duration in seconds to run (mutually exclusive with cycles)
        
        Raises:
            ValueError: If both cycles and duration are specified
            AgentZeroRuntimeError: If execution fails
        """
        if cycles is not None and duration is not None:
            raise ValueError("Specify either cycles or duration, not both")
        
        if cycles is not None:
            for _ in range(cycles):
                if not self.step():
                    raise AgentZeroRuntimeError("Step execution failed")
        elif duration is not None:
            import time
            start_time = time.time()
            while time.time() - start_time < duration:
                if not self.step():
                    raise AgentZeroRuntimeError("Step execution failed")
                time.sleep(0.01)  # Small delay to prevent busy-waiting
        else:
            # Run indefinitely
            self.start()
    
    def add_goal(self, goal_name, priority=1.0):
        """
        Add a goal for the agent to pursue.
        
        Args:
            goal_name: String name for the goal
            priority: Goal priority (0.0 to 1.0)
        
        Returns:
            Handle to the goal atom
        """
        goal = ConceptNode(goal_name)
        self.core.add_goal(goal)
        
        # Set priority as truth value strength
        self.knowledge.set_truth_value(goal, priority, 0.9)
        
        return goal
    
    def remove_goal(self, goal_name):
        """
        Remove a goal.
        
        Args:
            goal_name: String name of the goal to remove
        
        Returns:
            bool: True if removed, False if not found
        """
        goal = ConceptNode(goal_name)
        return self.core.remove_goal(goal)
    
    def add_knowledge(self, statement):
        """
        Add knowledge to the agent's knowledge base.
        
        Args:
            statement: AtomSpace atom representing knowledge
        
        Returns:
            bool: True if added successfully
        """
        return self.knowledge.add_knowledge(statement)
    
    def query(self, pattern):
        """
        Query the agent's knowledge.
        
        Args:
            pattern: Query pattern
        
        Returns:
            list: Query results
        """
        return self.knowledge.query_pattern(pattern)
    
    @property
    def status(self):
        """Get current agent status."""
        return self.core.get_status()
    
    @property
    def is_running(self):
        """Check if agent is running."""
        return self._running and self.loop.is_running()
    
    @property
    def goals(self):
        """Get active goals."""
        return self.core.get_active_goals()
    
    @property
    def pending_tasks(self):
        """Get pending tasks."""
        return self.task_manager.get_pending_tasks()
    
    @property
    def statistics(self):
        """Get execution statistics."""
        return self.loop.get_statistics()
    
    def configure(self, **kwargs):
        """
        Configure agent parameters.
        
        Args:
            **kwargs: Configuration key-value pairs
        """
        for key, value in kwargs.items():
            self.core.set_config(key, str(value))
    
    def shutdown(self):
        """Shutdown the agent gracefully."""
        self.stop()
        self.core.shutdown()
    
    def __enter__(self):
        """Context manager entry."""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.shutdown()
        return False
    
    def __repr__(self):
        """String representation."""
        status = "running" if self.is_running else "stopped"
        return f"<AgentZero {status}, goals={len(self.goals)}>"
