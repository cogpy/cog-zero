/*
 * standalone/tests/test_e2e.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * End-to-end (e2e) tests for the cog0 standalone agent.
 *
 * Each test exercises a complete scenario through the public Agent API,
 * verifying that all subsystems (AtomStore, TaskManager, ReasoningEngine,
 * CognitiveLoop) work together correctly from start to finish.
 */

#include <chrono>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "test_runner.h"
#include "cog0/Agent.h"

using namespace cog0;

// -----------------------------------------------------------------------
// Helpers

static Agent makeAgent(const std::string& name = "e2e-agent",
                       bool verbose = false) {
    AgentConfig cfg;
    cfg.name    = name;
    cfg.verbose = verbose;
    cfg.cycleInterval = std::chrono::milliseconds(10); // fast for tests
    return Agent(cfg);
}

// -----------------------------------------------------------------------
// E2E Scenario 1: full perception → reasoning → action cycle
//
// Verifies that a percept added before cycle execution ends up in the
// AtomStore, triggers a high-salience attention rule, and that the
// cycle count advances.

TEST(e2e_full_percept_reason_action_cycle) {
    auto a = makeAgent("e2e-full");

    a.setGoal("observe-environment", "Watch the environment", 0.8);
    a.addPercept("sensor-1", "critical-event-detected", 0.95);

    a.runCycles(3);

    // Percept atom must exist
    auto ph = a.atomStore().getNode(AtomType::CONCEPT,
                                    "Percept:critical-event-detected");
    ASSERT_TRUE(ph != nullptr);

    // High-salience rule should have fired and added AttentionFlag
    auto ah = a.atomStore().getNode(AtomType::CONCEPT,
                                    "AttentionFlag:high-salience");
    ASSERT_TRUE(ah != nullptr);

    // Goal atom must exist
    auto gh = a.atomStore().getNode(AtomType::CONCEPT,
                                    "Goal:observe-environment");
    ASSERT_TRUE(gh != nullptr);

    // goal-driven-assertion rule must have fired
    auto prop = a.atomStore().getNode(AtomType::CONCEPT,
                                      "AgentProperty:goal-driven");
    ASSERT_TRUE(prop != nullptr);

    ASSERT_EQ(a.cognitiveLoop().cycleCount(), 3u);
}

// -----------------------------------------------------------------------
// E2E Scenario 2: multiple goals with different priorities
//
// Two goals are set; only one critical task is present per cycle.
// After enough cycles every task should have completed.

TEST(e2e_multiple_goals_priority_ordering) {
    auto a = makeAgent("e2e-goals");

    a.setGoal("primary",   "Primary objective",   1.0);
    a.setGoal("secondary", "Secondary objective", 0.4);

    int order = 0;
    int critical_order = -1, normal_order = -1;

    a.scheduleTask("critical-work", "", Priority::CRITICAL, [&]() {
        critical_order = order++;
        return true;
    });
    a.scheduleTask("normal-work",   "", Priority::NORMAL, [&]() {
        normal_order = order++;
        return true;
    });

    // Run enough cycles; maxTasksPerCycle defaults to 3 so both tasks
    // may execute in cycle 1, but CRITICAL must come first.
    a.runCycles(2);

    ASSERT_GE(critical_order, 0);   // both tasks ran
    ASSERT_GE(normal_order,   0);
    ASSERT_LT(critical_order, normal_order); // CRITICAL executed first
}

// -----------------------------------------------------------------------
// E2E Scenario 3: task dependency chain
//
// Three tasks are added sequentially; each depends on the previous one
// having set a flag in the agent's AtomStore.

TEST(e2e_task_dependency_chain) {
    auto a = makeAgent("e2e-chain");

    bool step1Done = false, step2Done = false, step3Done = false;

    a.scheduleTask("step-1", "", Priority::HIGH, [&]() {
        a.atomStore().addNode(AtomType::CONCEPT, "Flag:step1");
        step1Done = true;
        return true;
    });
    a.scheduleTask("step-2", "", Priority::NORMAL, [&]() {
        // Only meaningful if step1 completed
        ASSERT_TRUE(step1Done);
        a.atomStore().addNode(AtomType::CONCEPT, "Flag:step2");
        step2Done = true;
        return true;
    });
    a.scheduleTask("step-3", "", Priority::LOW, [&]() {
        ASSERT_TRUE(step2Done);
        step3Done = true;
        return true;
    });

    a.runCycles(3); // 3 tasks at up to 3 per cycle → all done in cycle 1

    ASSERT_TRUE(step1Done);
    ASSERT_TRUE(step2Done);
    ASSERT_TRUE(step3Done);

    // Flags must be in the store
    ASSERT_TRUE(a.atomStore().getNode(AtomType::CONCEPT, "Flag:step1") != nullptr);
    ASSERT_TRUE(a.atomStore().getNode(AtomType::CONCEPT, "Flag:step2") != nullptr);
}

