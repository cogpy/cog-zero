/*
 * basic_communication_example.cpp
 *
 * Basic example demonstrating Agent-Zero communication protocols
 * Part of the AGENT-ZERO-GENESIS project - AZ-COMM-001
 */

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

#include <opencog/agentzero/communication/AgentComms.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/util/Logger.h>

using namespace opencog;
using namespace opencog::agentzero::communication;

int main() {
    std::cout << "=== Agent-Zero Communication Example ===" << std::endl;
    
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    try {
        // Create AtomSpace
        auto atomspace = createAtomSpace();
        std::cout << "Created AtomSpace" << std::endl;
        
        // Create two agents for communication
        AgentId agent1("Alice", "instance1");
        AgentId agent2("Bob", "instance2");
        
        // Configure communication
        CommConfig config;
        config.enable_network = false;      // Disable network for this example
        config.enable_persistence = true;   // Enable AtomSpace persistence
        config.max_message_size = 1024 * 1024;  // 1MB max message size
        config.message_timeout = std::chrono::seconds(30);
        
        // Create communication instances
        auto alice_comms = std::make_unique<AgentComms>(agent1, config, atomspace);
        auto bob_comms = std::make_unique<AgentComms>(agent2, config, atomspace);
        
        std::cout << "Created communication instances for Alice and Bob" << std::endl;
        
        // Set up message handlers
        bool alice_received_message = false;
        bool bob_received_message = false;
        
        alice_comms->registerMessageHandler(MessageType::INFO, 
            [&](const CommMessagePtr& msg) -> bool {
                std::cout << "Alice received message from " << msg->sender.toString() 
                         << ": " << msg->content << std::endl;
                alice_received_message = true;
                return true;
            });
        
        bob_comms->registerMessageHandler(MessageType::RESPONSE,
            [&](const CommMessagePtr& msg) -> bool {
                std::cout << "Bob received response from " << msg->sender.toString()
                         << ": " << msg->content << std::endl;
                bob_received_message = true;
                return true;
            });
        
        // Start communication systems
        if (!alice_comms->start()) {
            std::cerr << "Failed to start Alice's communication system" << std::endl;
            return 1;
        }
        
        if (!bob_comms->start()) {
            std::cerr << "Failed to start Bob's communication system" << std::endl;
            return 1;
        }
        
        std::cout << "Started communication systems" << std::endl;
        
        // Alice sends a message to Bob
        std::cout << "\n--- Sending Text Message ---" << std::endl;
        std::string msg_id1 = alice_comms->sendMessage(
            agent2,
            MessageType::INFO,
            "Hello Bob, this is Alice!",
            MessagePriority::NORMAL,
            ProtocolType::LOCAL
        );
        
        if (msg_id1.empty()) {
            std::cerr << "Failed to send message from Alice to Bob" << std::endl;
        } else {
            std::cout << "Alice sent message to Bob (ID: " << msg_id1 << ")" << std::endl;
        }
        
        // Bob sends a response to Alice
        std::cout << "\n--- Sending Response Message ---" << std::endl;
        std::string msg_id2 = bob_comms->sendMessage(
            agent1,
            MessageType::RESPONSE,
            "Hi Alice, Bob here!",
            MessagePriority::HIGH,
            ProtocolType::LOCAL
        );
        
        if (msg_id2.empty()) {
            std::cerr << "Failed to send response from Bob to Alice" << std::endl;
        } else {
            std::cout << "Bob sent response to Alice (ID: " << msg_id2 << ")" << std::endl;
        }
        
        // Create and send AtomSpace message
        std::cout << "\n--- Sending AtomSpace Message ---" << std::endl;
        Handle knowledge_atom = atomspace->add_node(CONCEPT_NODE, "SharedKnowledge");
        atomspace->add_link(EVALUATION_LINK, {
            atomspace->add_node(PREDICATE_NODE, "example_fact"),
            atomspace->add_link(LIST_LINK, {
                atomspace->add_node(CONCEPT_NODE, "communication"),
                atomspace->add_node(CONCEPT_NODE, "successful")
            })
        });
        
        std::string msg_id3 = alice_comms->sendAtomMessage(
            agent2,
            MessageType::KNOWLEDGE_SHARE,
            knowledge_atom,
            MessagePriority::NORMAL,
            ProtocolType::LOCAL
        );
        
        if (msg_id3.empty()) {
            std::cerr << "Failed to send AtomSpace message" << std::endl;
        } else {
            std::cout << "Alice sent AtomSpace message (ID: " << msg_id3 << ")" << std::endl;
        }
        
        // Broadcast message example
        std::cout << "\n--- Broadcasting Message ---" << std::endl;
        std::vector<AgentId> recipients = {agent2, AgentId("Charlie"), AgentId("David")};
        size_t broadcast_count = alice_comms->broadcastMessage(
            recipients,
            MessageType::NOTIFICATION,
            "This is a broadcast message from Alice",
            MessagePriority::LOW
        );
        
        std::cout << "Alice broadcasted message to " << broadcast_count << " recipients" << std::endl;
        
        // Wait a bit for message processing (in a real system, messages would be processed asynchronously)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Display statistics
        std::cout << "\n--- Communication Statistics ---" << std::endl;
        
        std::cout << "Alice's stats:" << std::endl;
        CommStats alice_stats = alice_comms->getStats();
        std::cout << "  Messages sent: " << alice_stats.messages_sent << std::endl;
        std::cout << "  Messages received: " << alice_stats.messages_received << std::endl;
        std::cout << "  Bytes sent: " << alice_stats.bytes_sent << std::endl;
        
        std::cout << "Bob's stats:" << std::endl;
        CommStats bob_stats = bob_comms->getStats();
        std::cout << "  Messages sent: " << bob_stats.messages_sent << std::endl;
        std::cout << "  Messages received: " << bob_stats.messages_received << std::endl;
        std::cout << "  Bytes sent: " << bob_stats.bytes_sent << std::endl;
        
        // Display status information
        std::cout << "\n--- Status Information ---" << std::endl;
        std::cout << "Alice status: " << alice_comms->getStatusInfo() << std::endl;
        std::cout << "Bob status: " << bob_comms->getStatusInfo() << std::endl;
        
        // Test configuration update
        std::cout << "\n--- Updating Configuration ---" << std::endl;
        CommConfig new_config = alice_comms->getConfig();
        new_config.max_message_size = 2048 * 1024;  // 2MB
        new_config.message_timeout = std::chrono::seconds(60);
        
        if (alice_comms->updateConfig(new_config)) {
            std::cout << "Successfully updated Alice's configuration" << std::endl;
            std::cout << "New max message size: " << new_config.max_message_size << " bytes" << std::endl;
        }
        
        // Clean shutdown
        std::cout << "\n--- Shutting Down ---" << std::endl;
        alice_comms->stop();
        bob_comms->stop();
        
        std::cout << "Communication systems stopped" << std::endl;
        std::cout << "Example completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error in communication example: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}