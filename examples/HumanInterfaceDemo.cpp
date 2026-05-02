/**
 * HumanInterfaceDemo.cpp - Demonstration of HumanInterface capabilities
 * 
 * Part of AZ-HUMAN-001: Create HumanInterface layer
 * Shows basic usage patterns and capabilities of the HumanInterface
 * 
 * Copyright (C) 2024 OpenCog Foundation
 */

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>

#include <agentzero/communication/HumanInterface.h>
#include <agentzero/communication/CommunicationUtils.h>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

using namespace opencog;
using namespace opencog::agentzero::communication;

/**
 * Demonstrate basic HumanInterface initialization and setup
 */
void demonstrateInitialization() {
    std::cout << "\n=== HumanInterface Initialization Demo ===" << std::endl;
    
    // Create AtomSpace for knowledge representation
    auto atomspace = std::make_shared<AtomSpace>();
    std::cout << "Created AtomSpace with " << atomspace->get_size() << " atoms" << std::endl;
    
    // Configure the communication system
    CommunicationConfig config;
    config.enable_input_preprocessing = true;
    config.enable_context_awareness = true;
    config.max_concurrent_sessions = 10;
    config.default_session_timeout = std::chrono::minutes(30);
    
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Input preprocessing: " << (config.enable_input_preprocessing ? "enabled" : "disabled") << std::endl;
    std::cout << "  Context awareness: " << (config.enable_context_awareness ? "enabled" : "disabled") << std::endl;
    std::cout << "  Max sessions: " << config.max_concurrent_sessions << std::endl;
    
    // Create and initialize HumanInterface
    HumanInterface interface(atomspace, config);
    
    if (interface.initialize()) {
        std::cout << "✓ HumanInterface initialized successfully" << std::endl;
        
        auto status = interface.getStatus();
        std::cout << "Status: " << (status.healthy ? "healthy" : "unhealthy") << std::endl;
        std::cout << "Active sessions: " << status.active_sessions << std::endl;
    } else {
        std::cout << "✗ Failed to initialize HumanInterface" << std::endl;
    }
}

/**
 * Demonstrate basic conversation capabilities
 */