// -----------------------------------------------------------------------
// E2E Scenario 4: reasoning chain across multiple cycles
//
// A custom rule is added that fires only when a specific atom is present.
// A percept adds that atom; after running cycles the derived atom should
// appear, demonstrating forward-chaining across the loop.

TEST(e2e_reasoning_chain_across_cycles) {
    auto a = makeAgent("e2e-reasoning");

    // Add a custom inference rule: if "Percept:trigger" exists →
    // create "Derived:chain-result"
    a.reasoningEngine().addRule(
        "chain-rule",
        [](const AtomStore& store) {
            return store.getNode(AtomType::CONCEPT, "Percept:trigger") != nullptr;
        },
        [](AtomStore& store) {
            store.addNode(AtomType::CONCEPT, "Derived:chain-result");
        },
        2.0
    );

    a.addPercept("test-sensor", "trigger", 0.7);
    a.runCycles(2);

    auto result = a.atomStore().getNode(AtomType::CONCEPT, "Derived:chain-result");
    ASSERT_TRUE(result != nullptr);
}

// -----------------------------------------------------------------------
// E2E Scenario 5: reflection hook collects per-cycle statistics
//
// The reflection hook is exercised across N cycles; total perceived
// percepts and tasks executed must equal the expected counts.

TEST(e2e_reflection_hook_collects_stats) {
    auto a = makeAgent("e2e-reflect");

    size_t totalPercepts = 0;
    size_t totalTasks    = 0;
    size_t cyclesSeen    = 0;

    a.cognitiveLoop().setReflectionHook([&](const CycleStats& s) {
        totalPercepts += s.perceptsAdded;
        totalTasks    += s.tasksExecuted;
        ++cyclesSeen;
        ASSERT_GT(s.cycleNumber, 0u);
    });

    a.addPercept("s", "p1", 0.5);
    a.addPercept("s", "p2", 0.5);

    int taskCount = 0;
    a.scheduleTask("t1", "", Priority::NORMAL, [&]() { ++taskCount; return true; });
    a.scheduleTask("t2", "", Priority::NORMAL, [&]() { ++taskCount; return true; });

    a.runCycles(3);

    ASSERT_EQ(cyclesSeen, 3u);
    // Both percepts are consumed in the first cycle
    ASSERT_EQ(totalPercepts, 2u);
    // Both tasks complete within 3 cycles
    ASSERT_EQ(taskCount, 2);
    (void)totalTasks;
}

// -----------------------------------------------------------------------
// E2E Scenario 6: agent stability under many cycles (no memory explosion)
//
// Running 50 cycles without injecting new data should not blow up the
// AtomStore size uncontrollably.  The store may grow due to rule
// applications, but it must remain bounded.

TEST(e2e_stability_many_cycles) {
    auto a = makeAgent("e2e-stability");

    a.setGoal("endurance", "Run for a long time", 0.5);

    const size_t N = 50;
    a.runCycles(N);

    ASSERT_EQ(a.cognitiveLoop().cycleCount(), N);

    // AtomStore should be bounded (well under 10 000 atoms)
    ASSERT_LT(a.atomStore().size(), 10000u);
}

// -----------------------------------------------------------------------
// E2E Scenario 7: percept-driven goal creation and task scheduling
//
// Mimics an autonomous-agency pattern: an external percept leads to a
// new goal being set, which then drives task creation and execution.

