/*
 * examples/dialogue_manager_example.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Example demonstrating DialogueManager usage
 * Part of the AGENT-ZERO-GENESIS project Phase 6: Communication & NLP
 */

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>
#include "opencog/agentzero/DialogueManager.h"

using namespace opencog;
using namespace opencog::agentzero;

void demonstrateBasicConversation() {
    std::cout << "\n=== Basic Conversation Demonstration ===" << std::endl;
    
    // Create AtomSpace and DialogueManager
    AtomSpacePtr atomspace = createAtomSpace();
    DialogueManager dialogue_manager(atomspace, "ExampleAgent");
    
    // Start a conversation
    std::vector<std::string> participants = {"ExampleAgent", "Human"};
    bool started = dialogue_manager.startConversation("demo_conv", participants);
    
    if (started) {
        std::cout << "✓ Started conversation 'demo_conv'" << std::endl;
    } else {
        std::cout << "✗ Failed to start conversation" << std::endl;
        return;
    }
    
    // Set conversation topic
    dialogue_manager.setConversationTopic("demo_conv", "AI and Cognitive Architecture");
    std::cout << "✓ Set topic: " << dialogue_manager.getConversationTopic("demo_conv") << std::endl;
    
    // Simulate conversation exchanges
    std::vector<std::string> user_messages = {
        "Hello, can you tell me about cognitive architectures?",
        "What makes OpenCog different from other AI systems?",
        "How does the AtomSpace work?",
        "Thank you for the information!"
    };
    
    for (const auto& message : user_messages) {
        std::cout << "\nHuman: " << message << std::endl;
        
        std::string response = dialogue_manager.processMessage("demo_conv", "Human", message);
        std::cout << "ExampleAgent: " << response << std::endl;
    }
    
    // Show conversation history
    std::cout << "\n--- Conversation History ---" << std::endl;
    auto history = dialogue_manager.getConversationHistory("demo_conv");
    for (size_t i = 0; i < history.size(); ++i) {
        const auto& msg = history[i];
        std::cout << "[" << (i + 1) << "] " << msg.sender_id << ": " << msg.content << std::endl;
    }
    
    // End conversation
    dialogue_manager.endConversation("demo_conv");
    std::cout << "\n✓ Conversation ended" << std::endl;
}

void demonstrateMultipleConversations() {
    std::cout << "\n=== Multiple Conversations Demonstration ===" << std::endl;
    
    AtomSpacePtr atomspace = createAtomSpace();
    DialogueManager dialogue_manager(atomspace, "MultiTaskAgent");
    
    // Start multiple conversations
    std::vector<std::string> participants1 = {"MultiTaskAgent", "Alice"};
    std::vector<std::string> participants2 = {"MultiTaskAgent", "Bob"};
    std::vector<std::string> participants3 = {"MultiTaskAgent", "Charlie"};
    
    dialogue_manager.startConversation("conv_alice", participants1);
    dialogue_manager.startConversation("conv_bob", participants2);
    dialogue_manager.startConversation("conv_charlie", participants3);
    
    std::cout << "✓ Started 3 conversations" << std::endl;
    
    // Set different topics
    dialogue_manager.setConversationTopic("conv_alice", "Machine Learning");
    dialogue_manager.setConversationTopic("conv_bob", "Natural Language Processing");
    dialogue_manager.setConversationTopic("conv_charlie", "Robotics");
    
    // Set context for each conversation
    dialogue_manager.setConversationContext("conv_alice", "skill_level", "expert");
    dialogue_manager.setConversationContext("conv_bob", "skill_level", "beginner");
    dialogue_manager.setConversationContext("conv_charlie", "skill_level", "intermediate");
    
    // Simulate interleaved messages
    dialogue_manager.processMessage("conv_alice", "Alice", "What's the latest in deep learning?");
    dialogue_manager.processMessage("conv_bob", "Bob", "What is NLP?");
    dialogue_manager.processMessage("conv_charlie", "Charlie", "How do robots perceive the world?");
    
    // Show active conversations
    auto active_convs = dialogue_manager.getActiveConversations();
    std::cout << "\nActive conversations: " << active_convs.size() << std::endl;
    
    for (const auto& conv_id : active_convs) {
        std::cout << "- " << conv_id 
                  << " (topic: " << dialogue_manager.getConversationTopic(conv_id) << ")"
                  << std::endl;
    }
    
    // Show status information
    std::cout << "\n--- Status Information ---" << std::endl;
    std::cout << dialogue_manager.getStatusInfo() << std::endl;
}

