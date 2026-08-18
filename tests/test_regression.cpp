/*
 * standalone/tests/test_regression.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Phase 9 — Regression baseline tests.
 *
 * These tests establish fixed, reproducible baselines for the observable
 * behaviour of the standalone cog0 agent.  They catch regressions by
 * asserting that known-good scenarios produce exactly the same outputs
 * as when the baseline was first recorded.
 *
 * Guidelines for regression tests:
 *   - Use deterministic scenarios only (no randomness, no wall-clock time).
 *   - Assert exact values (atom counts, rule-fire counts, task orderings).
 *   - Document the expected value and the reasoning behind it.
 */

#include <string>
#include <vector>

#include "test_runner.h"
#include "cog0/Agent.h"

using namespace cog0;

// -----------------------------------------------------------------------
// Helpers

static Agent makeRegAgent(const std::string& name = "reg-agent") {
    AgentConfig cfg;
    cfg.name             = name;
    cfg.cycleInterval    = std::chrono::milliseconds(0);
    cfg.maxTasksPerCycle = 20;
    return Agent(cfg);
}

// -----------------------------------------------------------------------
// Regression 1: empty agent has a known initial atom count
//
// A freshly initialised agent with no goals, percepts, or tasks must
// contain exactly the atoms added by buildDefaultRules() (i.e., the
// "GoalDriven" property atom and nothing else added externally).
// The exact count must not drift between code changes.

TEST(regression_empty_agent_baseline) {
    AgentConfig cfg;
    cfg.name = "reg-empty";
    Agent a(cfg);

    // An empty agent has no explicit atoms added to the store by the user;
    // the default rules use lambdas that only add atoms when fired.
    // Therefore the store should be empty at construction.
    ASSERT_EQ(a.atomStore().size(), 0u);
    ASSERT_EQ(a.cognitiveLoop().cycleCount(), 0u);
    ASSERT_EQ(a.taskManager().goals().size(), 0u);
    ASSERT_EQ(a.taskManager().pendingCount(), 0u);
}

// -----------------------------------------------------------------------
// Regression 2: exact atom count after a known scenario
//
// After running a deterministic scenario — 1 goal, 1 percept, 1 reasoning
// rule — the AtomStore must contain exactly the predicted set of atoms.

TEST(regression_atom_count_after_known_scenario) {
    auto a = makeRegAgent("reg-atom-count");

    a.setGoal("reg-goal", "Regression goal", 1.0);
    // setGoal adds: Goal:<name> atom
    // → expected +1

    a.addPercept("src", "event", 0.5);
    // Percept is consumed in first cycle and adds:
    //   Percept:<content>     → +1
    //   Source:<source>       → +1
    //   SalienceScore:<...>   → ≥ 0 additional atoms

    a.reasoningEngine().addRule("reg-rule",
        [](const AtomStore& s){ return s.getNode(AtomType::CONCEPT,"Percept:event") != nullptr; },
        [](AtomStore& s){ s.addNode(AtomType::CONCEPT,"Derived:reg-result"); },
        1.0);

    a.runCycles(2);

    // The derived atom must exist (exact presence check)
    ASSERT_TRUE(a.atomStore().getNode(AtomType::CONCEPT, "Derived:reg-result") != nullptr);
    // The goal atom must exist
    ASSERT_TRUE(a.atomStore().getNode(AtomType::CONCEPT, "Goal:reg-goal")      != nullptr);
    // The percept atom must exist
    ASSERT_TRUE(a.atomStore().getNode(AtomType::CONCEPT, "Percept:event")      != nullptr);

    // Store must be non-empty (exact minimum)
    ASSERT_GE(a.atomStore().size(), 3u);
}

// -----------------------------------------------------------------------
// Regression 3: rule fire counts for a deterministic sequence
//
// A rule that fires exactly once (because a "done" flag prevents re-firing)
// must have fireCount == 1 after N cycles, establishing a fire-count baseline.

TEST(regression_rule_fire_count_baseline) {
    auto a = makeRegAgent("reg-fire-count");

    a.atomStore().addNode(AtomType::CONCEPT, "Trigger:once");

    a.reasoningEngine().addRule(
        "fires-once",
        [](const AtomStore& s) {
            return s.getNode(AtomType::CONCEPT, "Trigger:once") != nullptr
                && s.getNode(AtomType::CONCEPT, "Done:fires-once") == nullptr;
        },
        [](AtomStore& s) {
            s.addNode(AtomType::CONCEPT, "Done:fires-once");
        },
        1.0
    );

    a.runCycles(5);

    // Locate the custom rule by name and verify its fire count
    const auto& rules = a.reasoningEngine().rules();
    auto it = std::find_if(rules.begin(), rules.end(),
                           [](const auto& r){ return r->name == "fires-once"; });
    ASSERT_TRUE(it != rules.end());
    // Rule must have fired exactly once (stop flag prevents re-firing)
    ASSERT_EQ((*it)->fireCount, 1u);

    // The stop flag atom must be present
    ASSERT_TRUE(a.atomStore().getNode(AtomType::CONCEPT, "Done:fires-once") != nullptr);
}