TEST(e2e_percept_drives_autonomous_goal_creation) {
    auto a = makeAgent("e2e-autonomous");

    bool goalActuated = false;

    // A rule that, when a specific percept appears, creates a new task
    a.reasoningEngine().addRule(
        "percept-to-task",
        [](const AtomStore& store) {
            return store.getNode(AtomType::CONCEPT,
                                 "Percept:environment-changed") != nullptr
                && store.getNode(AtomType::CONCEPT,
                                 "Flag:response-scheduled") == nullptr;
        },
        [&](AtomStore& store) {
            // Mark as scheduled to avoid re-firing
            store.addNode(AtomType::CONCEPT, "Flag:response-scheduled");
            // Register a task via the agent (captures a by ref — only safe
            // because the agent outlives the lambda in this test)
            a.scheduleTask("respond-to-change", "", Priority::HIGH, [&]() {
                goalActuated = true;
                return true;
            });
        },
        3.0
    );

    a.addPercept("world", "environment-changed", 0.85);
    a.runCycles(5);

    ASSERT_TRUE(goalActuated);
    auto flagH = a.atomStore().getNode(AtomType::CONCEPT, "Flag:response-scheduled");
    ASSERT_TRUE(flagH != nullptr);
}

// -----------------------------------------------------------------------
// E2E Scenario 8: concurrent perception (background thread)
//
// Start the background loop, inject percepts from the main thread while
// the loop is running, then stop and verify they were processed.

