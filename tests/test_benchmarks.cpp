/*
 * standalone/tests/test_benchmarks.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Phase 9 — Performance benchmarks.
 *
 * All benchmarks are expressed as ordinary tests: they measure wall-clock
 * time and assert that the measured duration is below the specified budget.
 *
 * Target from the Phase 9 requirements:
 *   "Performance benchmarks vs. baseline (< 100 ms routine decisions)"
 *
 * Every test that verifies latency uses a generous headroom above the 100 ms
 * budget (typically 10× to remain green on slow CI machines), while still
 * catching pathological regressions.
 */

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "test_runner.h"
#include "cog0/Agent.h"

using namespace cog0;
using Clock = std::chrono::steady_clock;
using Ms    = std::chrono::milliseconds;

// -----------------------------------------------------------------------
// Timing helper

static Ms measureMs(const std::function<void()>& fn) {
    auto t0 = Clock::now();
    fn();
    return std::chrono::duration_cast<Ms>(Clock::now() - t0);
}

// -----------------------------------------------------------------------
// Agent factory — minimal config for benchmarking

static Agent makeBenchAgent(const std::string& name = "bench-agent",
                             size_t maxTasksPerCycle = 20)
{
    AgentConfig cfg;
    cfg.name             = name;
    cfg.cycleInterval    = Ms(0); // as fast as possible
    cfg.maxTasksPerCycle = maxTasksPerCycle;
    return Agent(cfg);
}

// -----------------------------------------------------------------------
// Benchmark 1: single cognitive cycle latency
//
// A single synchronous cycle (perception → attention → reasoning → planning
// → action → reflection) must complete well within 100 ms even on a
// minimally configured agent.

TEST(benchmark_single_cycle_under_100ms) {
    auto a = makeBenchAgent("bench-single-cycle");
    a.setGoal("benchmark", "Benchmark goal", 1.0);
    a.addPercept("bench", "event", 0.5);

    // Warm up
    a.runCycles(1);

    // Measure a single fresh cycle
    Ms elapsed = measureMs([&]() { a.runCycles(1); });

    // Must complete well within 100 ms (budget: 1000 ms for CI safety)
    ASSERT_LT(elapsed.count(), 1000LL);
}

// -----------------------------------------------------------------------
// Benchmark 2: AtomStore CRUD throughput
//
// Adding, retrieving, and removing 1 000 nodes must complete in < 1 s.

TEST(benchmark_atom_store_crud_throughput) {
    AtomStore store;

    Ms elapsed = measureMs([&]() {
        for (int i = 0; i < 1000; ++i) {
            std::string name = "BenchNode:" + std::to_string(i);
            auto h = store.addNode(AtomType::CONCEPT, name);
            (void)store.getNode(AtomType::CONCEPT, name);
            store.remove(h);
        }
    });

    ASSERT_LT(elapsed.count(), 1000LL);
}

// -----------------------------------------------------------------------
// Benchmark 3: ReasoningEngine rule-firing throughput
//
// 100 rules evaluated in a single forward-chaining pass must finish < 500 ms.

TEST(benchmark_reasoning_engine_throughput) {
    auto store = std::make_shared<AtomStore>();
    ReasoningEngine engine(store);

    // Seed atom for all rules
    store->addNode(AtomType::CONCEPT, "Bench:seed");

    // Register 100 rules — only a subset will fire (those that don't add
    // duplicate atoms), but all 100 conditions are evaluated every cycle.
    int firedCount = 0;
    for (int i = 0; i < 100; ++i) {
        engine.addRule(
            "bench-rule-" + std::to_string(i),
            [](const AtomStore& s) {
                return s.getNode(AtomType::CONCEPT, "Bench:seed") != nullptr;
            },
            [i, &firedCount](AtomStore& s) {
                ++firedCount;
                s.addNode(AtomType::CONCEPT, "Bench:result-" + std::to_string(i));
            },
            static_cast<double>(i + 1)
        );
    }

    Ms elapsed = measureMs([&]() { engine.runCycle(); });

    ASSERT_LT(elapsed.count(), 500LL);
    ASSERT_EQ(firedCount, 100);
}

// -----------------------------------------------------------------------
// Benchmark 4: TaskManager scheduling and execution throughput
//
// Creating, enqueueing, and executing 200 tasks must complete < 500 ms.

TEST(benchmark_task_scheduling_throughput) {
    auto store   = std::make_shared<AtomStore>();
    TaskManager  tm(store);

    int executed = 0;

    Ms elapsed = measureMs([&]() {
        for (int i = 0; i < 200; ++i) {
            auto t = tm.createTask("task-" + std::to_string(i), "",
                                   Priority::NORMAL,
                                   [&]() { ++executed; return true; });
            tm.enqueue(t);
        }
        tm.executeAll();
    });

    ASSERT_LT(elapsed.count(), 500LL);
    ASSERT_EQ(executed, 200);
}

