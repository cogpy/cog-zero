# distutils: language = c++
# cython: language_level = 3

"""
AgentZero Core Cython Bindings

This module provides Python bindings for the AgentZeroCore C++ class,
enabling Python code to interact with the main Agent-Zero orchestration engine.
"""

from libcpp cimport bool
from libcpp.string cimport string
from libcpp.vector cimport vector

# Import AtomSpace bindings from opencog.atomspace
cdef extern from "opencog/atomspace/AtomSpace.h" namespace "opencog":
    cdef cppclass cAtomSpace "opencog::AtomSpace":
        pass

cdef extern from "opencog/atoms/base/Handle.h" namespace "opencog":
    cdef cppclass cHandle "opencog::Handle":
        pass

# AgentZeroCore C++ interface (stub for now if not built)
cdef extern from "opencog/agentzero/AgentZeroCore.h" namespace "opencog":
    cdef cppclass cAgentZeroCore "opencog::AgentZeroCore":
        cAgentZeroCore(cAtomSpace* atomspace) except +
        bool initialize() except +
        bool is_initialized()
        void shutdown()
        bool step() except +
        bool add_goal(cHandle goal) except +
        bool remove_goal(cHandle goal) except +
        vector[cHandle] get_active_goals()
        string get_status()
        void set_config(string key, string value) except +
        string get_config(string key)

cdef class AgentZeroCore:
    """
    Python wrapper for AgentZeroCore C++ class.
    
    The AgentZeroCore is the main orchestration engine that integrates
    Agent-Zero functionality with OpenCog's cognitive architecture.
    
    Attributes:
        atomspace: The OpenCog AtomSpace instance for knowledge representation
        initialized: Whether the agent core has been initialized
    
    Example:
        >>> from opencog.atomspace import AtomSpace
        >>> from opencog.agentzero import AgentZeroCore
        >>> 
        >>> atomspace = AtomSpace()
        >>> agent = AgentZeroCore(atomspace)
        >>> agent.initialize()
        >>> agent.step()
    """
    cdef cAgentZeroCore* c_obj
    cdef object atomspace
    
    def __cinit__(self, atomspace):
        """
        Create a new AgentZeroCore instance.
        
        Args:
            atomspace: OpenCog AtomSpace instance for knowledge representation
        
        Raises:
            TypeError: If atomspace is not a valid AtomSpace instance
            RuntimeError: If C++ object creation fails
        """
        if atomspace is None:
            raise TypeError("AtomSpace cannot be None")
        
        self.atomspace = atomspace
        # Note: In real implementation, we'd get the C++ pointer from atomspace
        # For now, this is a stub that will work when linked properly
        self.c_obj = NULL
    
    def __dealloc__(self):
        """Clean up C++ resources."""
        if self.c_obj != NULL:
            del self.c_obj
    
    def initialize(self):
        """
        Initialize the Agent-Zero core engine.
        
        This must be called before using other methods. It sets up
        internal data structures and prepares the agent for operation.
        
        Returns:
            bool: True if initialization succeeded, False otherwise
        
        Raises:
            RuntimeError: If initialization fails critically
        """
        if self.c_obj == NULL:
            # Stub implementation when C++ library not available
            return True
        return self.c_obj.initialize()
    
    def is_initialized(self):
        """
        Check if the agent core is initialized.
        
        Returns:
            bool: True if initialized, False otherwise
        """
        if self.c_obj == NULL:
            return True  # Stub always returns True
        return self.c_obj.is_initialized()
    
    def shutdown(self):
        """
        Shutdown the agent core gracefully.
        
        Cleans up resources and stops all ongoing operations.
        After shutdown, initialize() must be called again before reuse.
        """
        if self.c_obj != NULL:
            self.c_obj.shutdown()
    
    def step(self):
        """
        Execute one cognitive cycle step.
        
        This performs one iteration of the perception-action-reflection loop:
        1. Process perceptions
        2. Update knowledge
        3. Perform reasoning
        4. Execute actions
        5. Reflect on results
        
        Returns:
            bool: True if step completed successfully, False otherwise
        
        Raises:
            RuntimeError: If step execution fails
        """
        if self.c_obj == NULL:
            # Stub implementation
            return True
        return self.c_obj.step()
    
    def add_goal(self, goal_handle):
        """
        Add a goal for the agent to pursue.
        
        Args:
            goal_handle: AtomSpace Handle representing the goal
        
        Returns:
            bool: True if goal was added successfully, False otherwise
        
        Raises:
            TypeError: If goal_handle is not a valid Handle
            RuntimeError: If goal addition fails
        """
        if self.c_obj == NULL:
            return True
        # Note: Would need to convert Python Handle to C++ Handle
        # return self.c_obj.add_goal(goal_handle)
        return True
    
    def remove_goal(self, goal_handle):
        """
        Remove a goal from the agent's active goals.
        
        Args:
            goal_handle: AtomSpace Handle representing the goal to remove
        
        Returns:
            bool: True if goal was removed successfully, False otherwise
        """
        if self.c_obj == NULL:
            return True
        # return self.c_obj.remove_goal(goal_handle)
        return True
    
    def get_active_goals(self):
        """
        Get list of currently active goals.
        
        Returns:
            list: List of AtomSpace Handles representing active goals
        """
        if self.c_obj == NULL:
            return []
        # cdef vector[cHandle] goals = self.c_obj.get_active_goals()
        # Convert C++ vector to Python list
        return []
    
    def get_status(self):
        """
        Get current status of the agent.
        
        Returns:
            str: Human-readable status string
        """
        if self.c_obj == NULL:
            return "AgentZeroCore (stub mode - C++ library not linked)"
        return self.c_obj.get_status().decode('utf-8')
    
    def set_config(self, key, value):
        """
        Set a configuration parameter.
        
        Args:
            key (str): Configuration parameter name
            value (str): Configuration parameter value
        
        Raises:
            RuntimeError: If configuration update fails
        """
        if self.c_obj == NULL:
            return
        cdef string c_key = key.encode('utf-8')
        cdef string c_value = value.encode('utf-8')
        self.c_obj.set_config(c_key, c_value)
    
    def get_config(self, key):
        """
        Get a configuration parameter value.
        
        Args:
            key (str): Configuration parameter name
        
        Returns:
            str: Configuration parameter value, or empty string if not set
        """
        if self.c_obj == NULL:
            return ""
        cdef string c_key = key.encode('utf-8')
        return self.c_obj.get_config(c_key).decode('utf-8')
    
    @property
    def status(self):
        """Read-only property for agent status."""
        return self.get_status()
    
    def __repr__(self):
        """String representation of AgentZeroCore."""
        return f"<AgentZeroCore initialized={self.is_initialized()}>"
    
    def __str__(self):
        """Human-readable string representation."""
        return self.get_status()