void demonstrateContextTracking() {
    std::cout << "\n=== Context Tracking Demonstration ===" << std::endl;
    
    AtomSpacePtr atomspace = createAtomSpace();
    DialogueManager dialogue_manager(atomspace, "ContextAgent");
    
    std::vector<std::string> participants = {"ContextAgent", "User"};
    dialogue_manager.startConversation("context_demo", participants);
    
    // Enable context tracking and goal-oriented dialogue
    dialogue_manager.setContextTracking(true);
    dialogue_manager.setGoalOrientedDialogue(true);
    
    // Add a goal to the conversation
    Handle goal_atom = atomspace->add_node(CONCEPT_NODE, "ProvideHelpfulInformation");
    dialogue_manager.addConversationGoal("context_demo", goal_atom);
    
    std::cout << "✓ Added goal: Provide Helpful Information" << std::endl;
    
    // Set various context variables
    dialogue_manager.setConversationContext("context_demo", "user_preference", "technical_details");
    dialogue_manager.setConversationContext("context_demo", "session_type", "educational");
    dialogue_manager.setConversationContext("context_demo", "previous_knowledge", "basic_programming");
    
    std::cout << "✓ Set context variables:" << std::endl;
    std::cout << "  - User preference: " << dialogue_manager.getConversationContext("context_demo", "user_preference") << std::endl;
    std::cout << "  - Session type: " << dialogue_manager.getConversationContext("context_demo", "session_type") << std::endl;
    std::cout << "  - Previous knowledge: " << dialogue_manager.getConversationContext("context_demo", "previous_knowledge") << std::endl;
    
    // Demonstrate context-aware responses
    std::string response1 = dialogue_manager.processMessage("context_demo", "User", 
        "Can you explain how OpenCog processes information?");
    std::cout << "\nUser: Can you explain how OpenCog processes information?" << std::endl;
    std::cout << "ContextAgent: " << response1 << std::endl;
    
    // Change context and see different response style
    dialogue_manager.setConversationContext("context_demo", "user_preference", "simple_explanations");
    
    std::string response2 = dialogue_manager.processMessage("context_demo", "User", 
        "How does machine learning work in OpenCog?");
    std::cout << "\nUser: How does machine learning work in OpenCog?" << std::endl;
    std::cout << "ContextAgent: " << response2 << std::endl;
    
    // Show conversation goals
    auto goals = dialogue_manager.getConversationGoals("context_demo");
    std::cout << "\nActive goals: " << goals.size() << std::endl;
    for (const auto& goal : goals) {
        std::cout << "  - " << goal->get_name() << std::endl;
    }
}

void demonstrateAtomSpaceIntegration() {
    std::cout << "\n=== AtomSpace Integration Demonstration ===" << std::endl;
    
    AtomSpacePtr atomspace = createAtomSpace();
    DialogueManager dialogue_manager(atomspace, "AtomSpaceAgent");
    
    std::vector<std::string> participants = {"AtomSpaceAgent", "Researcher"};
    dialogue_manager.startConversation("atomspace_demo", participants);
    
    // Process some messages to create AtomSpace content
    dialogue_manager.processMessage("atomspace_demo", "Researcher", 
        "What are atoms in OpenCog?");
    dialogue_manager.processMessage("atomspace_demo", "Researcher", 
        "How do links connect atoms?");
    
    // Update dialogue atoms in AtomSpace
    dialogue_manager.updateDialogueAtoms();
    
    std::cout << "✓ Updated AtomSpace with dialogue information" << std::endl;
    
    // Show AtomSpace content related to dialogue
    std::cout << "\n--- AtomSpace Dialogue Content ---" << std::endl;
    
    HandleSeq all_atoms = atomspace->get_handles_by_type(CONCEPT_NODE);
    std::cout << "Total atoms in AtomSpace: " << all_atoms.size() << std::endl;
    
    // Find dialogue-related atoms
    std::vector<Handle> dialogue_atoms;
    for (const Handle& atom : all_atoms) {
        std::string name = atom->get_name();
        if (name.find("Conversation:") == 0 || 
            name.find("Message:") == 0 ||
            name.find("DialogueContext") == 0 ||
            name == "AtomSpaceAgent") {
            dialogue_atoms.push_back(atom);
        }
    }
    
    std::cout << "Dialogue-related atoms: " << dialogue_atoms.size() << std::endl;
    for (const auto& atom : dialogue_atoms) {
        std::cout << "  - " << atom->get_name() << " (" << nameserver().getTypeName(atom->get_type()) << ")" << std::endl;
    }
    
    // Convert conversation to atom
    Handle conv_atom = dialogue_manager.conversationToAtom("atomspace_demo");
    std::cout << "\nConversation atom: " << conv_atom->get_name() << std::endl;
    
    // Show atom relationships
    HandleSeq incoming = conv_atom->getIncomingSet();
    std::cout << "Conversation atom has " << incoming.size() << " incoming relationships" << std::endl;
}

int main() {
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    std::cout << "DialogueManager Example - OpenCog Agent-Zero Communication Module" << std::endl;
    std::cout << "=================================================================" << std::endl;
    
    try {
        // Run demonstrations
        demonstrateBasicConversation();
        demonstrateMultipleConversations();
        demonstrateContextTracking();
        demonstrateAtomSpaceIntegration();
        
        std::cout << "\n=== Example Complete ===" << std::endl;
        std::cout << "All demonstrations completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error during example execution: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}