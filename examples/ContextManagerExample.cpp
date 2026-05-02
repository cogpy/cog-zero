/*
 * examples/ContextManagerExample.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * ContextManager usage example
 * Demonstrates situational awareness and context management
 * Part of the AGENT-ZERO-GENESIS project - AZ-CONTEXT-001
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/agentzero/memory/ContextManager.h>
#include <opencog/agentzero/memory/MemoryTypes.h>
#include <opencog/util/Logger.h>

using namespace opencog;
using namespace opencog::agentzero::memory;

void print_separator(const std::string& title)
{
    std::cout << "\n========================================\n";
    std::cout << title << "\n";
    std::cout << "========================================\n";
}

void print_context_info(const ContextManager& cm, const std::string& ctx_id)
{
    auto info = cm.getContextInfo(ctx_id);
    std::cout << "Context: " << ctx_id << "\n";
    std::cout << "  - Importance: " << info["importance"] << "\n";
    std::cout << "  - Atom Count: " << info["atom_count"] << "\n";
    std::cout << "  - Access Count: " << info["access_count"] << "\n";
}

int main(int argc, char* argv[])
{
    // Set logging level
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    std::cout << "ContextManager Example - Situational Awareness Demo\n";
    std::cout << "====================================================\n";
    
    // Create AtomSpace
    auto atomspace = std::make_shared<AtomSpace>();
    
    // Create ContextManager
    auto context_manager = std::make_unique<ContextManager>(
        atomspace,
        50,    // max contexts
        0.1,   // min importance
        std::chrono::hours(1)  // decay time
    );
    
    // Initialize
    if (!context_manager->initialize()) {
        std::cerr << "Failed to initialize ContextManager\n";
        return 1;
    }
    
    // === Example 1: Creating and Managing Contexts ===
    print_separator("Example 1: Creating Contexts");
    
    // Create different types of contexts
    context_manager->createContext("planning_session", ContextType::TASK, {
        {"goal", "navigate_to_kitchen"},
        {"priority", "high"}
    }, 0.8);
    
    context_manager->createContext("kitchen_location", ContextType::SPATIAL, {
        {"x", "10.5"},
        {"y", "5.2"},
        {"z", "0.0"}
    }, 0.7);
    
    context_manager->createContext("user_interaction", ContextType::SOCIAL, {
        {"user", "Alice"},
        {"mood", "friendly"}
    }, 0.6);
    
    context_manager->createContext("current_time", ContextType::TEMPORAL, {
        {"hour", "14"},
        {"day", "Monday"}
    }, 0.5);
    
    std::cout << "Created " << context_manager->getContextCount() << " contexts\n";
    
    // === Example 2: Adding Atoms to Contexts ===
    print_separator("Example 2: Populating Contexts with Atoms");
    
    // Create some atoms representing knowledge
    Handle goal = atomspace->add_node(CONCEPT_NODE, "NavigateToKitchen");
    Handle location = atomspace->add_node(CONCEPT_NODE, "KitchenLocation");
    Handle user = atomspace->add_node(CONCEPT_NODE, "Alice");
    Handle action = atomspace->add_node(PREDICATE_NODE, "MoveTo");
    
    // Add atoms to appropriate contexts
    context_manager->addAtomToContext("planning_session", goal);
    context_manager->addAtomToContext("planning_session", action);
    context_manager->addAtomToContext("kitchen_location", location);
    context_manager->addAtomToContext("user_interaction", user);
    
    std::cout << "Added atoms to contexts\n";
    std::cout << "Total unique atoms: " << context_manager->getTotalAtomCount() << "\n";
    
    // === Example 3: Context Switching ===
    print_separator("Example 3: Context Switching");
    
    std::cout << "Activating 'planning_session' context...\n";
    context_manager->setActiveContext("planning_session");
    std::cout << "Active context: " << context_manager->getActiveContext() << "\n";
    
    std::cout << "\nSwitching to 'user_interaction' context...\n";
    context_manager->setActiveContext("user_interaction");
    std::cout << "Active context: " << context_manager->getActiveContext() << "\n";
    
    std::cout << "\nSwitching to 'kitchen_location' context...\n";
    context_manager->setActiveContext("kitchen_location");
    std::cout << "Active context: " << context_manager->getActiveContext() << "\n";
    
    auto history = context_manager->getContextHistory(5);
    std::cout << "\nContext history (most recent first):\n";
    for (const auto& ctx : history) {
        std::cout << "  - " << ctx << "\n";
    }
    
    // === Example 4: Context Queries ===
    print_separator("Example 4: Querying Contexts");
    
    // Query by type
    auto task_contexts = context_manager->getContextsByType(ContextType::TASK);
    std::cout << "Task contexts: ";
    for (const auto& ctx : task_contexts) {
        std::cout << ctx << " ";
    }
    std::cout << "\n";
    
    // Query by importance
    auto important_contexts = context_manager->getContextsByImportance(0.6);
    std::cout << "Important contexts (>= 0.6): ";
    for (const auto& ctx : important_contexts) {
        std::cout << ctx << " ";
    }
    std::cout << "\n";
    
    // Get most important
    auto top_contexts = context_manager->getMostImportantContexts(2);
    std::cout << "Top 2 most important contexts:\n";
    for (const auto& ctx : top_contexts) {
        print_context_info(*context_manager, ctx);
    }
    
    // === Example 5: Cross-Context Atom Tracking ===
    print_separator("Example 5: Cross-Context Atom Tracking");
    
    // Add a shared concept to multiple contexts
    Handle shared_concept = atomspace->add_node(CONCEPT_NODE, "SharedKnowledge");
    context_manager->addAtomToContext("planning_session", shared_concept);
    context_manager->addAtomToContext("user_interaction", shared_concept);
    
    auto contexts_for_atom = context_manager->getContextsForAtom(shared_concept);
    std::cout << "Atom 'SharedKnowledge' appears in " << contexts_for_atom.size() << " contexts:\n";
    for (const auto& ctx : contexts_for_atom) {
        std::cout << "  - " << ctx << "\n";
    }
    
    // === Example 6: Metadata Management ===
    print_separator("Example 6: Context Metadata");
    
    // Update metadata
    context_manager->setContextMetadata("planning_session", "status", "in_progress");
    context_manager->setContextMetadata("planning_session", "last_update", "now");
    
    auto metadata = context_manager->getAllContextMetadata("planning_session");
    std::cout << "Planning session metadata:\n";
    for (const auto& pair : metadata) {
        std::cout << "  - " << pair.first << ": " << pair.second << "\n";
    }
    
    // === Example 7: Importance Management ===
    print_separator("Example 7: Dynamic Importance");
    
    std::cout << "Initial importance of 'planning_session': "
              << context_manager->getContextImportance("planning_session") << "\n";
    
    // Boost importance when context becomes more relevant
    context_manager->boostContextImportance("planning_session", 1.2);
    std::cout << "After boost (x1.2): "
              << context_manager->getContextImportance("planning_session") << "\n";
    
    // Manually set importance
    context_manager->setContextImportance("current_time", 0.3);
    std::cout << "Set 'current_time' importance to: 0.3\n";
    
    // === Example 8: Context Merging ===
    print_separator("Example 8: Merging Contexts");
    
    // Create two related contexts
    context_manager->createContext("navigation_plan_a", ContextType::TASK, {}, 0.6);
    context_manager->createContext("navigation_plan_b", ContextType::TASK, {}, 0.5);
    
    Handle plan_a = atomspace->add_node(CONCEPT_NODE, "PlanA");
    Handle plan_b = atomspace->add_node(CONCEPT_NODE, "PlanB");
    
    context_manager->addAtomToContext("navigation_plan_a", plan_a);
    context_manager->addAtomToContext("navigation_plan_b", plan_b);
    
    std::cout << "Before merge: " << context_manager->getContextCount() << " contexts\n";
    
    // Merge plan_b into plan_a
    context_manager->mergeContexts("navigation_plan_b", "navigation_plan_a", true);
    
    std::cout << "After merge: " << context_manager->getContextCount() << " contexts\n";
    std::cout << "Merged context has " 
              << context_manager->getAtomsInContext("navigation_plan_a").size()
              << " atoms\n";
    
    // === Example 9: Creating AtomSpace Snapshot ===
    print_separator("Example 9: Context Snapshot");
    
    Handle snapshot = context_manager->createContextSnapshot();
    std::cout << "Created context snapshot in AtomSpace\n";
    std::cout << "Snapshot handle: " << snapshot.to_string() << "\n";
    
    // === Example 10: Statistics ===
    print_separator("Example 10: Context Statistics");
    
    auto stats = context_manager->getStatistics();
    std::cout << "Context Manager Statistics:\n";
    std::cout << "  - Total contexts: " << stats.total_contexts << "\n";
    std::cout << "  - Active contexts: " << stats.active_contexts << "\n";
    std::cout << "  - Total context switches: " << stats.total_context_switches << "\n";
    std::cout << "  - Average importance: " << stats.average_context_importance << "\n";
    std::cout << "  - Total atoms in contexts: " << stats.total_atoms_in_contexts << "\n";
    
    // === Example 11: Cleanup and Management ===
    print_separator("Example 11: Context Cleanup");
    
    std::cout << "Contexts before cleanup: " << context_manager->getContextCount() << "\n";
    
    // Create some low-importance contexts
    for (int i = 0; i < 5; i++) {
        std::string ctx_id = "temp_context_" + std::to_string(i);
        context_manager->createContext(ctx_id, ContextType::COGNITIVE, {}, 0.05);
    }
    
    std::cout << "After adding temporary contexts: " 
              << context_manager->getContextCount() << "\n";
    
    // Decay importances (would remove very low importance contexts over time)
    size_t decayed = context_manager->decayContextImportances();
    std::cout << "Decayed " << decayed << " contexts\n";
    
    // === Cleanup ===
    print_separator("Shutting Down");
    
    std::cout << "Final context count: " << context_manager->getContextCount() << "\n";
    std::cout << "Final atom count: " << context_manager->getTotalAtomCount() << "\n";
    
    context_manager->shutdown();
    
    std::cout << "\nContextManager example completed successfully!\n";
    
    return 0;
}
