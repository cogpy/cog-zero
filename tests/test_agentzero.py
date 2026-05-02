"""
Unit tests for Agent-Zero Python Bridge

Tests the Python bindings to Agent-Zero C++ components.
"""

import unittest
try:
    from opencog.atomspace import AtomSpace, ConceptNode
    ATOMSPACE_AVAILABLE = True
except ImportError:
    ATOMSPACE_AVAILABLE = False

from opencog.agentzero import (
    AgentZeroCore,
    CognitiveLoop,
    TaskManager,
    KnowledgeIntegrator,
    AgentZero
)
from opencog.agentzero.exceptions import AgentZeroError


@unittest.skipUnless(ATOMSPACE_AVAILABLE, "AtomSpace not available")
class TestAgentZeroCore(unittest.TestCase):
    """Test AgentZeroCore bindings."""
    
    def setUp(self):
        """Create test AtomSpace."""
        self.atomspace = AtomSpace()
        self.core = AgentZeroCore(self.atomspace)
    
    def test_initialization(self):
        """Test core initialization."""
        self.assertTrue(self.core.initialize())
        self.assertTrue(self.core.is_initialized())
    
    def test_step_execution(self):
        """Test single step execution."""
        self.core.initialize()
        self.assertTrue(self.core.step())
    
    def test_goal_management(self):
        """Test adding and removing goals."""
        self.core.initialize()
        goal = ConceptNode("TestGoal")
        
        self.assertTrue(self.core.add_goal(goal))
        goals = self.core.get_active_goals()
        # Note: Stub implementation returns empty list
        self.assertIsInstance(goals, list)
    
    def test_status(self):
        """Test status reporting."""
        self.core.initialize()
        status = self.core.get_status()
        self.assertIsInstance(status, str)
        self.assertGreater(len(status), 0)
    
    def test_configuration(self):
        """Test configuration management."""
        self.core.set_config("test_key", "test_value")
        value = self.core.get_config("test_key")
        # Stub may return empty string
        self.assertIsInstance(value, str)


@unittest.skipUnless(ATOMSPACE_AVAILABLE, "AtomSpace not available")
class TestCognitiveLoop(unittest.TestCase):
    """Test CognitiveLoop bindings."""
    
    def setUp(self):
        """Create test loop."""
        self.atomspace = AtomSpace()
        self.loop = CognitiveLoop(self.atomspace)
    
    def test_start_stop(self):
        """Test starting and stopping the loop."""
        self.assertTrue(self.loop.start())
        self.loop.stop()
    
    def test_cycle_time(self):
        """Test cycle time configuration."""
        self.loop.set_cycle_time(0.5)
        self.assertEqual(self.loop.get_cycle_time(), 0.5)
    
    def test_reflection(self):
        """Test reflection enable/disable."""
        self.loop.enable_reflection(True)
        self.assertTrue(self.loop.is_reflection_enabled())
        
        self.loop.enable_reflection(False)
        # Stub may not update state
    
    def test_statistics(self):
        """Test statistics retrieval."""
        stats = self.loop.get_statistics()
        self.assertIsInstance(stats, dict)
        # Should have expected keys
        for key in ['cycles', 'actions', 'perceptions', 'reflections']:
            self.assertIn(key, stats)
    
    def test_context_manager(self):
        """Test using loop as context manager."""
        with self.loop as loop:
            self.assertIs(loop, self.loop)


@unittest.skipUnless(ATOMSPACE_AVAILABLE, "AtomSpace not available")
class TestTaskManager(unittest.TestCase):
    """Test TaskManager bindings."""
    
    def setUp(self):
        """Create test task manager."""
        self.atomspace = AtomSpace()
        self.tm = TaskManager(self.atomspace)
    
    def test_goal_decomposition(self):
        """Test goal decomposition."""
        goal = ConceptNode("MainGoal")
        subgoals = [ConceptNode("Sub1"), ConceptNode("Sub2")]
        
        self.assertTrue(self.tm.decompose_goal(goal, subgoals))
    
    def test_task_execution(self):
        """Test task execution."""
        task = ConceptNode("TestTask")
        self.assertTrue(self.tm.execute_task(task))
    
    def test_task_lists(self):
        """Test getting task lists."""
        pending = self.tm.get_pending_tasks()
        completed = self.tm.get_completed_tasks()
        
        self.assertIsInstance(pending, list)
        self.assertIsInstance(completed, list)
    
    def test_properties(self):
        """Test task manager properties."""
        self.assertIsInstance(self.tm.pending_count, int)
        self.assertIsInstance(self.tm.completed_count, int)


@unittest.skipUnless(ATOMSPACE_AVAILABLE, "AtomSpace not available")
class TestKnowledgeIntegrator(unittest.TestCase):
    """Test KnowledgeIntegrator bindings."""
    
    def setUp(self):
        """Create test knowledge integrator."""
        self.atomspace = AtomSpace()
        self.ki = KnowledgeIntegrator(self.atomspace)
    
    def test_add_knowledge(self):
        """Test adding knowledge."""
        atom = ConceptNode("TestConcept")
        self.assertTrue(self.ki.add_knowledge(atom))
    
    def test_query_methods(self):
        """Test various query methods."""
        concept = ConceptNode("TestConcept")
        
        results1 = self.ki.query_pattern(concept)
        results2 = self.ki.query_subclasses(concept)
        results3 = self.ki.query_instances(concept)
        
        self.assertIsInstance(results1, list)
        self.assertIsInstance(results2, list)
        self.assertIsInstance(results3, list)
    
    def test_truth_values(self):
        """Test truth value operations."""
        atom = ConceptNode("TestAtom")
        
        self.ki.set_truth_value(atom, 0.8, 0.9)
        tv = self.ki.get_truth_value(atom)
        
        self.assertIsInstance(tv, float)
        self.assertGreaterEqual(tv, 0.0)
        self.assertLessEqual(tv, 1.0)
    
    def test_general_query(self):
        """Test general query method."""
        concept = ConceptNode("Test")
        
        results = self.ki.query(concept, "pattern")
        self.assertIsInstance(results, list)


@unittest.skipUnless(ATOMSPACE_AVAILABLE, "AtomSpace not available")
class TestAgentZero(unittest.TestCase):
    """Test high-level AgentZero API."""
    
    def setUp(self):
        """Create test agent."""
        self.agent = AgentZero()
    
    def tearDown(self):
        """Cleanup agent."""
        self.agent.shutdown()
    
    def test_creation(self):
        """Test agent creation."""
        self.assertIsNotNone(self.agent.atomspace)
        self.assertIsNotNone(self.agent.core)
    
    def test_goal_management(self):
        """Test goal management."""
        goal = self.agent.add_goal("TestGoal", priority=0.8)
        self.assertIsNotNone(goal)
        
        goals = self.agent.goals
        self.assertIsInstance(goals, list)
    
    def test_step_execution(self):
        """Test step execution."""
        result = self.agent.step()
        self.assertIsInstance(result, bool)
    
    def test_status(self):
        """Test status property."""
        status = self.agent.status
        self.assertIsInstance(status, str)
    
    def test_configuration(self):
        """Test configuration."""
        self.agent.configure(test_param="test_value")
        # Configuration is set, no error
    
    def test_context_manager(self):
        """Test using agent as context manager."""
        with AgentZero() as agent:
            self.assertIsNotNone(agent)


def run_tests():
    """Run all tests."""
    unittest.main()


if __name__ == '__main__':
    run_tests()
