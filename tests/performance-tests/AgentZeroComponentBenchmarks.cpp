/*
 * AgentZeroComponentBenchmarks.cpp
 *
 * Comprehensive benchmark suite for Agent-Zero components
 * Part of AZ-INT-002: Implement benchmarking suite
 *
 * Copyright (C) 2024 OpenCog Foundation
 * All Rights Reserved
 */

#include "AgentZeroBenchmark.h"
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <iostream>
#include <vector>

using namespace opencog;

/**
 * Benchmark AtomSpace core operations relevant to Agent-Zero
 */
void benchmark_atomspace_operations(AgentZeroBenchmark& bench)
{
    std::cout << "\n=== AtomSpace Operations Benchmarks ===" << std::endl;
    
    // Benchmark 1: Node creation
    bench.run_benchmark("node_creation", 10000, [&]() {
        static int counter = 0;
        bench.get_atomspace()->add_node(CONCEPT_NODE, "TestNode_" + std::to_string(counter++));
    });
    
    // Benchmark 2: Link creation
    std::vector<Handle> nodes;
    for (int i = 0; i < 100; ++i) {
        nodes.push_back(bench.get_atomspace()->add_node(CONCEPT_NODE, "LinkTestNode_" + std::to_string(i)));
    }
    
    bench.run_benchmark("link_creation", 5000, [&]() {
        static int idx = 0;
        Handle h1 = nodes[idx % nodes.size()];
        Handle h2 = nodes[(idx + 1) % nodes.size()];
        bench.get_atomspace()->add_link(LIST_LINK, h1, h2);
        idx++;
    });
    
    // Benchmark 3: AtomSpace retrieval
    bench.run_benchmark("atom_retrieval", 10000, [&]() {
        static int idx = 0;
        Handle h = nodes[idx % nodes.size()];
        bench.get_atomspace()->get_atom(h);
        idx++;
    });
    
    // Benchmark 4: Incoming set access
    bench.run_benchmark("incoming_set_access", 5000, [&]() {
        static int idx = 0;
        Handle h = nodes[idx % nodes.size()];
        h->getIncomingSet();
        idx++;
    });
}

/**
 * Benchmark cognitive loop operations
 */
void benchmark_cognitive_operations(AgentZeroBenchmark& bench)
{
    std::cout << "\n=== Cognitive Operations Benchmarks ===" << std::endl;
    
    // Benchmark 1: Goal creation and management
    bench.run_benchmark("goal_creation", 5000, [&]() {
        static int counter = 0;
        auto goal = bench.get_atomspace()->add_node(CONCEPT_NODE, "Goal_" + std::to_string(counter++));
        bench.get_atomspace()->add_link(EVALUATION_LINK, 
            bench.get_atomspace()->add_node(PREDICATE_NODE, "hasGoal"),
            goal);
    });
    
    // Benchmark 2: Simple pattern matching simulation
    std::vector<Handle> pattern_nodes;
    for (int i = 0; i < 50; ++i) {
        auto node = bench.get_atomspace()->add_node(CONCEPT_NODE, "Pattern_" + std::to_string(i));
        pattern_nodes.push_back(node);
    }
    
    bench.run_benchmark("pattern_matching_sim", 2000, [&]() {
        for (auto& node : pattern_nodes) {
            node->getIncomingSet();
        }
    });
}

/**
 * Benchmark knowledge integration operations
 */
void benchmark_knowledge_operations(AgentZeroBenchmark& bench)
{
    std::cout << "\n=== Knowledge Integration Benchmarks ===" << std::endl;
    
    // Benchmark 1: Fact storage
    bench.run_benchmark("fact_storage", 3000, [&]() {
        static int counter = 0;
        auto concept = bench.get_atomspace()->add_node(CONCEPT_NODE, "Fact_" + std::to_string(counter++));
        auto predicate = bench.get_atomspace()->add_node(PREDICATE_NODE, "isFact");
        bench.get_atomspace()->add_link(EVALUATION_LINK, predicate, concept);
    });
    
    // Benchmark 2: Semantic relationship creation
    bench.run_benchmark("semantic_relation", 2000, [&]() {
        static int counter = 0;
        auto subject = bench.get_atomspace()->add_node(CONCEPT_NODE, "Subject_" + std::to_string(counter));
        auto object = bench.get_atomspace()->add_node(CONCEPT_NODE, "Object_" + std::to_string(counter));
        auto relation = bench.get_atomspace()->add_node(PREDICATE_NODE, "relatesTo");
        
        bench.get_atomspace()->add_link(EVALUATION_LINK,
            relation,
            bench.get_atomspace()->add_link(LIST_LINK, subject, object));
        counter++;
    });
}

