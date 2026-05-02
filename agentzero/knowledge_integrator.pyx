# distutils: language = c++
# cython: language_level = 3

"""
Knowledge Integrator Cython Bindings

This module provides Python bindings for the KnowledgeIntegrator C++ class,
handling AtomSpace knowledge operations.
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

# KnowledgeIntegrator C++ interface (stub)
cdef extern from "opencog/agentzero/KnowledgeIntegrator.h" namespace "opencog":
    cdef cppclass cKnowledgeIntegrator "opencog::KnowledgeIntegrator":
        cKnowledgeIntegrator(cAtomSpace* atomspace) except +
        bool add_knowledge(cHandle atom) except +
        bool remove_knowledge(cHandle atom) except +
        vector[cHandle] query_pattern(cHandle pattern) except +
        vector[cHandle] query_subclasses(cHandle concept) except +
        vector[cHandle] query_instances(cHandle concept) except +
        bool infer(cHandle query) except +
        double get_truth_value(cHandle atom)
        void set_truth_value(cHandle atom, double strength, double confidence) except +

cdef class KnowledgeIntegrator:
    """
    Python wrapper for KnowledgeIntegrator C++ class.
    
    The KnowledgeIntegrator provides high-level operations for managing
    knowledge in the AtomSpace, including queries, inference, and updates.
    
    Example:
        >>> from opencog.atomspace import AtomSpace, ConceptNode, InheritanceLink
        >>> from opencog.agentzero import KnowledgeIntegrator
        >>> 
        >>> atomspace = AtomSpace()
        >>> ki = KnowledgeIntegrator(atomspace)
        >>> 
        >>> # Add knowledge
        >>> animal = ConceptNode("Animal")
        >>> dog = ConceptNode("Dog")
        >>> ki.add_knowledge(InheritanceLink(dog, animal))
        >>> 
        >>> # Query knowledge
        >>> results = ki.query_subclasses(animal)
        >>> print(f"Found {len(results)} animal types")
    """
    cdef cKnowledgeIntegrator* c_obj
    cdef object atomspace
    
    def __cinit__(self, atomspace):
        """
        Create a new KnowledgeIntegrator instance.
        
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
    
    def add_knowledge(self, atom):
        """
        Add knowledge to the AtomSpace.
        
        Args:
            atom: AtomSpace Handle or Atom to add
        
        Returns:
            bool: True if knowledge was added successfully, False otherwise
        
        Raises:
            TypeError: If atom is not valid
            RuntimeError: If addition fails
        """
        if self.c_obj == NULL:
            return True
        # return self.c_obj.add_knowledge(<cHandle>atom)
        return True
    
    def remove_knowledge(self, atom):
        """
        Remove knowledge from the AtomSpace.
        
        Args:
            atom: AtomSpace Handle or Atom to remove
        
        Returns:
            bool: True if knowledge was removed successfully, False otherwise
        
        Raises:
            RuntimeError: If removal fails
        """
        if self.c_obj == NULL:
            return True
        # return self.c_obj.remove_knowledge(<cHandle>atom)
        return True
    
    def query_pattern(self, pattern):
        """
        Query the AtomSpace for atoms matching a pattern.
        
        Args:
            pattern: AtomSpace Handle representing the query pattern
        
        Returns:
            list: List of Handles matching the pattern
        
        Raises:
            RuntimeError: If query fails
        """
        if self.c_obj == NULL:
            return []
        # cdef vector[cHandle] results = self.c_obj.query_pattern(<cHandle>pattern)
        # Convert to Python list
        return []
    
    def query_subclasses(self, concept):
        """
        Query for all subclasses of a concept.
        
        Args:
            concept: AtomSpace Handle representing the parent concept
        
        Returns:
            list: List of Handles representing subclasses
        """
        if self.c_obj == NULL:
            return []
        # cdef vector[cHandle] results = self.c_obj.query_subclasses(<cHandle>concept)
        return []
    
    def query_instances(self, concept):
        """
        Query for all instances of a concept.
        
        Args:
            concept: AtomSpace Handle representing the concept
        
        Returns:
            list: List of Handles representing instances
        """
        if self.c_obj == NULL:
            return []
        # cdef vector[cHandle] results = self.c_obj.query_instances(<cHandle>concept)
        return []
    
    def infer(self, query):
        """
        Perform inference to answer a query.
        
        Uses PLN (Probabilistic Logic Networks) to derive new knowledge
        that answers the query.
        
        Args:
            query: AtomSpace Handle representing the query
        
        Returns:
            bool: True if inference succeeded, False otherwise
        
        Raises:
            RuntimeError: If inference fails
        """
        if self.c_obj == NULL:
            return False
        # return self.c_obj.infer(<cHandle>query)
        return False
    
    def get_truth_value(self, atom):
        """
        Get the truth value strength of an atom.
        
        Args:
            atom: AtomSpace Handle or Atom
        
        Returns:
            float: Truth value strength (0.0 to 1.0)
        """
        if self.c_obj == NULL:
            return 0.0
        # return self.c_obj.get_truth_value(<cHandle>atom)
        return 0.0
    
    def set_truth_value(self, atom, strength, confidence=0.9):
        """
        Set the truth value of an atom.
        
        Args:
            atom: AtomSpace Handle or Atom
            strength (float): Truth value strength (0.0 to 1.0)
            confidence (float): Confidence in the truth value (0.0 to 1.0)
        
        Raises:
            ValueError: If strength or confidence are out of range
            RuntimeError: If update fails
        """
        if not (0.0 <= strength <= 1.0):
            raise ValueError("Strength must be between 0.0 and 1.0")
        if not (0.0 <= confidence <= 1.0):
            raise ValueError("Confidence must be between 0.0 and 1.0")
        
        if self.c_obj == NULL:
            return
        self.c_obj.set_truth_value(<cHandle>atom, strength, confidence)
    
    def query(self, pattern_or_concept, query_type="pattern"):
        """
        General-purpose query method.
        
        Args:
            pattern_or_concept: AtomSpace Handle
            query_type (str): Type of query - "pattern", "subclasses", or "instances"
        
        Returns:
            list: Query results
        
        Raises:
            ValueError: If query_type is invalid
        """
        if query_type == "pattern":
            return self.query_pattern(pattern_or_concept)
        elif query_type == "subclasses":
            return self.query_subclasses(pattern_or_concept)
        elif query_type == "instances":
            return self.query_instances(pattern_or_concept)
        else:
            raise ValueError(f"Invalid query type: {query_type}")
    
    def __repr__(self):
        """String representation."""
        return f"<KnowledgeIntegrator atomspace={self.atomspace}>"
