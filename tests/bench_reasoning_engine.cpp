/*
 * standalone/tests/bench_reasoning_engine.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Benchmarks for ReasoningEngine core operations.
 */
#include "bench_runner.h"
#include "cog0/AtomStore.h"
#include "cog0/ReasoningEngine.h"

#include <string>

using namespace cog0;

// ---------------------------------------------------------------------------
// addRule — register 100 rules, then discard the engine

BENCHMARK_N(reasoning_engine_add_rule_100, 500) {
    auto store = std::make_shared<AtomStore>();
    ReasoningEngine re(store);
    for (int i = 0; i < 100; ++i)
        re.addRule("r" + std::to_string(i),
                   [](const AtomStore&) { return false; },
                   [](AtomStore&) {});
}

// ---------------------------------------------------------------------------
// runCycle — 10 rules, condition always false (no-op cycle)

BENCHMARK_N(reasoning_engine_run_cycle_no_fire, 5000) {
    static auto store = std::make_shared<AtomStore>();
    static ReasoningEngine* re = []{
        auto* p = new ReasoningEngine(store);
        for (int i = 0; i < 10; ++i)
            p->addRule("r" + std::to_string(i),
                       [](const AtomStore&) { return false; },
                       [](AtomStore&) {});
        return p;
    }();
    re->runCycle();
}

// ---------------------------------------------------------------------------
// runCycle — 10 rules, condition always true (all fire)

BENCHMARK_N(reasoning_engine_run_cycle_all_fire, 2000) {
    auto store = std::make_shared<AtomStore>();
    ReasoningEngine re(store);
    for (int i = 0; i < 10; ++i)
        re.addRule("r" + std::to_string(i),
                   [](const AtomStore&) { return true; },
                   [](AtomStore&) {});
    re.runCycle();
}

// ---------------------------------------------------------------------------
// runForwardChaining — chain terminates after 1 firing (adds sentinel node)

BENCHMARK_N(reasoning_engine_forward_chaining_1, 2000) {
    auto store = std::make_shared<AtomStore>();
    ReasoningEngine re(store);
    re.addRule("once",
               [](const AtomStore& s) {
                   return s.getNode(AtomType::CONCEPT, "done") == nullptr;
               },
               [](AtomStore& s) {
                   s.addNode(AtomType::CONCEPT, "done");
               });
    re.runForwardChaining(10);
}

// ---------------------------------------------------------------------------
// queryExists — store has 1 000 nodes; query a present node

BENCHMARK_N(reasoning_engine_query_exists_hit, 10000) {
    static auto store = std::make_shared<AtomStore>();
    static ReasoningEngine re(store);
    static bool seeded = []{
        for (int i = 0; i < 1000; ++i)
            store->addNode(AtomType::CONCEPT, "c" + std::to_string(i));
        return true;
    }();
    (void)seeded;
    (void)re.queryExists(AtomType::CONCEPT, "c500");
}

// ---------------------------------------------------------------------------
// queryExists — query a missing node (miss path)

BENCHMARK_N(reasoning_engine_query_exists_miss, 10000) {
    static auto store = std::make_shared<AtomStore>();
    static ReasoningEngine re(store);
    (void)re.queryExists(AtomType::CONCEPT, "not_there");
}

// ---------------------------------------------------------------------------
// queryInherits — store has 200 inheritance links; query a present pair

BENCHMARK_N(reasoning_engine_query_inherits_hit, 5000) {
    static auto store = std::make_shared<AtomStore>();
    static ReasoningEngine re(store);
    static bool seeded = []{
        for (int i = 0; i < 200; ++i) {
            auto child  = store->addNode(AtomType::CONCEPT, "child_"  + std::to_string(i));
            auto parent = store->addNode(AtomType::CONCEPT, "parent_" + std::to_string(i));
            store->addLink(AtomType::INHERITANCE, {child, parent})->setTV({1.0, 1.0});
        }
        return true;
    }();
    (void)seeded;
    (void)re.queryInherits("child_100", "parent_100");
}

// ---------------------------------------------------------------------------
// Scaling: addNode with an ever-growing store (10 k nodes total)

BENCHMARK_N(atom_store_scaling_10k, 10) {
    AtomStore store;
    for (int i = 0; i < 10000; ++i)
        store.addNode(AtomType::CONCEPT, "node_" + std::to_string(i));
}

// ---------------------------------------------------------------------------
// Scaling: forward-chaining transitivity closure on a 20-node chain
//   A0 -> A1 -> ... -> A19  (expects 190 derived links)

BENCHMARK_N(reasoning_engine_transitivity_20, 50) {
    auto store = std::make_shared<AtomStore>();
    ReasoningEngine re(store);

    constexpr int N = 20;
    std::vector<Handle> nodes;
    nodes.reserve(N);
    for (int i = 0; i < N; ++i)
        nodes.push_back(store->addNode(AtomType::CONCEPT, "A" + std::to_string(i)));
    for (int i = 0; i + 1 < N; ++i)
        store->addLink(AtomType::INHERITANCE, {nodes[i], nodes[i+1]})->setTV({0.9, 0.9});

    re.addRule("transitivity",
               [](const AtomStore& s) {
                   auto links = s.getByType(AtomType::INHERITANCE);
                   for (const auto& ab : links) {
                       if (ab->out().size() < 2) continue;
                       const auto& B = ab->out()[1];
                       for (const auto& bc : links) {
                           if (bc->out().size() < 2) continue;
                           if (bc->out()[0] == B) {
                               const auto& A = ab->out()[0];
                               const auto& C = bc->out()[1];
                               if (!s.getLink(AtomType::INHERITANCE, {A, C}))
                                   return true;
                           }
                       }
                   }
                   return false;
               },
               [](AtomStore& s) {
                   auto links = s.getByType(AtomType::INHERITANCE);
                   for (const auto& ab : links) {
                       if (ab->out().size() < 2) continue;
                       const auto& A = ab->out()[0];
                       const auto& B = ab->out()[1];
                       for (const auto& bc : s.getByType(AtomType::INHERITANCE)) {
                           if (bc->out().size() < 2 || bc->out()[0] != B) continue;
                           const auto& C = bc->out()[1];
                           if (!s.getLink(AtomType::INHERITANCE, {A, C}))
                               s.addLink(AtomType::INHERITANCE, {A, C})->setTV(
                                   {ab->tv().strength * bc->tv().strength,
                                    ab->tv().confidence * bc->tv().confidence * 0.9});
                       }
                   }
               });

    re.runForwardChaining(N);
}
