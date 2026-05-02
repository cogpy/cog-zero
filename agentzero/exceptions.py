"""
Agent-Zero Exception Classes

Custom exceptions for Agent-Zero Python bridge.
"""


class AgentZeroError(Exception):
    """Base exception for all Agent-Zero errors."""
    pass


class AgentZeroRuntimeError(AgentZeroError):
    """Exception raised for runtime errors in Agent-Zero operations."""
    pass


class AgentZeroConfigError(AgentZeroError):
    """Exception raised for configuration errors."""
    pass


class AgentZeroInitializationError(AgentZeroError):
    """Exception raised when agent initialization fails."""
    pass


class AgentZeroTaskError(AgentZeroError):
    """Exception raised for task execution errors."""
    pass


class AgentZeroKnowledgeError(AgentZeroError):
    """Exception raised for knowledge operations errors."""
    pass