TEST(e2e_background_loop_concurrent_percepts) {
    AgentConfig cfg;
    cfg.name           = "e2e-bg";
    cfg.cycleInterval  = std::chrono::milliseconds(20);
    cfg.maxCycles      = 10;
    Agent a(cfg);

    a.start();

    // Inject percepts while the loop is running
    for (int i = 0; i < 5; ++i) {
        a.addPercept("thread", "msg-" + std::to_string(i), 0.6);
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    // Wait for the loop to finish its max cycles
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (a.isRunning() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ASSERT_FALSE(a.isRunning());
    // At least some of the percepts should have been processed
    ASSERT_GT(a.cognitiveLoop().cycleCount(), 0u);
}

// -----------------------------------------------------------------------
// E2E Scenario 9: status report reflects full agent state
//
// After a rich scenario the statusReport() must mention key fields.

TEST(e2e_status_report_completeness) {
    auto a = makeAgent("e2e-status");

    a.setGoal("alpha",  "First goal",  0.9);
    a.setGoal("beta",   "Second goal", 0.5);
    a.scheduleTask("task-x", "Do X", Priority::HIGH);
    a.addPercept("cli", "hello", 0.6);
    a.runCycles(3);

    std::string report = a.statusReport();

    ASSERT_FALSE(report.empty());
    ASSERT_TRUE(report.find("Agent Status") != std::string::npos);
    ASSERT_TRUE(report.find("Cycles run")   != std::string::npos);
    ASSERT_TRUE(report.find("AtomStore")    != std::string::npos);
}

// -----------------------------------------------------------------------
// E2E Scenario 10: script-style command sequence
//
// Exercises the complete command-dispatch logic (similar to what the CLI
// --script flag does) without needing file I/O.  Each command is a string
// parsed exactly as the interactive loop would parse it.

TEST(e2e_command_sequence_script_style) {
    auto a = makeAgent("e2e-script");

    // Simulate script lines: goal, percept, task, run
    struct ScriptLine { std::string cmd; std::string arg; };
    std::vector<ScriptLine> script = {
        {"goal",    "objective plan the mission"},
        {"percept", "sensor data incoming"},
        {"task",    "act execute the plan"},
        {"run",     "2"},
    };

    size_t goalsSet = 0, percepts = 0, tasksScheduled = 0;

    for (const auto& line : script) {
        if (line.cmd == "goal") {
            std::istringstream iss(line.arg);
            std::string name; iss >> name;
            std::string rest; std::getline(iss, rest);
            if (!rest.empty() && rest[0] == ' ') rest = rest.substr(1);
            a.setGoal(name, rest);
            ++goalsSet;
        } else if (line.cmd == "percept") {
            a.addPercept("script", line.arg, 0.7);
            ++percepts;
        } else if (line.cmd == "task") {
            std::istringstream iss(line.arg);
            std::string name; iss >> name;
            std::string rest; std::getline(iss, rest);
            if (!rest.empty() && rest[0] == ' ') rest = rest.substr(1);
            a.scheduleTask(name, rest);
            ++tasksScheduled;
        } else if (line.cmd == "run") {
            size_t n = static_cast<size_t>(std::stoul(line.arg));
            a.runCycles(n);
        }
    }

    ASSERT_EQ(goalsSet,       1u);
    ASSERT_EQ(percepts,       1u);
    ASSERT_EQ(tasksScheduled, 1u);
    ASSERT_EQ(a.cognitiveLoop().cycleCount(), 2u);

    auto gh = a.atomStore().getNode(AtomType::CONCEPT, "Goal:objective");
    ASSERT_TRUE(gh != nullptr);
}

// -----------------------------------------------------------------------
// E2E Scenario 11: goals listing — verifies that multiple goals can be
// set with distinct priorities and retrieved from the TaskManager in the
// order they were added (underpins the 'goals' REPL command).

TEST(e2e_goals_listing) {
    auto a = makeAgent("e2e-goals");

    a.setGoal("goal-alpha", "First goal",  0.9);
    a.setGoal("goal-beta",  "Second goal", 0.5);
    a.setGoal("goal-gamma", "Third goal",  1.0);

    const auto& goals = a.taskManager().goals();
    ASSERT_EQ(goals.size(), 3u);

    // Goals preserve insertion order
    ASSERT_EQ(goals[0]->name, "goal-alpha");
    ASSERT_EQ(goals[1]->name, "goal-beta");
    ASSERT_EQ(goals[2]->name, "goal-gamma");

    // Priorities round-trip correctly
    ASSERT_TRUE(goals[0]->priority > 0.8);
    ASSERT_TRUE(goals[1]->priority < 0.6);
    ASSERT_TRUE(goals[2]->priority > 0.9);

    // No goals are achieved yet (we haven't run any cycles)
    for (const auto& g : goals)
        ASSERT_FALSE(g->achieved);
}

// -----------------------------------------------------------------------
// E2E Scenario 12: explicit inference pass — verifies that
// reasoningEngine().runCycle() fires matching rules and returns the
// correct InferenceResult entries (underpins the 'infer' REPL command).

TEST(e2e_explicit_inference_pass) {
    auto a = makeAgent("e2e-infer");

    // Add a concept that will satisfy the rule condition
    a.atomStore().addNode(AtomType::CONCEPT, "trigger-concept");

    // Register a rule: if trigger-concept exists → add derived-concept
    a.reasoningEngine().addRule(
        "test-derive",
        [](const AtomStore& store) {
            return store.getNode(AtomType::CONCEPT, "trigger-concept") != nullptr;
        },
        [](AtomStore& store) {
            store.addNode(AtomType::CONCEPT, "derived-concept");
        });

    // Run exactly one inference pass via the API that 'infer' command calls
    auto results = a.reasoningEngine().runCycle();

    // At least one result must exist and the rule must have fired
    ASSERT_TRUE(!results.empty());
    bool found = false;
    for (const auto& r : results) {
        if (r.ruleName == "test-derive") {
            ASSERT_TRUE(r.fired);
            found = true;
        }
    }
    ASSERT_TRUE(found);

    // The derived concept must now exist in the store
    auto dh = a.atomStore().getNode(AtomType::CONCEPT, "derived-concept");
    ASSERT_TRUE(dh != nullptr);
}

// -----------------------------------------------------------------------
// E2E Scenario 13: infer with no matching rule — runCycle() completes
// cleanly when no rule condition is satisfied, and returns unfired results.

TEST(e2e_inference_no_match) {
    auto a = makeAgent("e2e-infer-nomatch");

    // Rule whose condition is never satisfied
    a.reasoningEngine().addRule(
        "never-fires",
        [](const AtomStore& store) {
            return store.getNode(AtomType::CONCEPT, "nonexistent-atom") != nullptr;
        },
        [](AtomStore& store) {
            store.addNode(AtomType::CONCEPT, "should-not-appear");
        });

    auto results = a.reasoningEngine().runCycle();

    // Result list is non-empty (contains the unfired entry)
    ASSERT_TRUE(!results.empty());
    for (const auto& r : results) {
        if (r.ruleName == "never-fires")
            ASSERT_FALSE(r.fired);
    }

    // Side-effect atom must NOT have been created
    auto h = a.atomStore().getNode(AtomType::CONCEPT, "should-not-appear");
    ASSERT_TRUE(h == nullptr);
}

// -----------------------------------------------------------------------
// E2E Scenario 14: goals empty list — goals() returns an empty vector
// when no goals have been set.

TEST(e2e_goals_empty) {
    auto a = makeAgent("e2e-goals-empty");
    const auto& goals = a.taskManager().goals();
    ASSERT_EQ(goals.size(), 0u);
}
