/**
 * test_HumanInterface.cpp - Unit Tests for HumanInterface
 * 
 * Part of AZ-HUMAN-001: Create HumanInterface layer
 * Tests core human-agent interaction functionality
 * 
 * Copyright (C) 2024 OpenCog Foundation
 */

#include <iostream>
#include <cassert>
#include <memory>

#include <agentzero/communication/HumanInterface.h>
#include <agentzero/communication/CommunicationUtils.h>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

using namespace opencog;
using namespace opencog::agentzero::communication;

void testBasicInitialization() {
    std::cout << "Testing basic initialization..." << std::endl;
    
    auto atomspace = std::make_shared<AtomSpace>();
    CommunicationConfig config;
    
    HumanInterface interface(atomspace, config);
    
    // Test initialization
    assert(interface.initialize() == true);
    assert(interface.isReady() == true);
    
    // Test status
    auto status = interface.getStatus();
    assert(status.healthy == true);
    assert(status.active_sessions == 0);
    
    std::cout << "✓ Basic initialization test passed" << std::endl;
}

void testSessionManagement() {
    std::cout << "Testing session management..." << std::endl;
    
    auto atomspace = std::make_shared<AtomSpace>();
    HumanInterface interface(atomspace);
    interface.initialize();
    
    // Test session creation
    std::string user_id = "test_user_123";
    std::string session_id = interface.startSession(user_id);
    
    assert(!session_id.empty());
    assert(interface.getActiveSessionCount() == 1);
    
    // Test session state
    auto session_state = interface.getSessionState(session_id);
    assert(session_state != nullptr);
    assert(session_state->user_id == user_id);
    assert(session_state->active == true);
    
    // Test session termination
    assert(interface.endSession(session_id) == true);
    assert(interface.getActiveSessionCount() == 0);
    
    std::cout << "✓ Session management test passed" << std::endl;
}

void testBasicInteraction() {
    std::cout << "Testing basic interaction..." << std::endl;
    
    auto atomspace = std::make_shared<AtomSpace>();
    HumanInterface interface(atomspace);
    interface.initialize();
    
    // Start a session
    std::string user_id = "test_user_456";
    std::string session_id = interface.startSession(user_id);
    
    // Create test input
    HumanInput input;
    input.user_id = user_id;
    input.raw.type = InputType::TEXT;
    input.raw.content = "Hello, how are you?";
    
    // Process input
    auto response = interface.processInput(input, session_id);
    
    assert(response.success == true);
    assert(!response.agent_response.content.empty());
    assert(response.session_id == session_id);
    assert(response.processing_time.count() > 0);
    
    // Verify response contains greeting
    std::string response_lower = response.agent_response.content;
    std::transform(response_lower.begin(), response_lower.end(), response_lower.begin(), ::tolower);
    assert(response_lower.find("hello") != std::string::npos || 
           response_lower.find("hi") != std::string::npos ||
           response_lower.find("greet") != std::string::npos);
    
    interface.endSession(session_id);
    
    std::cout << "✓ Basic interaction test passed" << std::endl;
}

void testContextManagement() {
    std::cout << "Testing context management..." << std::endl;
    
    auto atomspace = std::make_shared<AtomSpace>();
    HumanInterface interface(atomspace);
    interface.initialize();
    
    std::string session_id = interface.startSession("test_user");
    
    // Test context updates
    ContextUpdate update(ContextUpdate::UpdateType::SET, "test_key", "test_value");
    interface.updateContext(session_id, update);
    
    auto context = interface.getContext(session_id);
    assert(context.variables.find("test_key") != context.variables.end());
    assert(context.variables["test_key"] == "test_value");
    
    // Test context clearing
    interface.clearContext(session_id);
    context = interface.getContext(session_id);
    assert(context.variables.empty());
    
    interface.endSession(session_id);
    
    std::cout << "✓ Context management test passed" << std::endl;
}

void testErrorHandling() {
    std::cout << "Testing error handling..." << std::endl;
    
    auto atomspace = std::make_shared<AtomSpace>();
    HumanInterface interface(atomspace);
    interface.initialize();
    
    // Test invalid session
    HumanInput input("Test input");
    input.user_id = "test_user";
    
    auto response = interface.processInput(input, "invalid_session_id");
    assert(response.success == false);
    assert(!response.error_message.empty());
    
    // Test uninitialized interface
    HumanInterface uninitialized_interface(atomspace);
    response = uninitialized_interface.processInput(input, "any_session");
    assert(response.success == false);
    
    std::cout << "✓ Error handling test passed" << std::endl;
}

void testAnalytics() {
    std::cout << "Testing analytics..." << std::endl;
    
    auto atomspace = std::make_shared<AtomSpace>();
    HumanInterface interface(atomspace);
    interface.initialize();
    
    std::string session_id = interface.startSession("analytics_user");
    
    // Process several interactions
    for (int i = 0; i < 5; i++) {
        HumanInput input("Test message " + std::to_string(i));
        input.user_id = "analytics_user";
        interface.processInput(input, session_id);
    }
    
    // Get analytics
    auto analytics = interface.getAnalytics();
    assert(analytics.total_interactions == 5);
    assert(analytics.success_rate > 0.0);
    
    interface.endSession(session_id);
    
    std::cout << "✓ Analytics test passed" << std::endl;
}

int main() {
    std::cout << "Running HumanInterface unit tests..." << std::endl;
    
    try {
        testBasicInitialization();
        testSessionManagement();
        testBasicInteraction();
        testContextManagement();
        testErrorHandling();
        testAnalytics();
        
        std::cout << "\n=== All HumanInterface tests passed! ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}