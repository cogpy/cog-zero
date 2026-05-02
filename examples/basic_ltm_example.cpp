/*
 * examples/basic_ltm_example.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Basic LongTermMemory usage example
 * Part of the AGENT-ZERO-GENESIS project - AZ-MEM-003
 */

#include <iostream>
#include <memory>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/agentzero/memory/LongTermMemory.h>

using namespace opencog;
using namespace opencog::agentzero::memory;

int main()
{
    std::cout << "=== Agent-Zero LongTermMemory Basic Example ===" << std::endl;
    
    try {
        // Create AtomSpace
        auto atomspace = std::make_shared<AtomSpace>();
        std::cout << "Created AtomSpace" << std::endl;
        
        // Configure memory with custom settings
        MemoryConfig config;
        config.persistence_directory = "./example_memory_data";
        config.max_memory_count = 10000;
        config.min_retention_importance = MemoryImportance::LOW;
        config.consolidation_strategy = ConsolidationStrategy::HYBRID;
        
        // Create LongTermMemory instance
        auto ltm = std::make_unique<LongTermMemory>(atomspace, config);
        std::cout << "Created LongTermMemory instance" << std::endl;
        
        // Initialize the memory system
        if (!ltm->initialize()) {
            std::cerr << "Failed to initialize LongTermMemory" << std::endl;
            return 1;
        }
        std::cout << "Initialized LongTermMemory" << std::endl;
        
        // Create some test atoms
        Handle agent_concept = atomspace->add_node(CONCEPT_NODE, "Agent");
        Handle knowledge_concept = atomspace->add_node(CONCEPT_NODE, "Knowledge");
        Handle learning_concept = atomspace->add_node(CONCEPT_NODE, "Learning");
        
        // Create a relationship
        Handle learning_relation = atomspace->add_link(INHERITANCE_LINK, {
            learning_concept, knowledge_concept
        });
        
        std::cout << "Created test atoms" << std::endl;
        
        // Store atoms with different importance levels
        std::cout << "\n--- Storing Memories ---" << std::endl;
        
        bool stored_agent = ltm->store(agent_concept, MemoryImportance::CRITICAL,
                                      PersistenceLevel::PERMANENT,
                                      {ContextType::COGNITIVE});
        std::cout << "Stored agent concept: " << (stored_agent ? "Success" : "Failed") << std::endl;
        
        bool stored_knowledge = ltm->store(knowledge_concept, MemoryImportance::HIGH,
                                          PersistenceLevel::LONG_TERM,
                                          {ContextType::COGNITIVE, ContextType::TEMPORAL});
        std::cout << "Stored knowledge concept: " << (stored_knowledge ? "Success" : "Failed") << std::endl;
        
        bool stored_learning = ltm->store(learning_concept, MemoryImportance::MEDIUM,
                                         PersistenceLevel::MEDIUM_TERM,
                                         {ContextType::TEMPORAL});
        std::cout << "Stored learning concept: " << (stored_learning ? "Success" : "Failed") << std::endl;
        
        bool stored_relation = ltm->store(learning_relation, MemoryImportance::HIGH,
                                         PersistenceLevel::LONG_TERM,
                                         {ContextType::COGNITIVE});
        std::cout << "Stored learning relation: " << (stored_relation ? "Success" : "Failed") << std::endl;
        
        // Retrieve memories
        std::cout << "\n--- Retrieving Memories ---" << std::endl;
        
        Handle retrieved_agent = ltm->retrieve(agent_concept);
        std::cout << "Retrieved agent concept: " << 
                     (retrieved_agent != Handle::UNDEFINED ? "Found" : "Not found") << std::endl;
        
        // Check containment
        std::cout << "Contains knowledge concept: " << 
                     (ltm->contains(knowledge_concept) ? "Yes" : "No") << std::endl;
        
        // Find memories by importance
        std::cout << "\n--- Finding by Importance ---" << std::endl;
        
        auto high_importance = ltm->findByImportance(MemoryImportance::HIGH, 10);
        std::cout << "High importance memories found: " << high_importance.size() << std::endl;
        
        auto critical_importance = ltm->findByImportance(MemoryImportance::CRITICAL, 10);
        std::cout << "Critical importance memories found: " << critical_importance.size() << std::endl;
        
        // Find memories by context
        std::cout << "\n--- Finding by Context ---" << std::endl;
        
        auto cognitive_memories = ltm->findByContext(ContextType::COGNITIVE, 10);
        std::cout << "Cognitive context memories found: " << cognitive_memories.size() << std::endl;
        
        auto temporal_memories = ltm->findByContext(ContextType::TEMPORAL, 10);
        std::cout << "Temporal context memories found: " << temporal_memories.size() << std::endl;
        
        // Display system status
        std::cout << "\n--- System Status ---" << std::endl;
        std::cout << ltm->getSystemStatus() << std::endl;
        
        // Display statistics
        std::cout << "\n--- Memory Statistics ---" << std::endl;
        auto stats = ltm->getStatistics();
        std::cout << "Total read operations: " << stats.read_operations << std::endl;
        std::cout << "Total write operations: " << stats.write_operations << std::endl;
        std::cout << "Cache hits: " << stats.cache_hits << std::endl;
        std::cout << "Cache misses: " << stats.cache_misses << std::endl;
        
        // Trigger consolidation
        std::cout << "\n--- Memory Consolidation ---" << std::endl;
        auto consolidation_status = ltm->consolidate(true);
        std::cout << "Total memories: " << consolidation_status.total_memories << std::endl;
        std::cout << "Consolidated memories: " << consolidation_status.consolidated_memories << std::endl;
        std::cout << "Removed memories: " << consolidation_status.removed_memories << std::endl;
        
        // Force persistence
        std::cout << "\n--- Forcing Persistence ---" << std::endl;
        size_t persisted_count = ltm->flushToPersistence();
        std::cout << "Persisted " << persisted_count << " memories to storage" << std::endl;
        
        // Create backup
        std::cout << "\n--- Creating Backup ---" << std::endl;
        bool backup_success = ltm->backup("./example_backup.db");
        std::cout << "Backup creation: " << (backup_success ? "Success" : "Failed") << std::endl;
        
        // Shutdown gracefully
        std::cout << "\n--- Shutting Down ---" << std::endl;
        bool shutdown_success = ltm->shutdown();
        std::cout << "Shutdown: " << (shutdown_success ? "Success" : "Failed") << std::endl;
        
        std::cout << "\n=== Example Completed Successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}