"""
cog0 — Python interoperability bridge for the standalone Agent-Zero runtime.

Provides a Pythonic interface over the cog0 C API (see include/cog0/cog0_capi.h).

Backends (tried in order):
  1. Compiled Cython extension ``cog0._cog0`` (if built)
  2. ctypes bindings loading ``libcog0_capi``

Example::

    from cog0 import Agent

    agent = Agent(name="demo", cycle_interval_ms=10)
    agent.set_goal("explore", "Explore the environment", priority=0.9)
    agent.add_percept("camera", "obstacle-ahead", salience=0.8)
    agent.run_cycles(5)
    print(agent.status_report())
    print("atoms:", agent.atom_count)
"""

from __future__ import annotations

__version__ = "0.1.0"
__all__ = [
    "Agent",
    "Cog0Error",
    "Cog0LibraryNotFound",
    "__version__",
    "version",
]

try:
    # Prefer compiled Cython extension when available.
    from ._cog0 import Agent, version  # type: ignore
    from .exceptions import Cog0Error, Cog0LibraryNotFound
except Exception:
    from .exceptions import Cog0Error, Cog0LibraryNotFound
    from .agent import Agent, version
