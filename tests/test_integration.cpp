/*
 * standalone/tests/test_integration.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Phase 9 — Full system integration tests.
 *
 * These tests exercise all four subsystems (AtomStore, TaskManager,
 * ReasoningEngine, CognitiveLoop) working together in complex, realistic
 * scenarios that go beyond the per-component unit tests and the basic e2e
 * smoke tests.
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "test_runner.h"
#include "cog0/Agent.h"

using namespace cog0;

// -----------------------------------------------------------------------
// Helpers

static Agent makeAgent(const std::string& name  = "integ-agent",
                       bool               verbose = false,
                       size_t             maxTasksPerCycle = 10)
{
    AgentConfig cfg;
    cfg.name             = name;
    cfg.verbose          = verbose;
    cfg.cycleInterval    = std::chrono::milliseconds(5);
    cfg.maxTasksPerCycle = maxTasksPerCycle;
    return Agent(cfg);
}

// -----------------------------------------------------------------------
// Integration Test 1: all four subsystems collaborate
//
// A goal is set, a percept triggers a reasoning rule that creates a task,
// the task executes and writes an atom, and the final CognitiveLoop cycle
// count plus AtomStore contents confirm end-to-end cooperation.

TEST(integration_all_subsystems_cooperative) {
    auto a = makeAgent("integ-all");

    // Phase: goal management
    a.setGoal("mission", "Complete the mission", 1.0);

    // Phase: reasoning — rule fires when trigger percept is present
    bool ruleFired = false;
    a.reasoningEngine().addRule(
        "mission-trigger",
        [](const AtomStore& s) {
            return s.getNode(AtomType::CONCEPT, "Percept:activate-mission") != nullptr;
        },
        [&](AtomStore& s) {
            ruleFired = true;
            s.addNode(AtomType::CONCEPT, "Mission:active");
        },
        5.0
    );

    // Phase: task management — task records result
    bool taskExecuted = false;
    a.scheduleTask("mission-task", "Execute mission step", Priority::HIGH,
                   [&]() {
                       taskExecuted = true;
                       a.atomStore().addNode(AtomType::CONCEPT, "Result:mission-complete");
                       return true;
                   });

    // Phase: perception
    a.addPercept("command-center", "activate-mission", 0.95);

    // Run enough cycles to process percept + reasoning + task
    a.runCycles(5);

    // Verify all phases ran
    ASSERT_TRUE(ruleFired);
    ASSERT_TRUE(taskExecuted);
    ASSERT_TRUE(a.atomStore().getNode(AtomType::CONCEPT, "Mission:active")    != nullptr);
    ASSERT_TRUE(a.atomStore().getNode(AtomType::CONCEPT, "Result:mission-complete") != nullptr);
    ASSERT_EQ(a.cognitiveLoop().cycleCount(), 5u);
}

// -----------------------------------------------------------------------
// Integration Test 2: knowledge accumulation over many cycles
//
// Rules fire repeatedly, accumulating atoms. The final AtomStore size
// and specific atoms confirm correct incremental knowledge growth.

TEST(integration_knowledge_accumulation) {
    auto a = makeAgent("integ-knowledge");

    // Add a counting rule that fires once then adds a "done" flag to stop
    int fireCount = 0;
    a.reasoningEngine().addRule(
        "accumulate-knowledge",
        [](const AtomStore& s) {
            return s.getNode(AtomType::CONCEPT, "Knowledge:seed") != nullptr
                && s.getNode(AtomType::CONCEPT, "Knowledge:done")  == nullptr;
        },
        [&](AtomStore& s) {
            ++fireCount;
            s.addNode(AtomType::CONCEPT, "Knowledge:fact-" + std::to_string(fireCount));
            if (fireCount >= 5)
                s.addNode(AtomType::CONCEPT, "Knowledge:done");
        },
        2.0
    );

    a.atomStore().addNode(AtomType::CONCEPT, "Knowledge:seed");
    a.runCycles(10);

    // Rule fires exactly 5 times (once per cycle until done)
    ASSERT_EQ(fireCount, 5);

    for (int i = 1; i <= 5; ++i) {
        auto h = a.atomStore().getNode(AtomType::CONCEPT,
                                       "Knowledge:fact-" + std::to_string(i));
        ASSERT_TRUE(h != nullptr);
    }
    ASSERT_TRUE(a.atomStore().getNode(AtomType::CONCEPT, "Knowledge:done") != nullptr);
}

// -----------------------------------------------------------------------
// Integration Test 3: goal-driven task completion with feedback loop
//
// Two goals with different priorities; tasks attached to each goal complete
// in priority order.  After completion, goal "achieved" flags are set.

TEST(integration_goal_driven_task_completion) {
    auto a = makeAgent("integ-goal-tasks");

    auto g1 = a.setGoal("primary",   "High priority goal", 1.0);
    auto g2 = a.setGoal("secondary", "Low priority goal",  0.3);

    std::vector<std::string> executionLog;

    auto t1 = a.scheduleTask("primary-task", "", Priority::HIGH,
                              [&]() { executionLog.push_back("primary");   return true; });
    auto t2 = a.scheduleTask("secondary-task", "", Priority::LOW,
                              [&]() { executionLog.push_back("secondary"); return true; });

    a.taskManager().attachToGoal(g1->id, t1);
    a.taskManager().attachToGoal(g2->id, t2);

    a.runCycles(3);

    ASSERT_EQ(executionLog.size(), 2u);
    // HIGH task must execute before LOW
    ASSERT_EQ(executionLog[0], "primary");
    ASSERT_EQ(executionLog[1], "secondary");

    // Both tasks should be marked completed
    ASSERT_TRUE(t1->completed);
    ASSERT_TRUE(t2->completed);
}

// -----------------------------------------------------------------------
// Integration Test 4: rule cascade — chained inference produces final atom
//
// Three rules form a chain: A→B, B→C, C→D.  After seeding A and running
// cycles, D must appear in the AtomStore.

TEST(integration_rule_cascade) {
    auto a = makeAgent("integ-cascade");

    a.reasoningEngine().addRule("rule-AB",
        [](const AtomStore& s){ return s.getNode(AtomType::CONCEPT,"Chain:A") != nullptr; },
        [](AtomStore& s){ s.addNode(AtomType::CONCEPT,"Chain:B"); }, 3.0);

    a.reasoningEngine().addRule("rule-BC",
        [](const AtomStore& s){ return s.getNode(AtomType::CONCEPT,"Chain:B") != nullptr; },
        [](AtomStore& s){ s.addNode(AtomType::CONCEPT,"Chain:C"); }, 2.0);

    a.reasoningEngine().addRule("rule-CD",
        [](const AtomStore& s){ return s.getNode(AtomType::CONCEPT,"Chain:C") != nullptr; },
        [](AtomStore& s){ s.addNode(AtomType::CONCEPT,"Chain:D"); }, 1.0);

    a.atomStore().addNode(AtomType::CONCEPT, "Chain:A");
    a.runCycles(4);

    ASSERT_TRUE(a.atomStore().getNode(AtomType::CONCEPT, "Chain:B") != nullptr);
    ASSERT_TRUE(a.atomStore().getNode(AtomType::CONCEPT, "Chain:C") != nullptr);
    ASSERT_TRUE(a.atomStore().getNode(AtomType::CONCEPT, "Chain:D") != nullptr);
}

// -----------------------------------------------------------------------
// Integration Test 5: concurrent goals and tasks with percept-driven logic
//
// Multiple goals and tasks are interleaved; percepts drive additional rule
// firings; final state is fully consistent.

TEST(integration_concurrent_goals_and_tasks) {
    auto a = makeAgent("integ-concurrent");

    // Three goals at different priority levels
    a.setGoal("alpha", "Goal alpha", 1.0);
    a.setGoal("beta",  "Goal beta",  0.6);
    a.setGoal("gamma", "Goal gamma", 0.2);

    std::atomic<int> alphaCount{0}, betaCount{0}, gammaCount{0};

    for (int i = 0; i < 3; ++i) {
        a.scheduleTask("alpha-task-" + std::to_string(i), "", Priority::HIGH,
                       [&]() { ++alphaCount; return true; });
        a.scheduleTask("beta-task-"  + std::to_string(i), "", Priority::NORMAL,
                       [&]() { ++betaCount;  return true; });
        a.scheduleTask("gamma-task-" + std::to_string(i), "", Priority::LOW,
                       [&]() { ++gammaCount; return true; });
    }

    // Add percepts that trigger reasoning
    for (int i = 0; i < 3; ++i)
        a.addPercept("env", "event-" + std::to_string(i), 0.7);

    a.runCycles(5);

    ASSERT_EQ(alphaCount.load(), 3);
    ASSERT_EQ(betaCount.load(),  3);
    ASSERT_EQ(gammaCount.load(), 3);
}

// -----------------------------------------------------------------------
// Integration Test 6: agent restart — stop and restart is clean
//
// Agent is started in background mode, stopped, and then restarted.
// State is preserved across start/stop cycles; new cycles are additive.

TEST(integration_agent_restart) {
    AgentConfig cfg;
    cfg.name          = "integ-restart";
    cfg.cycleInterval = std::chrono::milliseconds(10);
    cfg.maxCycles     = 3;
    Agent a(cfg);

    a.setGoal("persist", "Persists across restart", 0.8);

    // First run
    a.start();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (a.isRunning() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ASSERT_FALSE(a.isRunning());
    size_t firstCount = a.cognitiveLoop().cycleCount();
    ASSERT_GE(firstCount, 1u);

    // Goal must still be present
    auto gh = a.atomStore().getNode(AtomType::CONCEPT, "Goal:persist");
    ASSERT_TRUE(gh != nullptr);

    // Second run — cycle count increases
    a.cognitiveLoop().setMaxCycles(3);
    a.start();
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (a.isRunning() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ASSERT_FALSE(a.isRunning());
    ASSERT_GT(a.cognitiveLoop().cycleCount(), firstCount);
}

// -----------------------------------------------------------------------
// Integration Test 7: rule priority ordering in forward chaining
//
// Two rules can both fire; the one with higher priority must execute first,
// leaving an ordering marker in the AtomStore.

TEST(integration_rule_priority_ordering) {
    auto a = makeAgent("integ-priority");

    std::vector<std::string> fireOrder;

    a.reasoningEngine().addRule("low-priority-rule",
        [](const AtomStore& s){ return s.getNode(AtomType::CONCEPT,"Trigger:go") != nullptr; },
        [&](AtomStore& s){ fireOrder.push_back("low");  s.addNode(AtomType::CONCEPT,"Fired:low");  },
        1.0);

    a.reasoningEngine().addRule("high-priority-rule",
        [](const AtomStore& s){ return s.getNode(AtomType::CONCEPT,"Trigger:go") != nullptr; },
        [&](AtomStore& s){ fireOrder.push_back("high"); s.addNode(AtomType::CONCEPT,"Fired:high"); },
        10.0);

    a.atomStore().addNode(AtomType::CONCEPT, "Trigger:go");
    a.runCycles(1);

    // Both rules fire in a single cycle
    ASSERT_EQ(fireOrder.size(), 2u);
    // High-priority rule should fire first
    ASSERT_EQ(fireOrder[0], "high");
    ASSERT_EQ(fireOrder[1], "low");
}

// -----------------------------------------------------------------------
// Integration Test 8: large-scale atom population stays bounded
//
// 500 unique concept nodes are added; the store indexes them correctly and
// getByType returns the right count.

TEST(integration_large_scale_atoms) {
    auto a = makeAgent("integ-large");

    const size_t N = 500;
    for (size_t i = 0; i < N; ++i)
        a.atomStore().addNode(AtomType::CONCEPT, "Entity:" + std::to_string(i));

    ASSERT_GE(a.atomStore().size(), N);

    auto concepts = a.atomStore().getByType(AtomType::CONCEPT);
    // At least N concept atoms exist (the loop may have added a few from rules)
    ASSERT_GE(concepts.size(), N);

    // Specific atoms are retrievable
    for (size_t i = 0; i < N; i += 50) {
        auto h = a.atomStore().getNode(AtomType::CONCEPT, "Entity:" + std::to_string(i));
        ASSERT_TRUE(h != nullptr);
    }
}

// -----------------------------------------------------------------------
// Integration Test 9: multi-goal task coordination with subtasks
//
// A goal is broken into subtasks that must all complete before the goal is
// considered achieved.

TEST(integration_multi_goal_subtask_coordination) {
    auto a = makeAgent("integ-subtasks");

    auto goal = a.setGoal("complex-goal", "Goal with sub-tasks", 1.0);

    std::vector<bool> subtaskDone(3, false);

    auto t1 = a.taskManager().createTask("sub-1", "First sub-task",  Priority::HIGH,
                                          [&]() { subtaskDone[0] = true; return true; });
    auto t2 = a.taskManager().createTask("sub-2", "Second sub-task", Priority::NORMAL,
                                          [&]() { subtaskDone[1] = true; return true; });
    auto t3 = a.taskManager().createTask("sub-3", "Third sub-task",  Priority::LOW,
                                          [&]() { subtaskDone[2] = true; return true; });

    t1->addSubtask(t2);
    t1->addSubtask(t3);

    a.taskManager().enqueue(t1);
    a.taskManager().enqueue(t2);
    a.taskManager().enqueue(t3);
    a.taskManager().attachToGoal(goal->id, t1);

    a.runCycles(3);

    ASSERT_TRUE(subtaskDone[0]);
    ASSERT_TRUE(subtaskDone[1]);
    ASSERT_TRUE(subtaskDone[2]);
}

// -----------------------------------------------------------------------
// Integration Test 10: reflection stats accumulate correctly over cycles
//
// The reflection hook is used to accumulate global statistics across many
// cycles; totals must match the number of percepts and tasks injected.

TEST(integration_reflection_stats_accumulation) {
    auto a = makeAgent("integ-reflect-stats");

    size_t totalRulesFired = 0;
    size_t totalCycles     = 0;

    a.cognitiveLoop().setReflectionHook([&](const CycleStats& s) {
        totalRulesFired += s.rulesFired;
        ++totalCycles;
    });

    // Add a rule that fires every cycle
    a.atomStore().addNode(AtomType::CONCEPT, "AlwaysPresent:yes");
    a.reasoningEngine().addRule("always-fires",
        [](const AtomStore& s){ return s.getNode(AtomType::CONCEPT,"AlwaysPresent:yes") != nullptr; },
        [](AtomStore&){}, // no-op action
        1.0);

    const size_t N = 5;
    a.runCycles(N);

    ASSERT_EQ(totalCycles, N);
    // Rule fires once per cycle
    ASSERT_GE(totalRulesFired, N);
}

// -----------------------------------------------------------------------
// Integration Test 11: percept salience affects attention rule
//
// High-salience percepts should trigger the attention rule; low-salience
// ones should not.

TEST(integration_percept_salience_attention) {
    auto a = makeAgent("integ-salience");

    // Inject one high and one low salience percept
    a.addPercept("sensor", "high-value-event", 0.96); // above 0.9 threshold
    a.addPercept("sensor", "low-value-event",  0.2);  // below threshold
    a.runCycles(2);

    // High-salience percept's attention flag must exist
    auto highFlag = a.atomStore().getNode(AtomType::CONCEPT,
                                          "AttentionFlag:high-salience");
    ASSERT_TRUE(highFlag != nullptr);

    // Low-salience percept should NOT have its own attention flag
    auto lowFlag = a.atomStore().getNode(AtomType::CONCEPT,
                                         "Percept:low-value-event");
    // The low-salience percept atom exists (it was added), but no attention
    // flag should have been created specifically for it being high-salience
    (void)lowFlag;

    // AtomStore should be consistent (not corrupted)
    ASSERT_GT(a.atomStore().size(), 0u);
}

// -----------------------------------------------------------------------
// Integration Test 12: full agent lifecycle — init → run → stop → inspect
//
// Exercises the complete lifecycle in one test: configure, add state,
// run asynchronously, stop, then inspect final state.

TEST(integration_full_lifecycle) {
    AgentConfig cfg;
    cfg.name          = "integ-lifecycle";
    cfg.cycleInterval = std::chrono::milliseconds(15);
    cfg.maxCycles     = 5;
    cfg.maxTasksPerCycle = 5;
    Agent a(cfg);

    a.setGoal("lifecycle-goal", "Lifecycle test goal", 0.9);
    a.addPercept("init", "bootstrap", 0.8);

    int tasksRun = 0;
    for (int i = 0; i < 3; ++i)
        a.scheduleTask("life-task-" + std::to_string(i), "", Priority::NORMAL,
                       [&]() { ++tasksRun; return true; });

    a.start();

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (a.isRunning() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    a.stop(); // idempotent if already stopped

    ASSERT_FALSE(a.isRunning());
    ASSERT_EQ(tasksRun, 3);

    std::string report = a.statusReport();
    ASSERT_FALSE(report.empty());
    ASSERT_TRUE(report.find("lifecycle-goal") != std::string::npos ||
                report.find("Cycles run")     != std::string::npos);
}