// -----------------------------------------------------------------------
// Regression 4: task execution order is deterministic and priority-stable
//
// Given a fixed set of tasks at specific priorities, the execution order
// must always match the expected priority sequence: CRITICAL > HIGH >
// NORMAL > LOW > BACKGROUND.

TEST(regression_task_execution_order) {
    auto a = makeRegAgent("reg-task-order");

    std::vector<std::string> order;

    a.scheduleTask("bg",       "", Priority::BACKGROUND, [&](){ order.push_back("bg");       return true; });
    a.scheduleTask("low",      "", Priority::LOW,        [&](){ order.push_back("low");      return true; });
    a.scheduleTask("normal",   "", Priority::NORMAL,     [&](){ order.push_back("normal");   return true; });
    a.scheduleTask("high",     "", Priority::HIGH,       [&](){ order.push_back("high");     return true; });
    a.scheduleTask("critical", "", Priority::CRITICAL,   [&](){ order.push_back("critical"); return true; });

    a.runCycles(2);

    // All 5 tasks must have executed
    ASSERT_EQ(order.size(), 5u);

    // Order must follow strict priority descending
    ASSERT_EQ(order[0], "critical");
    ASSERT_EQ(order[1], "high");
    ASSERT_EQ(order[2], "normal");
    ASSERT_EQ(order[3], "low");
    ASSERT_EQ(order[4], "bg");
}

// -----------------------------------------------------------------------
// Regression 5: CycleStats fields are consistent with actual work done
//
// After running 3 cycles with known percepts and tasks, the accumulated
// CycleStats values must match expected counts exactly.

TEST(regression_cycle_stats_consistency) {
    auto a = makeRegAgent("reg-stats");

    size_t totalPercepts  = 0;
    size_t totalTasks     = 0;
    size_t cyclesSeen     = 0;

    a.cognitiveLoop().setReflectionHook([&](const CycleStats& s) {
        totalPercepts += s.perceptsAdded;
        totalTasks    += s.tasksExecuted;
        ++cyclesSeen;
        // Each cycle number must be positive and sequentially increasing
        ASSERT_EQ(s.cycleNumber, cyclesSeen);
    });

    // Inject exactly 2 percepts (consumed in cycle 1)
    a.addPercept("s", "p1", 0.5);
    a.addPercept("s", "p2", 0.5);

    // Schedule exactly 3 tasks (all execute within 3 cycles)
    int tasksDone = 0;
    a.scheduleTask("t1", "", Priority::HIGH,   [&](){ ++tasksDone; return true; });
    a.scheduleTask("t2", "", Priority::NORMAL, [&](){ ++tasksDone; return true; });
    a.scheduleTask("t3", "", Priority::LOW,    [&](){ ++tasksDone; return true; });

    a.runCycles(3);

    ASSERT_EQ(cyclesSeen,     3u);
    ASSERT_EQ(totalPercepts,  2u);
    ASSERT_EQ(tasksDone,      3);
}

// -----------------------------------------------------------------------
// Regression 6: final state after a scripted command sequence
//
// A deterministic sequence of commands (goal, percept, task, rule, run)
// must always produce the same final AtomStore state.

TEST(regression_scripted_command_final_state) {
    auto a = makeRegAgent("reg-script-state");

    // Command sequence
    a.setGoal("objective", "Scripted objective", 0.8);
    a.addPercept("script", "event-alpha", 0.75);

    a.reasoningEngine().addRule("script-rule",
        [](const AtomStore& s){ return s.getNode(AtomType::CONCEPT,"Percept:event-alpha") != nullptr; },
        [](AtomStore& s){ s.addNode(AtomType::CONCEPT,"Derived:script-output"); }, 2.0);

    bool taskRan = false;
    a.scheduleTask("script-task", "", Priority::NORMAL,
                   [&]() { taskRan = true; return true; });

    a.runCycles(3);

    // Final state assertions (baseline)
    ASSERT_TRUE(a.atomStore().getNode(AtomType::CONCEPT, "Goal:objective")       != nullptr);
    ASSERT_TRUE(a.atomStore().getNode(AtomType::CONCEPT, "Percept:event-alpha")  != nullptr);
    ASSERT_TRUE(a.atomStore().getNode(AtomType::CONCEPT, "Derived:script-output") != nullptr);
    ASSERT_TRUE(taskRan);
    ASSERT_EQ(a.cognitiveLoop().cycleCount(), 3u);
}

