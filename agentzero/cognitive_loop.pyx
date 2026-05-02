# distutils: language = c++
# cython: language_level = 3

"""
Cognitive Loop Cython Bindings

This module provides Python bindings for the CognitiveLoop C++ class,
implementing the perception-action-reflection cycle.
"""

from libcpp cimport bool
from libcpp.string cimport string
from libcpp.map cimport map

cdef extern from "opencog/atomspace/AtomSpace.h" namespace "opencog":
    cdef cppclass cAtomSpace "opencog::AtomSpace":
        pass

# CognitiveLoop C++ interface (stub)
cdef extern from "opencog/agentzero/CognitiveLoop.h" namespace "opencog":
    cdef cppclass cCognitiveLoop "opencog::CognitiveLoop":
        cCognitiveLoop(cAtomSpace* atomspace) except +
        bool start() except +
        bool stop() except +
        bool is_running()
        void step() except +
        void set_cycle_time(double seconds) except +
        double get_cycle_time()
        void enable_reflection(bool enable)
        bool is_reflection_enabled()
        map[string, long] get_statistics()

cdef class CognitiveLoop:
    """
    Python wrapper for CognitiveLoop C++ class.
    
    The CognitiveLoop implements the continuous perception-action-reflection
    cycle that drives agent behavior.
    
    Example:
        >>> from opencog.atomspace import AtomSpace
        >>> from opencog.agentzero import CognitiveLoop
        >>> 
        >>> atomspace = AtomSpace()
        >>> loop = CognitiveLoop(atomspace)
        >>> loop.set_cycle_time(0.1)  # 100ms per cycle
        >>> loop.start()
        >>> # ... agent operates autonomously ...
        >>> loop.stop()
    """
    cdef cCognitiveLoop* c_obj
    cdef object atomspace
    
    def __cinit__(self, atomspace):
        """
        Create a new CognitiveLoop instance.
        
        Args:
            atomspace: OpenCog AtomSpace instance
        
        Raises:
            TypeError: If atomspace is not valid
        """
        if atomspace is None:
            raise TypeError("AtomSpace cannot be None")
        
        self.atomspace = atomspace
        self.c_obj = NULL
    
    def __dealloc__(self):
        """Clean up C++ resources."""
        if self.c_obj != NULL:
            del self.c_obj
    
    def start(self):
        """
        Start the cognitive loop.
        
        The loop will run continuously in the background, executing
        perception-action-reflection cycles at the configured rate.
        
        Returns:
            bool: True if started successfully, False otherwise
        
        Raises:
            RuntimeError: If start fails
        """
        if self.c_obj == NULL:
            return True
        return self.c_obj.start()
    
    def stop(self):
        """
        Stop the cognitive loop.
        
        Gracefully stops the loop after completing the current cycle.
        
        Returns:
            bool: True if stopped successfully, False otherwise
        """
        if self.c_obj == NULL:
            return True
        return self.c_obj.stop()
    
    def is_running(self):
        """
        Check if the cognitive loop is currently running.
        
        Returns:
            bool: True if running, False otherwise
        """
        if self.c_obj == NULL:
            return False
        return self.c_obj.is_running()
    
    def step(self):
        """
        Execute a single cognitive cycle step manually.
        
        This is useful for debugging or when you want precise control
        over execution timing.
        
        Raises:
            RuntimeError: If step execution fails
        """
        if self.c_obj == NULL:
            return
        self.c_obj.step()
    
    def set_cycle_time(self, seconds):
        """
        Set the target time for each cognitive cycle.
        
        Args:
            seconds (float): Target cycle time in seconds
        
        Raises:
            ValueError: If seconds is not positive
            RuntimeError: If update fails
        """
        if seconds <= 0:
            raise ValueError("Cycle time must be positive")
        
        if self.c_obj == NULL:
            return
        self.c_obj.set_cycle_time(seconds)
    
    def get_cycle_time(self):
        """
        Get the current cycle time setting.
        
        Returns:
            float: Cycle time in seconds
        """
        if self.c_obj == NULL:
            return 0.1  # Default stub value
        return self.c_obj.get_cycle_time()
    
    def enable_reflection(self, enable=True):
        """
        Enable or disable the reflection phase of the cognitive loop.
        
        Args:
            enable (bool): True to enable reflection, False to disable
        """
        if self.c_obj == NULL:
            return
        self.c_obj.enable_reflection(enable)
    
    def is_reflection_enabled(self):
        """
        Check if reflection is enabled.
        
        Returns:
            bool: True if reflection is enabled, False otherwise
        """
        if self.c_obj == NULL:
            return True  # Default stub value
        return self.c_obj.is_reflection_enabled()
    
    def get_statistics(self):
        """
        Get execution statistics for the cognitive loop.
        
        Returns:
            dict: Dictionary with statistics like cycle count, action count, etc.
        """
        if self.c_obj == NULL:
            return {
                'cycles': 0,
                'actions': 0,
                'perceptions': 0,
                'reflections': 0
            }
        
        # Get statistics from C++ and convert
        cdef map[string, long] stats = self.c_obj.get_statistics()
        result = {}
        for item in stats:
            result[item.first.decode('utf-8')] = item.second
        return result
    
    @property
    def cycle_time(self):
        """Property for getting/setting cycle time."""
        return self.get_cycle_time()
    
    @cycle_time.setter
    def cycle_time(self, value):
        self.set_cycle_time(value)
    
    @property
    def running(self):
        """Read-only property for running state."""
        return self.is_running()
    
    def __repr__(self):
        """String representation."""
        running_str = "running" if self.is_running() else "stopped"
        return f"<CognitiveLoop {running_str}, cycle_time={self.get_cycle_time():.3f}s>"
    
    def __enter__(self):
        """Context manager entry - starts the loop."""
        self.start()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit - stops the loop."""
        self.stop()
        return False