/**
 * Benchmark memory operations
 */
void benchmark_memory_operations(AgentZeroBenchmark& bench)
{
    std::cout << "\n=== Memory Operations Benchmarks ===" << std::endl;
    
    // Benchmark 1: Episodic memory entry creation
    bench.run_benchmark("episodic_memory", 2000, [&]() {
        static int counter = 0;
        auto event = bench.get_atomspace()->add_node(CONCEPT_NODE, "Event_" + std::to_string(counter));
        // Note: TIME_NODE and AT_TIME_LINK are not standard in base atomspace
        // Using CONCEPT_NODE and LIST_LINK as alternative for demonstration
        auto timestamp = bench.get_atomspace()->add_node(CONCEPT_NODE, "Time_" + std::to_string(counter));
        bench.get_atomspace()->add_link(LIST_LINK, event, timestamp);
        counter++;
    });
    
    // Benchmark 2: Working memory simulation (short-lived atoms)
    bench.run_benchmark("working_memory", 5000, [&]() {
        static int counter = 0;
        auto temp = bench.get_atomspace()->add_node(CONCEPT_NODE, "Temp_" + std::to_string(counter++));
        // In real implementation, these would be removed after use
    });
}

/**
 * Scaling benchmarks - test performance with varying data sizes
 */
void run_scaling_benchmarks(AgentZeroBenchmark& bench)
{
    std::cout << "\n=== Scaling Benchmarks ===" << std::endl;
    
    std::vector<size_t> sizes = {100, 500, 1000, 5000};
    std::vector<Handle> scaling_nodes;
    
    auto results = bench.run_scaling_benchmark(
        "atomspace_scaling",
        sizes,
        [&](size_t size) {
            // Setup: create specified number of nodes
            scaling_nodes.clear();
            for (size_t i = 0; i < size; ++i) {
                scaling_nodes.push_back(
                    bench.get_atomspace()->add_node(CONCEPT_NODE, "ScalingNode_" + std::to_string(i))
                );
            }
        },
        [&]() {
            // Benchmark: access all nodes
            for (const auto& node : scaling_nodes) {
                bench.get_atomspace()->get_atom(node);
            }
        }
    );
}

/**
 * Stress test - sustained high-load operations
 */
void run_stress_test(AgentZeroBenchmark& bench)
{
    std::cout << "\n=== Stress Test ===" << std::endl;
    
    bench.run_benchmark("sustained_operations", 1000, [&]() {
        // Simulate a full cognitive cycle
        static int cycle = 0;
        
        // 1. Perception (create sensory atoms)
        auto percept = bench.get_atomspace()->add_node(CONCEPT_NODE, "Percept_" + std::to_string(cycle));
        
        // 2. Processing (create relationships)
        auto processed = bench.get_atomspace()->add_node(CONCEPT_NODE, "Processed_" + std::to_string(cycle));
        bench.get_atomspace()->add_link(INHERITANCE_LINK, processed, percept);
        
        // 3. Action planning (create goal links)
        auto action = bench.get_atomspace()->add_node(CONCEPT_NODE, "Action_" + std::to_string(cycle));
        auto goal = bench.get_atomspace()->add_node(CONCEPT_NODE, "Goal_" + std::to_string(cycle));
        bench.get_atomspace()->add_link(EVALUATION_LINK,
            bench.get_atomspace()->add_node(PREDICATE_NODE, "achieves"),
            bench.get_atomspace()->add_link(LIST_LINK, action, goal));
        
        cycle++;
    });
}

/**
 * Main benchmark suite entry point
 */
int main(int argc, char** argv)
{
    std::cout << "Agent-Zero Benchmarking Suite" << std::endl;
    std::cout << "AZ-INT-002: Comprehensive Performance Testing" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    // Initialize benchmark framework
    AgentZeroBenchmark benchmark(nullptr, "./benchmark_results", true);
    
    try {
        // Run all benchmark categories
        benchmark_atomspace_operations(benchmark);
        benchmark_cognitive_operations(benchmark);
        benchmark_knowledge_operations(benchmark);
        benchmark_memory_operations(benchmark);
        run_scaling_benchmarks(benchmark);
        run_stress_test(benchmark);
        
        // Print summary
        benchmark.print_summary();
        
        // Export results
        benchmark.export_results_to_csv("agentzero_benchmark_results");
        
        std::cout << "\n✅ All benchmarks completed successfully!" << std::endl;
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "❌ Benchmark failed with error: " << e.what() << std::endl;
        return 1;
    }
}
