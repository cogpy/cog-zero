/*
 * standalone/tests/test_reasoning_engine_fuzz.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Property-based (fuzz) tests for ReasoningEngine.
 *
 * Each test generates a range of pseudo-random rule sets / knowledge graphs
 * and verifies that fundamental invariants hold across all of them.
 * No external fuzzing library is required — only C++17 stdlib <random>.
 */
#include "test_runner.h"
#include "cog0/AtomStore.h"
#include "cog0/ReasoningEngine.h"

#include <algorithm>
#include <random>
#include <string>
#include <vector>

using namespace cog0;

// ---------------------------------------------------------------------------
// Property: rules whose condition always returns false never fire.

TEST(fuzz_never_fire_rules) {
    std::mt19937 rng(0xABCD1234);
    std::uniform_int_distribution<int> countDist(1, 20);
    for (int trial = 0; trial < 100; ++trial) {
        auto store = std::make_shared<AtomStore>();
        ReasoningEngine re(store);
        int n = countDist(rng);
        std::vector<bool> fired(n, false);
        for (int i = 0; i < n; ++i) {
            re.addRule("r" + std::to_string(i),
                       [](const AtomStore&) { return false; },
                       [&fired, i](AtomStore&) { fired[i] = true; });
        }
        re.runCycle();
        for (int i = 0; i < n; ++i)
            ASSERT_FALSE(fired[i]);
    }
}

// ---------------------------------------------------------------------------
// Property: rules whose condition always returns true always fire in runCycle.

TEST(fuzz_always_fire_rules) {
    std::mt19937 rng(0x4321DCBA);
    std::uniform_int_distribution<int> countDist(1, 10);
    for (int trial = 0; trial < 100; ++trial) {
        auto store = std::make_shared<AtomStore>();
        ReasoningEngine re(store);
        int n = countDist(rng);
        std::vector<int> fireCount(n, 0);
        for (int i = 0; i < n; ++i) {
            re.addRule("r" + std::to_string(i),
                       [](const AtomStore&) { return true; },
                       [&fireCount, i](AtomStore&) { ++fireCount[i]; });
        }
        re.runCycle();
        for (int i = 0; i < n; ++i)
            ASSERT_GE(fireCount[i], 1);
    }
}

// ---------------------------------------------------------------------------
// Property: runForwardChaining terminates within the specified maxCycles limit.
//           A rule that adds a unique node per firing will fire exactly once
//           per chain because on the second pass no new node is added,
//           leaving the condition false.

TEST(fuzz_forward_chaining_terminates) {
    std::mt19937 rng(0xDEADBEEF);
    std::uniform_int_distribution<int> maxCyc(1, 20);
    for (int trial = 0; trial < 80; ++trial) {
        auto store = std::make_shared<AtomStore>();
        ReasoningEngine re(store);
        size_t mc = static_cast<size_t>(maxCyc(rng));
        int fired = 0;
        // Rule fires every cycle unconditionally, incrementing a counter.
        // runForwardChaining should stop because no new rules are triggered
        // after the first round, or at maxCycles.
        re.addRule("always",
                   [](const AtomStore&) { return true; },
                   [&fired](AtomStore&) { ++fired; });
        size_t total = re.runForwardChaining(mc);
        // The engine terminates — total firings must be <= maxCycles
        ASSERT_LE(static_cast<size_t>(total), mc);
    }
}

// ---------------------------------------------------------------------------
// Property: addRule / removeRule — rule count is restored to its original value.

TEST(fuzz_add_remove_rule_count) {
    std::mt19937 rng(0x13579BDF);
    std::uniform_int_distribution<int> countDist(1, 15);
    for (int trial = 0; trial < 100; ++trial) {
        auto store = std::make_shared<AtomStore>();
        ReasoningEngine re(store);
        size_t initial = re.rules().size();
        int n = countDist(rng);
        for (int i = 0; i < n; ++i) {
            re.addRule("r" + std::to_string(i),
                       [](const AtomStore&) { return false; },
                       [](AtomStore&) {});
        }
        ASSERT_EQ(re.rules().size(), initial + static_cast<size_t>(n));
        for (int i = 0; i < n; ++i)
            re.removeRule("r" + std::to_string(i));
        ASSERT_EQ(re.rules().size(), initial);
    }
}

// ---------------------------------------------------------------------------
// Property: queryExists reflects the store state for random atoms.

TEST(fuzz_query_exists_mirrors_store) {
    std::mt19937 rng(0x2468ACE0);
    std::uniform_int_distribution<int> countDist(1, 20);
    for (int trial = 0; trial < 80; ++trial) {
        auto store = std::make_shared<AtomStore>();
        ReasoningEngine re(store);
        int n = countDist(rng);
        for (int i = 0; i < n; ++i) {
            std::string name = "atom_" + std::to_string(i);
            // Before adding: should be absent
            ASSERT_FALSE(re.queryExists(AtomType::CONCEPT, name));
            store->addNode(AtomType::CONCEPT, name);
            // After adding: should be present
            ASSERT_TRUE(re.queryExists(AtomType::CONCEPT, name));
        }
    }
}

