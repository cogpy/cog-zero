/*
 * standalone/tests/test_atom_store_fuzz.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Property-based (fuzz) tests for AtomStore.
 *
 * Each test generates a range of pseudo-random inputs and verifies that
 * fundamental invariants hold across all of them.  No external fuzzing
 * library is required — only C++17 stdlib <random>.
 */
#include "test_runner.h"
#include "cog0/AtomStore.h"

#include <random>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace cog0;

// ---------------------------------------------------------------------------
// Helpers

static const AtomType kNodeTypes[] = {
    AtomType::CONCEPT,
    AtomType::PREDICATE,
    AtomType::VARIABLE,
    AtomType::CUSTOM,
};
static constexpr size_t kNumNodeTypes =
    sizeof(kNodeTypes) / sizeof(kNodeTypes[0]);

// Generate a deterministic pseudo-random name of length 1..8
static std::string randName(std::mt19937& rng, size_t maxLen = 8) {
    static const char kAlpha[] = "abcdefghijklmnopqrstuvwxyz0123456789_";
    std::uniform_int_distribution<size_t> lenDist(1, maxLen);
    std::uniform_int_distribution<size_t> charDist(0, sizeof(kAlpha) - 2);
    size_t len = lenDist(rng);
    std::string s;
    s.reserve(len);
    for (size_t i = 0; i < len; ++i)
        s += kAlpha[charDist(rng)];
    return s;
}

// ---------------------------------------------------------------------------
// Property: addNode is idempotent — calling it twice returns the same handle.

TEST(fuzz_add_node_idempotent) {
    std::mt19937 rng(0xDEADBEEF);
    for (int trial = 0; trial < 200; ++trial) {
        AtomStore store;
        std::uniform_int_distribution<size_t> typeDist(0, kNumNodeTypes - 1);
        AtomType t = kNodeTypes[typeDist(rng)];
        std::string name = randName(rng);
        auto h1 = store.addNode(t, name);
        auto h2 = store.addNode(t, name);
        ASSERT_EQ(h1, h2);
    }
}

// ---------------------------------------------------------------------------
// Property: every added node is retrievable by getNode.

TEST(fuzz_add_node_retrievable) {
    std::mt19937 rng(0xC0FFEE01);
    for (int trial = 0; trial < 100; ++trial) {
        AtomStore store;
        std::uniform_int_distribution<size_t> typeDist(0, kNumNodeTypes - 1);
        std::uniform_int_distribution<int>    countDist(1, 30);
        int n = countDist(rng);
        for (int i = 0; i < n; ++i) {
            AtomType t = kNodeTypes[typeDist(rng)];
            std::string name = randName(rng);
            auto added = store.addNode(t, name);
            auto got   = store.getNode(t, name);
            ASSERT_EQ(added, got);
        }
    }
}

// ---------------------------------------------------------------------------
// Property: store.size() == number of distinct (type, name) pairs added.

TEST(fuzz_size_matches_distinct_nodes) {
    std::mt19937 rng(0xBAADF00D);
    for (int trial = 0; trial < 100; ++trial) {
        AtomStore store;
        std::uniform_int_distribution<size_t> typeDist(0, kNumNodeTypes - 1);
        std::uniform_int_distribution<int>    countDist(1, 40);
        int n = countDist(rng);

        std::unordered_set<std::string> seen;
        for (int i = 0; i < n; ++i) {
            AtomType    t    = kNodeTypes[typeDist(rng)];
            std::string name = randName(rng, 3); // small alphabet → collisions
            store.addNode(t, name);
            // Track distinct (type, name) keys
            std::string key = std::to_string(static_cast<int>(t)) + ":" + name;
            seen.insert(key);
        }
        ASSERT_EQ(store.size(), seen.size());
    }
}

// ---------------------------------------------------------------------------
// Property: getByType count equals nodes of that type added (distinct).

TEST(fuzz_get_by_type_count) {
    std::mt19937 rng(0xFEEDFACE);
    for (int trial = 0; trial < 80; ++trial) {
        AtomStore store;
        std::uniform_int_distribution<size_t> typeDist(0, kNumNodeTypes - 1);
        std::uniform_int_distribution<int>    countDist(1, 30);
        int n = countDist(rng);

        // per-type distinct name sets
        std::unordered_map<int, std::unordered_set<std::string>> byType;
        for (int i = 0; i < n; ++i) {
            AtomType    t    = kNodeTypes[typeDist(rng)];
            std::string name = randName(rng, 4);
            store.addNode(t, name);
            byType[static_cast<int>(t)].insert(name);
        }
        for (const auto& [ti, names] : byType) {
            AtomType t = static_cast<AtomType>(ti);
            ASSERT_EQ(store.getByType(t).size(), names.size());
        }
    }
}

// ---------------------------------------------------------------------------
// Property: after remove(), size decreases by exactly 1 for a known node.

