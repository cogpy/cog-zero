/*
 * profiling_demo.cpp
 *
 * Demonstration of Agent-Zero profiling infrastructure
 * AZ-PERF-001: Performance optimization and profiling
 *
 * Copyright (C) 2024 OpenCog Foundation
 */

#include "AgentZeroProfiler.h"

#include <iostream>
#include <vector>
#include <cmath>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>

using namespace opencog;

// Simulated workload functions for profiling demonstration

void compute_intensive_task(AgentZeroProfiler* profiler)
{
    PROFILE_FUNCTION(profiler);
    
    // Simulate CPU-intensive computation
    double result = 0.0;
    for (int i = 0; i < 1000000; i++) {
        result += std::sqrt(i) * std::sin(i);
    }
    
    // Prevent optimization
    volatile double dummy = result;
    (void)dummy;
}

void memory_intensive_task(AgentZeroProfiler* profiler, AtomSpace* as)
{
    PROFILE_FUNCTION(profiler);
    
    // Simulate memory-intensive operations
    std::vector<Handle> handles;
    for (int i = 0; i < 1000; i++) {
        Handle h = as->add_node(CONCEPT_NODE, "TestNode_" + std::to_string(i));
        handles.push_back(h);
    }
    
    // Create some links
    for (size_t i = 0; i < handles.size() - 1; i++) {
        as->add_link(LIST_LINK, handles[i], handles[i+1]);
    }
}

void atomspace_query_task(AgentZeroProfiler* profiler, AtomSpace* as)
{
    PROFILE_FUNCTION(profiler);
    
    // Simulate AtomSpace queries
    HandleSeq all_nodes;
    as->get_handles_by_type(all_nodes, CONCEPT_NODE);
    
    // Access incoming sets
    for (const Handle& h : all_nodes) {
        IncomingSet iset = h->getIncomingSet();
        // Process incoming set
        volatile size_t size = iset.size();
        (void)size;
    }
}

void fast_operation(AgentZeroProfiler* profiler)
{
    PROFILE_FUNCTION(profiler);
    
    // Quick operation
    int sum = 0;
    for (int i = 0; i < 100; i++) {
        sum += i;
    }
    volatile int dummy = sum;
    (void)dummy;
}

void nested_profiling_example(AgentZeroProfiler* profiler)
{
    PROFILE_FUNCTION(profiler);
    
    {
        PROFILE_SCOPE(profiler, "nested_profiling_example::inner_loop_1");
        for (int i = 0; i < 500000; i++) {
            volatile double x = std::sqrt(i);
            (void)x;
        }
    }
    
    {
        PROFILE_SCOPE(profiler, "nested_profiling_example::inner_loop_2");
        for (int i = 0; i < 300000; i++) {
            volatile double x = std::sin(i);
            (void)x;
        }
    }
}

void cognitive_cycle_simulation(AgentZeroProfiler* profiler, AtomSpace* as)
{
    PROFILE_FUNCTION(profiler);
    
    // Simulate a cognitive processing cycle
    {
        PROFILE_SCOPE(profiler, "cognitive_cycle::perception");
        // Simulate perception
        for (int i = 0; i < 50; i++) {
            Handle h = as->add_node(CONCEPT_NODE, "Percept_" + std::to_string(i));
        }
    }
    
    {
        PROFILE_SCOPE(profiler, "cognitive_cycle::reasoning");
        // Simulate reasoning
        compute_intensive_task(profiler);
    }
    
    {
        PROFILE_SCOPE(profiler, "cognitive_cycle::action_selection");
        // Simulate action selection
        atomspace_query_task(profiler, as);
    }
}

int main(int argc, char* argv[])
{
    std::cout << "===================================================\n";
    std::cout << "  Agent-Zero Profiling Infrastructure Demo        \n";
    std::cout << "  AZ-PERF-001: Performance Optimization            \n";
    std::cout << "===================================================\n\n";
    
    // Initialize AtomSpace
    AtomSpacePtr atomspace = createAtomSpace();
    
    // Initialize profiler
    AgentZeroProfiler profiler(true, "./profiling_results", atomspace);
    
    std::cout << "Running profiling demonstration...\n\n";
    
    // Run various workloads
    std::cout << "1. Testing compute-intensive operations...\n";
    for (int i = 0; i < 5; i++) {
        compute_intensive_task(&profiler);
    }
    
    std::cout << "2. Testing memory-intensive operations...\n";
    for (int i = 0; i < 3; i++) {
        memory_intensive_task(&profiler, atomspace.get());
    }
    
    std::cout << "3. Testing AtomSpace query operations...\n";
    for (int i = 0; i < 10; i++) {
        atomspace_query_task(&profiler, atomspace.get());
    }
    
    std::cout << "4. Testing fast operations (high frequency)...\n";
    for (int i = 0; i < 100; i++) {
        fast_operation(&profiler);
    }
    
    std::cout << "5. Testing nested profiling...\n";
    for (int i = 0; i < 3; i++) {
        nested_profiling_example(&profiler);
    }
    
    std::cout << "6. Testing cognitive cycle simulation...\n";
    for (int i = 0; i < 5; i++) {
        cognitive_cycle_simulation(&profiler, atomspace.get());
    }
    
    std::cout << "\nProfiling demonstration complete.\n\n";
    
    // Print profiling summary
    profiler.print_summary();
    
    // Export results
    std::cout << "Exporting profiling data...\n";
    profiler.export_to_csv("profiling_data.csv");
    profiler.export_report("profiling_report.txt");
    
    std::cout << "\nResults exported to ./profiling_results/\n";
    std::cout << "  - profiling_data.csv        (CSV data for analysis)\n";
    std::cout << "  - profiling_report.txt      (Human-readable report)\n\n";
    
    std::cout << "===================================================\n";
    std::cout << "Profiling complete! Check the output files.\n";
    std::cout << "===================================================\n\n";
    
    std::cout << "To use external profilers:\n";
    std::cout << "  - gprof:    make profile-with-gprof\n";
    std::cout << "  - perf:     make profile-with-perf\n";
    std::cout << "  - valgrind: make profile-with-valgrind\n\n";
    
    return 0;
}
