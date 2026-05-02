"""
Agent-Zero Utility Functions

Helper functions for working with Agent-Zero.
"""

try:
    from opencog.atomspace import ConceptNode, PredicateNode, ListLink, ExecutionLink
except ImportError:
    # Stubs when atomspace not available
    class ConceptNode:
        def __init__(self, name):
            self.name = name
    class PredicateNode:
        def __init__(self, name):
            self.name = name
    class ListLink:
        pass
    class ExecutionLink:
        pass


def create_goal(name, description=None, priority=1.0):
    """
    Create a goal atom with metadata.
    
    Args:
        name (str): Goal name
        description (str): Optional description
        priority (float): Priority level (0.0 to 1.0)
    
    Returns:
        ConceptNode: The goal atom
    
    Example:
        >>> goal = create_goal("LearnPython", "Learn Python programming", 0.9)
    """
    goal = ConceptNode(name)
    
    if description:
        # Could add description as metadata
        pass
    
    return goal


def create_task(name, goal=None, dependencies=None):
    """
    Create a task atom linked to a goal.
    
    Args:
        name (str): Task name
        goal: Optional goal this task contributes to
        dependencies: Optional list of prerequisite tasks
    
    Returns:
        ConceptNode: The task atom
    
    Example:
        >>> task = create_task("InstallPython", goal=learn_goal)
    """
    task = ConceptNode(name)
    
    if goal:
        # Could create a link between task and goal
        pass
    
    if dependencies:
        # Could create dependency links
        pass
    
    return task


def query_knowledge(atomspace, concept_name, query_type="instances"):
    """
    Query knowledge about a concept.
    
    Args:
        atomspace: The AtomSpace to query
        concept_name (str): Name of the concept
        query_type (str): Type of query - "instances", "subclasses", or "related"
    
    Returns:
        list: Query results
    
    Example:
        >>> results = query_knowledge(atomspace, "Animal", "subclasses")
    """
    from .knowledge_integrator import KnowledgeIntegrator
    
    integrator = KnowledgeIntegrator(atomspace)
    concept = ConceptNode(concept_name)
    
    if query_type == "instances":
        return integrator.query_instances(concept)
    elif query_type == "subclasses":
        return integrator.query_subclasses(concept)
    else:
        return integrator.query_pattern(concept)


def format_handle_list(handles):
    """
    Format a list of handles for display.
    
    Args:
        handles: List of AtomSpace handles
    
    Returns:
        str: Formatted string
    """
    if not handles:
        return "[]"
    
    items = []
    for h in handles[:10]:  # Limit to first 10
        try:
            items.append(str(h))
        except:
            items.append(repr(h))
    
    result = ", ".join(items)
    if len(handles) > 10:
        result += f", ... ({len(handles) - 10} more)"
    
    return f"[{result}]"


def create_action(name, parameters=None):
    """
    Create an action atom.
    
    Args:
        name (str): Action name
        parameters: Optional action parameters
    
    Returns:
        ConceptNode: The action atom
    """
    action = ConceptNode(name)
    
    if parameters:
        # Could add parameters as values
        pass
    
    return action


def get_agent_statistics(agent):
    """
    Get comprehensive statistics about an agent.
    
    Args:
        agent: AgentZero instance
    
    Returns:
        dict: Statistics dictionary
    """
    stats = {
        'running': agent.is_running,
        'goals': len(agent.goals),
        'pending_tasks': len(agent.pending_tasks),
        'status': agent.status,
    }
    
    # Add loop statistics if available
    try:
        loop_stats = agent.statistics
        stats.update(loop_stats)
    except (AttributeError, TypeError):
        # Statistics not available or agent not initialized properly
        pass
    
    return stats
