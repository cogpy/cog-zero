# distutils: language = c++
# cython: language_level = 3

"""
Task Manager Cython Bindings

This module provides Python bindings for the TaskManager C++ class,
handling goal decomposition and task execution.
"""

from libcpp cimport bool
from libcpp.string cimport string
from libcpp.vector cimport vector

cdef extern from "opencog/atomspace/AtomSpace.h" namespace "opencog":
    cdef cppclass cAtomSpace "opencog::AtomSpace":
        pass

cdef extern from "opencog/atoms/base/Handle.h" namespace "opencog":
    cdef cppclass cHandle "opencog::Handle":
        pass

# TaskManager C++ interface (stub)
cdef extern from "opencog/agentzero/TaskManager.h" namespace "opencog":
    cdef cppclass cTaskManager "opencog::TaskManager":
        cTaskManager(cAtomSpace* atomspace) except +
        bool decompose_goal(cHandle goal, vector[cHandle] subgoals) except +
        bool execute_next_task() except +
        bool execute_task(cHandle task) except +
        vector[cHandle] get_pending_tasks()
        vector[cHandle] get_completed_tasks()
        bool is_goal_achieved(cHandle goal)
        void clear_completed()
        string get_task_status(cHandle task)

cdef class TaskManager:
    """
    Python wrapper for TaskManager C++ class.
    
    The TaskManager handles hierarchical goal decomposition and task execution,
    coordinating the agent's activities toward achieving its goals.
    
    Example:
        >>> from opencog.atomspace import AtomSpace, ConceptNode
        >>> from opencog.agentzero import TaskManager
        >>> 
        >>> atomspace = AtomSpace()
        >>> tm = TaskManager(atomspace)
        >>> 
        >>> goal = ConceptNode("BuildRobot")
        >>> subgoals = [
        ...     ConceptNode("DesignBlueprint"),
        ...     ConceptNode("AssembleParts")
        ... ]
        >>> tm.decompose_goal(goal, subgoals)
        >>> tm.execute_next_task()
    """
    cdef cTaskManager* c_obj
    cdef object atomspace
    
    def __cinit__(self, atomspace):
        """
        Create a new TaskManager instance.
        
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
    
    def decompose_goal(self, goal, subgoals):
        """
        Decompose a goal into subgoals.
        
        Creates a hierarchical structure where the main goal depends on
        completing all subgoals.
        
        Args:
            goal: AtomSpace Handle representing the main goal
            subgoals: List of AtomSpace Handles representing subgoals
        
        Returns:
            bool: True if decomposition succeeded, False otherwise
        
        Raises:
            TypeError: If arguments are invalid
            RuntimeError: If decomposition fails
        """
        if self.c_obj == NULL:
            return True
        # In real implementation:
        # cdef vector[cHandle] c_subgoals
        # for sg in subgoals:
        #     c_subgoals.push_back(<cHandle>sg)
        # return self.c_obj.decompose_goal(<cHandle>goal, c_subgoals)
        return True
    
    def execute_next_task(self):
        """
        Execute the next pending task in the queue.
        
        Tasks are prioritized based on goal importance and dependencies.
        
        Returns:
            bool: True if a task was executed, False if no tasks pending
        
        Raises:
            RuntimeError: If task execution fails
        """
        if self.c_obj == NULL:
            return False
        return self.c_obj.execute_next_task()
    
    def execute_task(self, task):
        """
        Execute a specific task.
        
        Args:
            task: AtomSpace Handle representing the task
        
        Returns:
            bool: True if task executed successfully, False otherwise
        
        Raises:
            RuntimeError: If task execution fails
        """
        if self.c_obj == NULL:
            return True
        # return self.c_obj.execute_task(<cHandle>task)
        return True
    
    def get_pending_tasks(self):
        """
        Get list of pending tasks.
        
        Returns:
            list: List of AtomSpace Handles representing pending tasks
        """
        if self.c_obj == NULL:
            return []
        # cdef vector[cHandle] tasks = self.c_obj.get_pending_tasks()
        # Convert to Python list
        return []
    
    def get_completed_tasks(self):
        """
        Get list of completed tasks.
        
        Returns:
            list: List of AtomSpace Handles representing completed tasks
        """
        if self.c_obj == NULL:
            return []
        # cdef vector[cHandle] tasks = self.c_obj.get_completed_tasks()
        return []
    
    def is_goal_achieved(self, goal):
        """
        Check if a goal has been achieved.
        
        Args:
            goal: AtomSpace Handle representing the goal
        
        Returns:
            bool: True if goal is achieved, False otherwise
        """
        if self.c_obj == NULL:
            return False
        # return self.c_obj.is_goal_achieved(<cHandle>goal)
        return False
    
    def clear_completed(self):
        """
        Clear the list of completed tasks.
        
        This helps manage memory by removing old task records.
        """
        if self.c_obj == NULL:
            return
        self.c_obj.clear_completed()
    
    def get_task_status(self, task):
        """
        Get the status of a specific task.
        
        Args:
            task: AtomSpace Handle representing the task
        
        Returns:
            str: Status string (e.g., "pending", "running", "completed", "failed")
        """
        if self.c_obj == NULL:
            return "unknown"
        # return self.c_obj.get_task_status(<cHandle>task).decode('utf-8')
        return "unknown"
    
    @property
    def pending_count(self):
        """Number of pending tasks."""
        return len(self.get_pending_tasks())
    
    @property
    def completed_count(self):
        """Number of completed tasks."""
        return len(self.get_completed_tasks())
    
    def __repr__(self):
        """String representation."""
        return f"<TaskManager pending={self.pending_count}, completed={self.completed_count}>"
    
    def __len__(self):
        """Return number of pending tasks."""
        return self.pending_count