void demonstrateBasicConversation() {
    std::cout << "\n=== Basic Conversation Demo ===" << std::endl;
    
    auto atomspace = std::make_shared<AtomSpace>();
    HumanInterface interface(atomspace);
    interface.initialize();
    
    // Start a session
    std::string user_id = "demo_user_001";
    std::string session_id = interface.startSession(user_id);
    std::cout << "Started session: " << session_id << std::endl;
    
    // Test different types of input
    std::vector<std::string> test_inputs = {
        "Hello!",
        "How are you doing today?",
        "Can you help me with something?",
        "What can you tell me about artificial intelligence?",
        "Thank you for your help"
    };
    
    for (const auto& input_text : test_inputs) {
        std::cout << "\n👤 User: " << input_text << std::endl;
        
        // Create input
        HumanInput input(input_text);
        input.user_id = user_id;
        
        // Process and get response
        auto response = interface.processInput(input, session_id);
        
        if (response.success) {
            std::cout << "🤖 Agent: " << response.agent_response.content << std::endl;
            std::cout << "   Confidence: " << std::fixed << std::setprecision(2) 
                     << response.agent_response.confidence << std::endl;
            std::cout << "   Processing time: " << response.processing_time.count() << "ms" << std::endl;
            
            if (!response.agent_response.suggestions.empty()) {
                std::cout << "   Suggestions:" << std::endl;
                for (const auto& suggestion : response.agent_response.suggestions) {
                    std::cout << "   • " << suggestion << std::endl;
                }
            }
        } else {
            std::cout << "✗ Error: " << response.error_message << std::endl;
        }
        
        // Brief pause for realism
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // End session
    interface.endSession(session_id);
    std::cout << "\nSession ended" << std::endl;
}

/**
 * Demonstrate context management capabilities
 */
void demonstrateContextManagement() {
    std::cout << "\n=== Context Management Demo ===" << std::endl;
    
    auto atomspace = std::make_shared<AtomSpace>();
    HumanInterface interface(atomspace);
    interface.initialize();
    
    std::string session_id = interface.startSession("context_demo_user");
    
    // Set up initial context
    ContextUpdate initial_context(ContextUpdate::UpdateType::SET, "user_name", "Alice");
    interface.updateContext(session_id, initial_context);
    
    ContextUpdate topic_context(ContextUpdate::UpdateType::SET, "current_topic", "machine_learning");
    interface.updateContext(session_id, topic_context);
    
    std::cout << "Set initial context: user_name=Alice, current_topic=machine_learning" << std::endl;
    
    // Demonstrate context-aware interaction
    HumanInput input("Tell me more about this topic");
    input.user_id = "context_demo_user";
    
    auto response = interface.processInput(input, session_id);
    std::cout << "👤 User: " << input.raw.content << std::endl;
    std::cout << "🤖 Agent: " << response.agent_response.content << std::endl;
    
    // Show current context
    auto context = interface.getContext(session_id);
    std::cout << "\nCurrent context variables:" << std::endl;
    for (const auto& pair : context.variables) {
        std::cout << "  " << pair.first << " = " << pair.second << std::endl;
    }
    
    // Update context during conversation
    ContextUpdate new_topic(ContextUpdate::UpdateType::SET, "current_topic", "neural_networks");
    interface.updateContext(session_id, new_topic);
    std::cout << "\nUpdated topic to: neural_networks" << std::endl;
    
    // Continue conversation with new context
    HumanInput followup("What are the key concepts I should know?");
    followup.user_id = "context_demo_user";
    
    response = interface.processInput(followup, session_id);
    std::cout << "👤 User: " << followup.raw.content << std::endl;
    std::cout << "🤖 Agent: " << response.agent_response.content << std::endl;
    
    interface.endSession(session_id);
}

/**
 * Demonstrate multi-session handling
 */
void demonstrateMultiSession() {
    std::cout << "\n=== Multi-Session Demo ===" << std::endl;
    
    auto atomspace = std::make_shared<AtomSpace>();
    HumanInterface interface(atomspace);
    interface.initialize();
    
    // Create multiple concurrent sessions
    std::vector<std::string> session_ids;
    std::vector<std::string> user_ids = {"user_A", "user_B", "user_C"};
    
    for (const auto& user_id : user_ids) {
        std::string session_id = interface.startSession(user_id);
        session_ids.push_back(session_id);
        std::cout << "Started session for " << user_id << ": " << session_id << std::endl;
    }
    
    std::cout << "Active sessions: " << interface.getActiveSessionCount() << std::endl;
    
    // Simulate concurrent interactions
    for (size_t i = 0; i < session_ids.size(); ++i) {
        HumanInput input("Hello from " + user_ids[i]);
        input.user_id = user_ids[i];
        
        auto response = interface.processInput(input, session_ids[i]);
        std::cout << "Session " << (i+1) << " - Response: " << response.agent_response.content << std::endl;
    }
    
    // Clean up sessions
    for (const auto& session_id : session_ids) {
        interface.endSession(session_id);
    }
    
    std::cout << "All sessions ended. Active sessions: " << interface.getActiveSessionCount() << std::endl;
}

/**
 * Demonstrate analytics and monitoring
 */
void demonstrateAnalytics() {
    std::cout << "\n=== Analytics and Monitoring Demo ===" << std::endl;
    
    auto atomspace = std::make_shared<AtomSpace>();
    HumanInterface interface(atomspace);
    interface.initialize();
    
    std::string session_id = interface.startSession("analytics_user");
    
    // Generate some interaction data
    std::vector<std::string> inputs = {
        "Hello there",
        "What is machine learning?",
        "How does deep learning work?",
        "Can you help me understand neural networks?",
        "Thanks for the information"
    };
    
    std::cout << "Processing " << inputs.size() << " interactions..." << std::endl;
    
    for (const auto& input_text : inputs) {
        HumanInput input(input_text);
        input.user_id = "analytics_user";
        interface.processInput(input, session_id);
    }
    
    // Get analytics
    auto analytics = interface.getAnalytics();
    
    std::cout << "\n--- Analytics Report ---" << std::endl;
    std::cout << "Total interactions: " << analytics.total_interactions << std::endl;
    std::cout << "Average response time: " << analytics.average_response_time.count() << "ms" << std::endl;
    std::cout << "Success rate: " << std::fixed << std::setprecision(1) 
             << (analytics.success_rate * 100) << "%" << std::endl;
    std::cout << "Average confidence: " << std::fixed << std::setprecision(2) 
             << analytics.average_confidence << std::endl;
    
    std::cout << "\nInput type distribution:" << std::endl;
    for (const auto& pair : analytics.input_type_counts) {
        std::cout << "  " << utils::inputTypeToString(pair.first) << ": " << pair.second << std::endl;
    }
    
    std::cout << "\nIntent distribution:" << std::endl;
    for (const auto& pair : analytics.intent_counts) {
        std::cout << "  " << pair.first << ": " << pair.second << std::endl;
    }
    
    interface.endSession(session_id);
}

/**
 * Demonstrate utility functions
 */
void demonstrateUtilities() {
    std::cout << "\n=== Utility Functions Demo ===" << std::endl;
    
    // Text processing utilities
    std::string sample_text = "  Hello, World!   This is a TEST.  ";
    std::cout << "Original text: '" << sample_text << "'" << std::endl;
    std::cout << "Normalized: '" << utils::text::normalize(sample_text) << "'" << std::endl;
    
    auto tokens = utils::text::tokenize(sample_text);
    std::cout << "Tokens: ";
    for (const auto& token : tokens) {
        std::cout << "'" << token << "' ";
    }
    std::cout << std::endl;
    
    // Session utilities
    std::string session_id = utils::session::generateSessionId("demo_user", "example");
    std::cout << "Generated session ID: " << session_id << std::endl;
    std::cout << "Is valid: " << (utils::session::isValidSessionId(session_id) ? "yes" : "no") << std::endl;
    
    // Performance utilities
    utils::performance::Timer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto elapsed = timer.elapsed();
    std::cout << "Timer test - elapsed time: " << elapsed.count() << "ms" << std::endl;
    
    // Configuration utilities
    auto default_config = utils::config::getDefaultConfig();
    std::cout << "Default max response length: " << default_config.max_response_length << std::endl;
}

int main() {
    std::cout << "Agent-Zero HumanInterface Demonstration" << std::endl;
    std::cout << "=======================================" << std::endl;
    
    try {
        demonstrateInitialization();
        demonstrateBasicConversation();
        demonstrateContextManagement();
        demonstrateMultiSession();
        demonstrateAnalytics();
        demonstrateUtilities();
        
        std::cout << "\n🎉 All demonstrations completed successfully!" << std::endl;
        std::cout << "\nThe HumanInterface layer provides:" << std::endl;
        std::cout << "• Multi-modal input processing" << std::endl;
        std::cout << "• Context-aware conversations" << std::endl;
        std::cout << "• Session management" << std::endl;
        std::cout << "• AtomSpace integration" << std::endl;
        std::cout << "• Performance monitoring" << std::endl;
        std::cout << "• Comprehensive error handling" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Demo failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Demo failed with unknown exception" << std::endl;
        return 1;
    }
}