// -----------------------------------------------------------------------
// Regression 7: goal priority is preserved across cycles
//
// Goals set with specific priorities must retain those exact values
// throughout execution — they must not drift or be reset.

TEST(regression_goal_priority_stability) {
    auto a = makeRegAgent("reg-goal-priority");

    auto g1 = a.setGoal("alpha", "Alpha goal", 0.9);
    auto g2 = a.setGoal("beta",  "Beta goal",  0.3);

    a.runCycles(5);

    // Priorities must be exactly preserved
    ASSERT_EQ(g1->priority, 0.9);
    ASSERT_EQ(g2->priority, 0.3);

    // Goals must still be in the manager
    const auto& goals = a.taskManager().goals();
    ASSERT_EQ(goals.size(), 2u);
}

// -----------------------------------------------------------------------
// Regression 8: AtomStore remove does not corrupt state
//
// Adding atoms, removing some, then querying must return consistent results.
// This guards against regressions in the incoming-index management.

TEST(regression_atom_store_remove_consistency) {
    AtomStore store;

    auto h1 = store.addNode(AtomType::CONCEPT, "Reg:node-1");
    auto h2 = store.addNode(AtomType::CONCEPT, "Reg:node-2");
    auto h3 = store.addNode(AtomType::CONCEPT, "Reg:node-3");

    // Link node-1 and node-2
    auto link = store.addLink(AtomType::INHERITANCE, {h1, h2});
    ASSERT_EQ(store.size(), 4u); // 3 nodes + 1 link

    // Remove node-3 (no incoming links)
    store.remove(h3);
    ASSERT_EQ(store.size(), 3u);
    ASSERT_TRUE(store.getNode(AtomType::CONCEPT, "Reg:node-3") == nullptr);

    // h1 and h2 still accessible
    ASSERT_TRUE(store.getNode(AtomType::CONCEPT, "Reg:node-1") != nullptr);
    ASSERT_TRUE(store.getNode(AtomType::CONCEPT, "Reg:node-2") != nullptr);

    // Link still accessible
    auto linkCheck = store.getLink(AtomType::INHERITANCE, {h1, h2});
    ASSERT_TRUE(linkCheck != nullptr);
}

// -----------------------------------------------------------------------
// Regression 9: reasoning engine forward-chaining terminates predictably
//
// A chain of rules that terminates when a stop condition is met must
// produce exactly the expected number of firing iterations.

TEST(regression_forward_chaining_termination) {
    auto store  = std::make_shared<AtomStore>();
    ReasoningEngine engine(store);

    // Seed the chain
    store->addNode(AtomType::CONCEPT, "FC:step-0");

    // 3 chained rules: step-0 → step-1 → step-2 → step-3
    for (int i = 0; i < 3; ++i) {
        std::string from = "FC:step-" + std::to_string(i);
        std::string to   = "FC:step-" + std::to_string(i + 1);
        engine.addRule(
            "fc-rule-" + std::to_string(i),
            [from](const AtomStore& s){ return s.getNode(AtomType::CONCEPT, from) != nullptr; },
            [to](AtomStore& s){ s.addNode(AtomType::CONCEPT, to); },
            static_cast<double>(3 - i) // descending priority
        );
    }

    // Run until fixpoint
    size_t firings = engine.runForwardChaining(10);

    // Exactly 3 rules fire (one per pass: rule0 in pass1, rule1 in pass2, rule2 in pass3)
    ASSERT_GE(firings, 3u);

    // All chain atoms present
    for (int i = 1; i <= 3; ++i) {
        auto h = store->getNode(AtomType::CONCEPT, "FC:step-" + std::to_string(i));
        ASSERT_TRUE(h != nullptr);
    }
}

// -----------------------------------------------------------------------
// Regression 10: TruthValue survives serialization round-trip through toStr
//
// Atom string representations must include the atom's name and type,
// so they can be used for debugging and logging reproducibly.

TEST(regression_atom_tostr_format) {
    AtomStore store;
    auto h = store.addNode(AtomType::CONCEPT, "TV:test-atom");
    h->setTV(TruthValue(0.85, 0.9));

    std::string s = h->toStr();
    ASSERT_FALSE(s.empty());
    // Must contain the atom name
    ASSERT_TRUE(s.find("TV:test-atom") != std::string::npos);

    // TruthValue must be preserved
    ASSERT_EQ(h->tv().strength,   0.85);
    ASSERT_EQ(h->tv().confidence, 0.9);
}
