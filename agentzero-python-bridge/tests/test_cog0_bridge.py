#!/usr/bin/env python3
"""Phase 9 — Python interoperability bridge tests (ctypes / Cython)."""

from __future__ import annotations

import os
import sys
import time
import unittest

# Ensure package root is importable when run via ctest
_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)


class TestCog0Bridge(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Import may raise Cog0LibraryNotFound if lib missing
        from cog0 import Agent, version  # noqa: WPS433

        cls.Agent = Agent
        cls.version_fn = staticmethod(version)

    def test_version_string(self):
        v = self.version_fn()
        self.assertIsInstance(v, str)
        self.assertGreater(len(v), 0)

    def test_agent_lifecycle(self):
        agent = self.Agent(name="py-test", cycle_interval_ms=5, max_tasks_per_cycle=5)
        try:
            self.assertFalse(agent.is_running)
            self.assertGreaterEqual(agent.cycle_count, 0)
            self.assertGreaterEqual(agent.atom_count, 0)
        finally:
            agent.close()

    def test_context_manager(self):
        with self.Agent(name="ctx-agent") as agent:
            agent.set_goal("g1", "goal one", 0.8)
            agent.add_concept("Fact:hello")
            self.assertTrue(agent.has_concept("Fact:hello"))

    def test_goal_percept_cycles(self):
        with self.Agent(name="cycle-agent", cycle_interval_ms=1) as agent:
            agent.set_goal("explore", "Explore area", 1.0)
            agent.add_percept("camera", "object-seen", 0.9)
            before = agent.cycle_count
            agent.run_cycles(3)
            self.assertGreaterEqual(agent.cycle_count, before + 3)
            report = agent.status_report()
            self.assertIsInstance(report, str)
            self.assertGreater(len(report), 0)

    def test_background_start_stop(self):
        with self.Agent(name="bg-agent", cycle_interval_ms=5) as agent:
            agent.set_goal("run", "background", 0.5)
            agent.start()
            # Give the loop a brief moment
            deadline = time.time() + 2.0
            while time.time() < deadline and agent.cycle_count < 1:
                time.sleep(0.05)
            agent.stop()
            self.assertFalse(agent.is_running)
            self.assertGreaterEqual(agent.cycle_count, 0)

    def test_add_has_concept(self):
        with self.Agent(name="concept-agent") as agent:
            self.assertFalse(agent.has_concept("Unique:py-bridge-atom"))
            agent.add_concept("Unique:py-bridge-atom")
            self.assertTrue(agent.has_concept("Unique:py-bridge-atom"))
            self.assertGreaterEqual(agent.atom_count, 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