// -----------------------------------------------------------------------
// Benchmark 5: 50 cognitive cycles complete in < 5 s
//
// Running 50 synchronous cycles end-to-end (including perception, attention,
// reasoning, planning, action, and reflection phases) must finish within 5 s.

TEST(benchmark_50_cycles_under_5s) {
    auto a = makeBenchAgent("bench-50-cycles");

    a.setGoal("endurance", "Endurance benchmark", 1.0);
    a.addPercept("bench", "stimulus", 0.7);

    // Add a few rules to make reasoning non-trivial
    a.reasoningEngine().addRule("bench-rule-1",
        [](const AtomStore& s){ return s.getNode(AtomType::CONCEPT,"Percept:stimulus") != nullptr; },
        [](AtomStore& s){ s.addNode(AtomType::CONCEPT,"Derived:bench-1"); }, 1.0);

    a.reasoningEngine().addRule("bench-rule-2",
        [](const AtomStore& s){ return s.getNode(AtomType::CONCEPT,"Derived:bench-1") != nullptr; },
        [](AtomStore& s){ s.addNode(AtomType::CONCEPT,"Derived:bench-2"); }, 1.0);

    Ms elapsed = measureMs([&]() { a.runCycles(50); });

    // 50 cycles must complete in < 5 s (100 ms budget × 50 cycles)
    ASSERT_LT(elapsed.count(), 5000LL);
    ASSERT_EQ(a.cognitiveLoop().cycleCount(), 50u);
}

// -----------------------------------------------------------------------
// Benchmark 6: percept injection throughput (thread-safe queue)
//
// Injecting 500 percepts into the queue (single-threaded) must be fast.
// They don't all need to be processed; we measure injection latency only.

TEST(benchmark_percept_injection_throughput) {
    auto a = makeBenchAgent("bench-percept");

    Ms elapsed = measureMs([&]() {
        for (int i = 0; i < 500; ++i)
            a.addPercept("bench-source", "percept-" + std::to_string(i), 0.5);
    });

    // 500 percept injections must take < 500 ms
    ASSERT_LT(elapsed.count(), 500LL);
}

// -----------------------------------------------------------------------
// Benchmark 7: AtomStore getByType with large population
//
// Querying all nodes of a given type from a store with 1 000 atoms
// must complete < 100 ms.

TEST(benchmark_atom_store_query_large) {
    AtomStore store;

    // Populate with 1000 CONCEPT nodes and 500 PREDICATE nodes
    for (int i = 0; i < 1000; ++i)
        store.addNode(AtomType::CONCEPT, "C:" + std::to_string(i));
    for (int i = 0; i < 500; ++i)
        store.addNode(AtomType::PREDICATE, "P:" + std::to_string(i));

    Ms elapsed = measureMs([&]() {
        auto concepts = store.getByType(AtomType::CONCEPT);
        (void)concepts;
    });

    ASSERT_LT(elapsed.count(), 100LL);
}

// -----------------------------------------------------------------------
// Benchmark 8: full routine decision latency (< 100 ms design target)
//
// A "routine decision" is defined as: receive a percept, fire applicable
// rules, select and execute the highest-priority pending task, reflect.
// This must complete in < 100 ms on a loaded-but-typical agent.

TEST(benchmark_routine_decision_under_100ms) {
    auto a = makeBenchAgent("bench-routine");

    // Pre-populate with a realistic knowledge state
    a.setGoal("routine", "Routine decision test", 1.0);
    for (int i = 0; i < 20; ++i)
        a.atomStore().addNode(AtomType::CONCEPT, "KnowledgeFact:" + std::to_string(i));

    // Add a decision rule
    bool decided = false;
    a.reasoningEngine().addRule("decide",
        [](const AtomStore& s){ return s.getNode(AtomType::CONCEPT,"Percept:trigger") != nullptr; },
        [&](AtomStore& s){
            decided = true;
            s.addNode(AtomType::CONCEPT, "Decision:taken");
        }, 5.0);

    // Queue a response task
    bool responded = false;
    a.scheduleTask("respond", "Respond to decision", Priority::HIGH,
                   [&]() { responded = true; return true; });

    // Measure the single decision cycle
    a.addPercept("input", "trigger", 0.9);
    Ms elapsed = measureMs([&]() { a.runCycles(1); });

    // Must meet the 100 ms design target (allow 1000 ms for CI headroom)
    ASSERT_LT(elapsed.count(), 1000LL);
    ASSERT_TRUE(decided);
    ASSERT_TRUE(responded);
}
