/*
 * standalone/tests/bench_atom_store.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Benchmarks for AtomStore core operations.
 */
#include "bench_runner.h"
#include "cog0/AtomStore.h"

#include <string>

using namespace cog0;

// ---------------------------------------------------------------------------
// addNode — insert 1 000 distinct concept nodes into a fresh store

BENCHMARK_N(atom_store_add_node_1k, 200) {
    AtomStore store;
    for (int i = 0; i < 1000; ++i)
        store.addNode(AtomType::CONCEPT, "concept_" + std::to_string(i));
}

// ---------------------------------------------------------------------------
// addNode idempotency — repeatedly adding the same node (cache hot path)

BENCHMARK_N(atom_store_add_node_existing, 5000) {
    static AtomStore store;
    static bool seeded = []{
        store.addNode(AtomType::CONCEPT, "hot");
        return true;
    }();
    (void)seeded;
    store.addNode(AtomType::CONCEPT, "hot");
}

// ---------------------------------------------------------------------------
// getNode hit — retrieve an already-stored node

BENCHMARK_N(atom_store_get_node_hit, 10000) {
    static AtomStore store;
    static bool seeded = []{
        store.addNode(AtomType::CONCEPT, "target");
        return true;
    }();
    (void)seeded;
    (void)store.getNode(AtomType::CONCEPT, "target");
}

// ---------------------------------------------------------------------------
// getNode miss — query a name that does not exist

BENCHMARK_N(atom_store_get_node_miss, 10000) {
    static AtomStore store;
    (void)store.getNode(AtomType::CONCEPT, "never_inserted");
}

// ---------------------------------------------------------------------------
// addLink — build 500 inheritance links over 100 pre-existing nodes

BENCHMARK_N(atom_store_add_link_500, 100) {
    AtomStore store;
    std::vector<Handle> nodes;
    nodes.reserve(100);
    for (int i = 0; i < 100; ++i)
        nodes.push_back(store.addNode(AtomType::CONCEPT, "n" + std::to_string(i)));
    for (int i = 0; i < 500; ++i)
        store.addLink(AtomType::INHERITANCE,
                      {nodes[i % 100], nodes[(i + 1) % 100]});
}

// ---------------------------------------------------------------------------
// getByType — scan all atoms of a given type (1 000-node store)

BENCHMARK_N(atom_store_get_by_type_1k, 500) {
    static AtomStore store;
    static bool seeded = []{
        for (int i = 0; i < 500; ++i)
            store.addNode(AtomType::CONCEPT, "c" + std::to_string(i));
        for (int i = 0; i < 500; ++i)
            store.addNode(AtomType::PREDICATE, "p" + std::to_string(i));
        return true;
    }();
    (void)seeded;
    (void)store.getByType(AtomType::CONCEPT);
}

// ---------------------------------------------------------------------------
// getIncoming — fan-in query (node connected to 10 links)

BENCHMARK_N(atom_store_get_incoming, 5000) {
    static AtomStore store;
    static Handle hub;
    static bool seeded = []{
        hub = store.addNode(AtomType::CONCEPT, "hub");
        for (int i = 0; i < 10; ++i) {
            auto spoke = store.addNode(AtomType::CONCEPT, "spoke_" + std::to_string(i));
            store.addLink(AtomType::INHERITANCE, {spoke, hub});
        }
        return true;
    }();
    (void)seeded;
    (void)store.getIncoming(hub);
}

// ---------------------------------------------------------------------------
// remove — add 100 nodes and remove each one

BENCHMARK_N(atom_store_remove_100, 200) {
    AtomStore store;
    std::vector<Handle> hs;
    hs.reserve(100);
    for (int i = 0; i < 100; ++i)
        hs.push_back(store.addNode(AtomType::CONCEPT, "r" + std::to_string(i)));
    for (auto& h : hs)
        store.remove(h);
}

// ---------------------------------------------------------------------------
// clear — fill with 1 000 nodes, then clear

BENCHMARK_N(atom_store_clear_1k, 200) {
    AtomStore store;
    for (int i = 0; i < 1000; ++i)
        store.addNode(AtomType::CONCEPT, "c" + std::to_string(i));
    store.clear();
}

// ---------------------------------------------------------------------------
// TruthValue set/get round-trip

BENCHMARK_N(atom_store_truth_value_rw, 20000) {
    static AtomStore store;
    static Handle h = store.addNode(AtomType::CONCEPT, "tv_bench");
    h->setTV(TruthValue{0.9, 0.8});
    (void)h->tv();
}
