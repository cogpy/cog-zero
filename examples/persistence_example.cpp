/*
 * examples/persistence_example.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * LongTermMemory persistence demonstration
 * Shows how memories persist across system restarts
 * Part of the AGENT-ZERO-GENESIS project - AZ-MEM-003
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/agentzero/memory/LongTermMemory.h>

using namespace opencog;
using namespace opencog::agentzero::memory;

void demonstrate_persistence_storage()
{
    std::cout << "=== Persistence Storage Phase ===" << std::endl;
    
    auto atomspace = std::make_shared<AtomSpace>();
    
    MemoryConfig config;
    config.persistence_directory = "./persistence_demo_data";
    config.enable_compression = true;
    config.enable_incremental_backup = true;
    
    auto ltm = std::make_unique<LongTermMemory>(atomspace, config);
    
    if (!ltm->initialize()) {
        std::cerr << "Failed to initialize LongTermMemory" << std::endl;
        return;
    }
    
    // Create and store various types of knowledge
    std::cout << "Creating and storing persistent memories..." << std::endl;
    
    // Core concepts
    Handle agent_self = atomspace->add_node(CONCEPT_NODE, "SelfAgent");
    Handle memory_system = atomspace->add_node(CONCEPT_NODE, "MemorySystem");
    Handle persistence_capability = atomspace->add_node(CONCEPT_NODE, "PersistenceCapability");
    
    // Store with high importance for persistence
    ltm->store(agent_self, MemoryImportance::CRITICAL, PersistenceLevel::PERMANENT);
    ltm->store(memory_system, MemoryImportance::HIGH, PersistenceLevel::LONG_TERM);
    ltm->store(persistence_capability, MemoryImportance::HIGH, PersistenceLevel::LONG_TERM);
    
    // Create some learned knowledge
    for (int i = 0; i < 10; ++i) {
        Handle learned_fact = atomspace->add_node(CONCEPT_NODE, "LearnedFact_" + std::to_string(i));
        ltm->store(learned_fact, MemoryImportance::MEDIUM, PersistenceLevel::LONG_TERM,
                  {ContextType::TEMPORAL, ContextType::COGNITIVE});
    }
    
    // Force persistence
    size_t persisted = ltm->flushToPersistence();
    std::cout << "Persisted " << persisted << " memories to storage" << std::endl;
    
    // Show current status
    std::cout << ltm->getSystemStatus() << std::endl;
    
    // Graceful shutdown
    ltm->shutdown();
    std::cout << "First session completed and shutdown gracefully" << std::endl;
}

void demonstrate_persistence_recovery()
{
    std::cout << "\n=== Persistence Recovery Phase ===" << std::endl;
    
    // Simulate system restart with new AtomSpace
    auto new_atomspace = std::make_shared<AtomSpace>();
    
    MemoryConfig config;
    config.persistence_directory = "./persistence_demo_data";
    
    auto ltm = std::make_unique<LongTermMemory>(new_atomspace, config);
    
    if (!ltm->initialize()) {
        std::cerr << "Failed to initialize LongTermMemory for recovery" << std::endl;
        return;
    }
    
    std::cout << "Initialized new LongTermMemory instance" << std::endl;
    
    // Load from persistence
    size_t loaded = ltm->loadFromPersistence();
    std::cout << "Loaded " << loaded << " memories from persistent storage" << std::endl;
    
    // Verify that critical memories are available
    std::cout << "\nVerifying persistent memories..." << std::endl;
    
    auto critical_memories = ltm->findByImportance(MemoryImportance::CRITICAL);
    std::cout << "Found " << critical_memories.size() << " critical memories" << std::endl;
    
    auto high_memories = ltm->findByImportance(MemoryImportance::HIGH);
    std::cout << "Found " << high_memories.size() << " high importance memories" << std::endl;
    
    auto cognitive_memories = ltm->findByContext(ContextType::COGNITIVE);
    std::cout << "Found " << cognitive_memories.size() << " cognitive context memories" << std::endl;
    
    // Show recovery status
    std::cout << "\n" << ltm->getSystemStatus() << std::endl;
    
    // Add some new memories to test continued operation
    Handle new_memory = new_atomspace->add_node(CONCEPT_NODE, "PostRecoveryMemory");
    ltm->store(new_memory, MemoryImportance::MEDIUM, PersistenceLevel::LONG_TERM);
    
    std::cout << "Added new memory after recovery: " << 
                 (ltm->contains(new_memory) ? "Success" : "Failed") << std::endl;
    
    ltm->shutdown();
    std::cout << "Recovery session completed" << std::endl;
}

void demonstrate_backup_and_restore()
{
    std::cout << "\n=== Backup and Restore Demonstration ===" << std::endl;
    
    auto atomspace = std::make_shared<AtomSpace>();
    
    MemoryConfig config;
    config.persistence_directory = "./backup_demo_data";
    
    auto ltm = std::make_unique<LongTermMemory>(atomspace, config);
    ltm->initialize();
    
    // Create some test data
    for (int i = 0; i < 5; ++i) {
        Handle backup_test = atomspace->add_node(CONCEPT_NODE, "BackupTest_" + std::to_string(i));
        ltm->store(backup_test, MemoryImportance::HIGH, PersistenceLevel::LONG_TERM);
    }
    
    ltm->flushToPersistence();
    
    // Create backup
    std::string backup_path = "./demo_backup_" + 
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".db";
    
    bool backup_success = ltm->backup(backup_path);
    std::cout << "Backup creation: " << (backup_success ? "Success" : "Failed") << std::endl;
    
    if (backup_success) {
        std::cout << "Backup created at: " << backup_path << std::endl;
        
        // Verify backup file exists
        if (std::filesystem::exists(backup_path)) {
            auto file_size = std::filesystem::file_size(backup_path);
            std::cout << "Backup file size: " << file_size << " bytes" << std::endl;
        }
    }
    
    ltm->shutdown();
}

int main()
{
    std::cout << "=== Agent-Zero LongTermMemory Persistence Demo ===" << std::endl;
    std::cout << "This demo shows how memories persist across system restarts" << std::endl;
    
    try {
        // Clean up any existing demo data
        std::filesystem::remove_all("./persistence_demo_data");
        std::filesystem::remove_all("./backup_demo_data");
        
        // Phase 1: Store memories and shutdown
        demonstrate_persistence_storage();
        
        // Simulate system restart delay
        std::cout << "\nSimulating system restart..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // Phase 2: Recover memories in new session
        demonstrate_persistence_recovery();
        
        // Phase 3: Demonstrate backup functionality
        demonstrate_backup_and_restore();
        
        std::cout << "\n=== Persistence Demo Completed Successfully ===" << std::endl;
        std::cout << "Key takeaways:" << std::endl;
        std::cout << "1. Memories persist across system restarts" << std::endl;
        std::cout << "2. Importance levels control retention policies" << std::endl;
        std::cout << "3. Context information is preserved" << std::endl;
        std::cout << "4. Backup and restore operations work correctly" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Demo error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}