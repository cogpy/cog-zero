/*
 * examples/WorkingMemoryExample.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Example usage of WorkingMemory implementation
 * Demonstrates AZ-MEM-002: Create WorkingMemory management
 */

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/util/Logger.h>

#include "../include/opencog/agentzero/WorkingMemory.h"

using namespace opencog;
using namespace opencog::agentzero;

int main() {
    std::cout << "=== WorkingMemory Example ===" << std::endl;
    
    // Configure logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    try {
        // Create AtomSpace
        auto atomspace = std::make_shared<AtomSpace>();
        std::cout << "Created AtomSpace" << std::endl;
        
        // Create WorkingMemory with custom configuration
        // Capacity: 50 items, Threshold: 0.2, Retention: 5 minutes
        auto working_memory = std::unique_ptr<WorkingMemory>(
            new WorkingMemory(atomspace, 50, 0.2, std::chrono::seconds(300))
        );
        
        std::cout << "Created WorkingMemory:" << std::endl;
        std::cout << "  Max Capacity: " << working_memory->getMaxCapacity() << std::endl;
        std::cout << "  Importance Threshold: " << working_memory->getImportanceThreshold() << std::endl;
        
        // Create test concepts
        std::cout << "\nAdding concepts to working memory..." << std::endl;
        
        Handle robot = atomspace->add_node(CONCEPT_NODE, "Robot");
        Handle goal = atomspace->add_node(CONCEPT_NODE, "Goal");
        Handle environment = atomspace->add_node(CONCEPT_NODE, "Environment");
        Handle action = atomspace->add_node(CONCEPT_NODE, "Action");
        
        // Add items with different importance and contexts
        working_memory->addItem(robot, 0.9, "agent_self");
        working_memory->addItem(goal, 0.8, "goals");
        working_memory->addItem(environment, 0.6, "perception");
        working_memory->addItem(action, 0.7, "planning");
        
        std::cout << "Added 4 items to working memory" << std::endl;
        
        // Demonstrate context operations
        working_memory->setActiveContext("reasoning");
        Handle reasoning = atomspace->add_node(CONCEPT_NODE, "Reasoning");
        working_memory->addItem(reasoning, 0.5);  // Uses active context
        
        std::cout << "\nMemory contents by context:" << std::endl;
        
        auto goal_items = working_memory->getItemsByContext("goals");
        std::cout << "Goals context: " << goal_items.size() << " items" << std::endl;
        
        auto perception_items = working_memory->getItemsByContext("perception");
        std::cout << "Perception context: " << perception_items.size() << " items" << std::endl;
        
        // Demonstrate importance-based operations
        std::cout << "\nImportance-based retrieval:" << std::endl;
        
        auto important_items = working_memory->getImportantItems(0.7);
        std::cout << "High importance items (>= 0.7): " << important_items.size() << std::endl;
        
        auto top_items = working_memory->getMostImportantItems(3);
        std::cout << "Top 3 most important items:" << std::endl;
        for (size_t i = 0; i < top_items.size(); ++i) {
            std::cout << "  " << (i+1) << ". " << top_items[i]->atom->to_string() 
                     << " (importance: " << top_items[i]->importance << ")" << std::endl;
        }
        
        // Simulate memory access patterns
        std::cout << "\nSimulating memory access patterns..." << std::endl;
        
        working_memory->accessItem(robot);  // Access robot concept
        working_memory->accessItem(goal);   // Access goal concept
        working_memory->accessItem(robot);  // Access robot again
        
        // Show performance statistics
        auto stats = working_memory->getPerformanceStats();
        std::cout << "\nPerformance Statistics:" << std::endl;
        std::cout << "  Memory Size: " << static_cast<int>(stats["current_size"]) << std::endl;
        std::cout << "  Capacity Utilization: " << std::fixed << std::setprecision(1) 
                  << (stats["capacity_utilization"] * 100) << "%" << std::endl;
        std::cout << "  Hit Rate: " << std::fixed << std::setprecision(3) 
                  << stats["hit_rate"] << std::endl;
        std::cout << "  Total Operations: " << static_cast<int>(stats["total_operations"]) << std::endl;
        
        // Demonstrate memory management
        std::cout << "\nTesting memory management..." << std::endl;
        
        // Add items with low importance
        for (int i = 0; i < 5; ++i) {
            Handle temp_concept = atomspace->add_node(CONCEPT_NODE, "Temp" + std::to_string(i));
            working_memory->addItem(temp_concept, 0.1, "temporary");  // Low importance
        }
        
        std::cout << "Added 5 low-importance temporary items" << std::endl;
        std::cout << "Memory size before cleanup: " << working_memory->getCurrentSize() << std::endl;
        
        // Run cleanup to remove low-importance items
        size_t removed = working_memory->runCleanup(true);
        std::cout << "Cleanup removed " << removed << " items" << std::endl;
        std::cout << "Memory size after cleanup: " << working_memory->getCurrentSize() << std::endl;
        
        // Validate memory consistency
        if (working_memory->validateMemoryConsistency()) {
            std::cout << "\n✓ Memory consistency validated successfully" << std::endl;
        } else {
            std::cout << "\n✗ Memory consistency validation failed" << std::endl;
        }
        
        // Display detailed information about a specific item
        std::cout << "\nDetailed item information:" << std::endl;
        std::cout << working_memory->getItemInfo(robot) << std::endl;
        
        std::cout << "\n=== Example completed successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}