TEST(fuzz_remove_decreases_size) {
    std::mt19937 rng(0xABCDABCD);
    for (int trial = 0; trial < 100; ++trial) {
        AtomStore store;
        std::uniform_int_distribution<int> countDist(2, 20);
        int n = countDist(rng);
        std::vector<Handle> handles;
        for (int i = 0; i < n; ++i) {
            std::string name = "node_" + std::to_string(i);
            handles.push_back(store.addNode(AtomType::CONCEPT, name));
        }
        ASSERT_EQ(store.size(), static_cast<size_t>(n));

        // Remove a random subset
        std::shuffle(handles.begin(), handles.end(), rng);
        std::uniform_int_distribution<size_t> rmDist(1, handles.size());
        size_t toRemove = rmDist(rng);
        for (size_t i = 0; i < toRemove; ++i)
            store.remove(handles[i]);

        ASSERT_EQ(store.size(), static_cast<size_t>(n) - toRemove);
    }
}

// ---------------------------------------------------------------------------
// Property: TruthValue strength/confidence are preserved exactly.

TEST(fuzz_truth_value_round_trip) {
    std::mt19937 rng(0x12345678);
    std::uniform_real_distribution<double> vDist(0.0, 1.0);
    for (int trial = 0; trial < 200; ++trial) {
        AtomStore store;
        double s = vDist(rng);
        double c = vDist(rng);
        auto h = store.addNode(AtomType::CONCEPT, "tv");
        h->setTV(TruthValue{s, c});
        ASSERT_EQ(h->tv().strength,   s);
        ASSERT_EQ(h->tv().confidence, c);
    }
}

// ---------------------------------------------------------------------------
// Property: clear() always results in size == 0.

TEST(fuzz_clear_always_empties) {
    std::mt19937 rng(0x11223344);
    std::uniform_int_distribution<int> countDist(0, 50);
    for (int trial = 0; trial < 50; ++trial) {
        AtomStore store;
        int n = countDist(rng);
        for (int i = 0; i < n; ++i)
            store.addNode(AtomType::CONCEPT, "c" + std::to_string(i));
        store.clear();
        ASSERT_EQ(store.size(), 0u);
    }
}

// ---------------------------------------------------------------------------
// Property: addLink with same (type, outgoing) is idempotent.

TEST(fuzz_add_link_idempotent) {
    std::mt19937 rng(0xCAFEBABE);
    std::uniform_int_distribution<int> countDist(2, 6);
    for (int trial = 0; trial < 100; ++trial) {
        AtomStore store;
        int n = countDist(rng);
        HandleVec nodes;
        for (int i = 0; i < n; ++i)
            nodes.push_back(store.addNode(AtomType::CONCEPT, "n" + std::to_string(i)));

        auto l1 = store.addLink(AtomType::INHERITANCE, {nodes[0], nodes[1]});
        auto l2 = store.addLink(AtomType::INHERITANCE, {nodes[0], nodes[1]});
        ASSERT_EQ(l1, l2);
    }
}

// ---------------------------------------------------------------------------
// Property: incoming index is consistent — every link appears in the
//           incoming set of each of its outgoing atoms.

TEST(fuzz_incoming_index_consistent) {
    std::mt19937 rng(0xDEADC0DE);
    std::uniform_int_distribution<int> nodeCount(3, 10);
    std::uniform_int_distribution<int> linkCount(2, 8);

    for (int trial = 0; trial < 60; ++trial) {
        AtomStore store;
        int nn = nodeCount(rng);
        int nl = linkCount(rng);

        std::vector<Handle> nodes;
        for (int i = 0; i < nn; ++i)
            nodes.push_back(store.addNode(AtomType::CONCEPT, "x" + std::to_string(i)));

        std::uniform_int_distribution<int> idxDist(0, nn - 1);
        for (int i = 0; i < nl; ++i) {
            int a = idxDist(rng);
            int b = idxDist(rng);
            auto lnk = store.addLink(AtomType::INHERITANCE, {nodes[a], nodes[b]});
            // Both endpoints must have lnk in their incoming sets
            auto incA = store.getIncoming(nodes[a]);
            auto incB = store.getIncoming(nodes[b]);
            bool foundA = std::find(incA.begin(), incA.end(), lnk) != incA.end();
            bool foundB = std::find(incB.begin(), incB.end(), lnk) != incB.end();
            ASSERT_TRUE(foundA);
            ASSERT_TRUE(foundB);
        }
    }
}

// ---------------------------------------------------------------------------
// Property: getNode returns nullptr for names that were never added.

TEST(fuzz_get_nonexistent_node_returns_null) {
    std::mt19937 rng(0x0BADC0DE);
    for (int trial = 0; trial < 50; ++trial) {
        AtomStore store;
        // Populate with names "node_0" .. "node_9"
        for (int i = 0; i < 10; ++i)
            store.addNode(AtomType::CONCEPT, "node_" + std::to_string(i));
        // Query names that were never added
        for (int i = 100; i < 110; ++i)
            ASSERT_EQ(store.getNode(AtomType::CONCEPT, "node_" + std::to_string(i)), nullptr);
    }
}

// ---------------------------------------------------------------------------
// Property: STI/LTI are preserved exactly after random assignments.

TEST(fuzz_attention_values_round_trip) {
    std::mt19937 rng(0xFACEB00C);
    std::uniform_real_distribution<double> vDist(-1.0, 1.0);
    for (int trial = 0; trial < 200; ++trial) {
        AtomStore store;
        double sti = vDist(rng);
        double lti = vDist(rng);
        auto h = store.addNode(AtomType::PREDICATE, "av");
        h->setSTI(sti);
        h->setLTI(lti);
        ASSERT_EQ(h->sti(), sti);
        ASSERT_EQ(h->lti(), lti);
    }
}