// ---------------------------------------------------------------------------
// Property: queryInherits is true only when the inheritance link exists.

TEST(fuzz_query_inherits_consistency) {
    std::mt19937 rng(0x9ABCDEF0);
    std::uniform_int_distribution<int> countDist(2, 10);
    for (int trial = 0; trial < 80; ++trial) {
        auto store = std::make_shared<AtomStore>();
        ReasoningEngine re(store);
        int n = countDist(rng);
        std::vector<std::string> names;
        for (int i = 0; i < n; ++i)
            names.push_back("c" + std::to_string(trial * 100 + i));

        for (const auto& nm : names)
            store->addNode(AtomType::CONCEPT, nm);

        // Randomly add inheritance links and verify queryInherits
        std::uniform_int_distribution<int> idxDist(0, n - 1);
        for (int k = 0; k < 5; ++k) {
            int ci = idxDist(rng);
            int pi = idxDist(rng);
            if (ci == pi) continue;
            const std::string& child  = names[ci];
            const std::string& parent = names[pi];

            bool before = re.queryInherits(child, parent);
            auto ch = store->getNode(AtomType::CONCEPT, child);
            auto pa = store->getNode(AtomType::CONCEPT, parent);
            auto existing = store->getLink(AtomType::INHERITANCE, {ch, pa});
            if (!existing) {
                ASSERT_FALSE(before);
                auto lnk = store->addLink(AtomType::INHERITANCE, {ch, pa});
                lnk->setTV(TruthValue{1.0, 1.0});
                ASSERT_TRUE(re.queryInherits(child, parent));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Property: results from runCycle list each fired rule exactly once.

TEST(fuzz_run_cycle_results_unique_rules) {
    std::mt19937 rng(0xFEDCBA98);
    std::uniform_int_distribution<int> countDist(1, 10);
    for (int trial = 0; trial < 60; ++trial) {
        auto store = std::make_shared<AtomStore>();
        ReasoningEngine re(store);
        int n = countDist(rng);
        for (int i = 0; i < n; ++i) {
            re.addRule("r" + std::to_string(i),
                       [](const AtomStore&) { return true; },
                       [](AtomStore&) {});
        }
        auto results = re.runCycle();
        // Each fired rule name should appear exactly once in results
        std::vector<std::string> names;
        for (const auto& r : results)
            if (r.fired) names.push_back(r.ruleName);
        std::sort(names.begin(), names.end());
        auto unique_end = std::unique(names.begin(), names.end());
        ASSERT_EQ(unique_end - names.begin(),
                  static_cast<std::ptrdiff_t>(names.size()));
    }
}

// ---------------------------------------------------------------------------
// Property: a rule that conditionally creates nodes is monotone — it never
//           removes nodes from the store.

TEST(fuzz_rules_are_monotone_when_only_adding) {
    std::mt19937 rng(0x76543210);
    std::uniform_int_distribution<int> countDist(1, 15);
    for (int trial = 0; trial < 60; ++trial) {
        auto store = std::make_shared<AtomStore>();
        ReasoningEngine re(store);
        int n = countDist(rng);
        // Seed the store
        for (int i = 0; i < n; ++i)
            store->addNode(AtomType::CONCEPT, "seed_" + std::to_string(i));
        size_t before = store->size();

        // Rule only adds, never removes
        re.addRule("addonly",
                   [](const AtomStore& s) {
                       return s.getNode(AtomType::CONCEPT, "derived") == nullptr;
                   },
                   [](AtomStore& s) {
                       s.addNode(AtomType::CONCEPT, "derived");
                   });
        re.runForwardChaining(5);
        ASSERT_GE(store->size(), before);
    }
}

// ---------------------------------------------------------------------------
// Property: two independent ReasoningEngine instances on identical stores
//           produce identical queryInherits results.

TEST(fuzz_independent_engines_agree) {
    std::mt19937 rng(0x11111111);
    std::uniform_int_distribution<int> countDist(2, 8);
    for (int trial = 0; trial < 50; ++trial) {
        auto storeA = std::make_shared<AtomStore>();
        auto storeB = std::make_shared<AtomStore>();
        ReasoningEngine reA(storeA);
        ReasoningEngine reB(storeB);

        int n = countDist(rng);
        for (int i = 0; i < n; ++i) {
            std::string nm = "n" + std::to_string(i);
            storeA->addNode(AtomType::CONCEPT, nm);
            storeB->addNode(AtomType::CONCEPT, nm);
        }
        // Add a random inheritance link in both
        std::uniform_int_distribution<int> idxDist(0, n - 1);
        int ci = idxDist(rng);
        int pi = idxDist(rng);
        if (ci != pi) {
            std::string child  = "n" + std::to_string(ci);
            std::string parent = "n" + std::to_string(pi);
            auto chA = storeA->getNode(AtomType::CONCEPT, child);
            auto paA = storeA->getNode(AtomType::CONCEPT, parent);
            storeA->addLink(AtomType::INHERITANCE, {chA, paA})->setTV({1.0, 1.0});
            auto chB = storeB->getNode(AtomType::CONCEPT, child);
            auto paB = storeB->getNode(AtomType::CONCEPT, parent);
            storeB->addLink(AtomType::INHERITANCE, {chB, paB})->setTV({1.0, 1.0});
            ASSERT_EQ(reA.queryInherits(child, parent),
                      reB.queryInherits(child, parent));
        }
    }
}

// ---------------------------------------------------------------------------
// Property: rules are kept sorted by descending priority after random inserts.

TEST(fuzz_rules_sorted_by_priority) {
    std::mt19937 rng(0x22222222);
    std::uniform_int_distribution<int> countDist(2, 20);
    std::uniform_real_distribution<double> prioDist(0.0, 100.0);

    for (int trial = 0; trial < 80; ++trial) {
        auto store = std::make_shared<AtomStore>();
        ReasoningEngine re(store);
        int n = countDist(rng);
        for (int i = 0; i < n; ++i) {
            re.addRule("prio_" + std::to_string(i),
                       [](const AtomStore&) { return false; },
                       [](AtomStore&) {},
                       prioDist(rng));
        }
        const auto& rules = re.rules();
        ASSERT_EQ(rules.size(), static_cast<size_t>(n));
        for (size_t i = 1; i < rules.size(); ++i) {
            ASSERT_GE(rules[i - 1]->priority, rules[i]->priority);
        }
    }
}

// ---------------------------------------------------------------------------
// Property: queryEval is true iff EvaluationLink strength >= threshold.

TEST(fuzz_query_eval_threshold) {
    std::mt19937 rng(0x33333333);
    std::uniform_real_distribution<double> sDist(0.0, 1.0);
    std::uniform_real_distribution<double> thrDist(0.0, 1.0);

    for (int trial = 0; trial < 100; ++trial) {
        auto store = std::make_shared<AtomStore>();
        ReasoningEngine re(store);

        std::string pred = "P" + std::to_string(trial);
        std::string arg  = "A" + std::to_string(trial);
        auto predAtom = store->addNode(AtomType::PREDICATE, pred);
        auto argAtom  = store->addNode(AtomType::CONCEPT, arg);
        auto listLink = store->addLink(AtomType::LIST, {argAtom});
        auto evalLink = store->addLink(AtomType::EVALUATION, {predAtom, listLink});

        double strength = sDist(rng);
        double thr      = thrDist(rng);
        evalLink->setTV(TruthValue{strength, 1.0});

        bool expected = strength >= thr;
        ASSERT_EQ(re.queryEval(pred, arg, thr), expected);
    }
}

// ---------------------------------------------------------------------------
// Property: inheritance with strength < 0.5 is not matched by queryInherits.

TEST(fuzz_query_inherits_strength_gate) {
    std::mt19937 rng(0x44444444);
    std::uniform_real_distribution<double> sDist(0.0, 1.0);

    for (int trial = 0; trial < 80; ++trial) {
        auto store = std::make_shared<AtomStore>();
        ReasoningEngine re(store);
        auto child  = store->addNode(AtomType::CONCEPT, "child_" + std::to_string(trial));
        auto parent = store->addNode(AtomType::CONCEPT, "parent_" + std::to_string(trial));
        auto lnk = store->addLink(AtomType::INHERITANCE, {child, parent});
        double strength = sDist(rng);
        lnk->setTV(TruthValue{strength, 1.0});

        bool expected = strength >= 0.5;
        ASSERT_EQ(re.queryInherits("child_" + std::to_string(trial),
                                   "parent_" + std::to_string(trial)),
                  expected);
    }
}

// ---------------------------------------------------------------------------
// Property: runCycle returns one result entry per registered rule.

TEST(fuzz_run_cycle_result_count_matches_rules) {
    std::mt19937 rng(0x55555555);
    std::uniform_int_distribution<int> countDist(0, 15);
    std::uniform_int_distribution<int> fireDist(0, 1);

    for (int trial = 0; trial < 60; ++trial) {
        auto store = std::make_shared<AtomStore>();
        ReasoningEngine re(store);
        int n = countDist(rng);
        for (int i = 0; i < n; ++i) {
            bool willFire = fireDist(rng) != 0;
            re.addRule("rc_" + std::to_string(i),
                       [willFire](const AtomStore&) { return willFire; },
                       [](AtomStore&) {});
        }
        auto results = re.runCycle();
        ASSERT_EQ(results.size(), re.rules().size());
        ASSERT_EQ(results.size(), static_cast<size_t>(n));
    }
}

// ---------------------------------------------------------------------------
// Property: fireCount on each rule increments exactly once per firing cycle.

TEST(fuzz_rule_fire_count_increments) {
    std::mt19937 rng(0x66666666);
    std::uniform_int_distribution<int> cyclesDist(1, 8);

    for (int trial = 0; trial < 40; ++trial) {
        auto store = std::make_shared<AtomStore>();
        ReasoningEngine re(store);
        re.addRule("counter",
                   [](const AtomStore&) { return true; },
                   [](AtomStore&) {});
        int cycles = cyclesDist(rng);
        for (int c = 0; c < cycles; ++c)
            re.runCycle();
        ASSERT_EQ(re.rules().front()->fireCount, static_cast<size_t>(cycles));
    }
}
