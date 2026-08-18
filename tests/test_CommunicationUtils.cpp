/**
 * test_CommunicationUtils.cpp - Unit Tests for Communication Utilities
 * 
 * Part of AZ-HUMAN-001: Create HumanInterface layer
 * Tests utility functions for communication operations
 * 
 * Copyright (C) 2024 OpenCog Foundation
 */

#include <iostream>
#include <cassert>
#include <vector>

#include <agentzero/communication/CommunicationUtils.h>

using namespace opencog::agentzero::communication;
using namespace opencog::agentzero::communication::utils;

void testTextProcessing() {
    std::cout << "Testing text processing utilities..." << std::endl;
    
    // Test normalization
    std::string test_text = "  Hello WORLD!  \n\t ";
    std::string normalized = text::normalize(test_text);
    assert(normalized == "hello world!");
    
    // Test tokenization
    std::vector<std::string> tokens = text::tokenize("Hello, world! How are you?");
    assert(tokens.size() >= 4);
    assert(tokens[0] == "Hello" || tokens[0] == "hello");
    
    // Test sentence extraction
    std::vector<std::string> sentences = text::extractSentences("Hello world. How are you? Fine, thanks!");
    assert(sentences.size() == 3);
    
    // Test similarity
    double similarity = text::calculateSimilarity("hello world", "hello earth");
    assert(similarity > 0.0 && similarity < 1.0);
    
    // Test word count
    size_t word_count = text::countWords("This is a test sentence");
    assert(word_count == 5);
    
    std::cout << "✓ Text processing utilities test passed" << std::endl;
}

void testSessionUtilities() {
    std::cout << "Testing session utilities..." << std::endl;
    
    // Test session ID generation
    std::string session_id = session::generateSessionId("user123", "test");
    assert(!session_id.empty());
    assert(session_id.find("test") != std::string::npos);
    assert(session_id.find("user123") != std::string::npos);
    
    // Test session ID validation
    assert(session::isValidSessionId(session_id) == true);
    assert(session::isValidSessionId("") == false);
    assert(session::isValidSessionId("invalid") == false);
    
    // Test user ID extraction
    std::string extracted_user = session::extractUserId(session_id);
    assert(extracted_user == "user123");
    
    std::cout << "✓ Session utilities test passed" << std::endl;
}

void testContextUtilities() {
    std::cout << "Testing context utilities..." << std::endl;
    
    // Create test contexts
    InteractionContext base_context;
    base_context.variables["key1"] = "value1";
    base_context.active_topics.push_back("topic1");
    
    InteractionContext update_context;
    update_context.variables["key2"] = "value2";
    update_context.active_topics.push_back("topic2");
    
    // Test context merging
    InteractionContext merged = context::mergeContexts(base_context, update_context);
    assert(merged.variables.size() == 2);
    assert(merged.variables["key1"] == "value1");
    assert(merged.variables["key2"] == "value2");
    assert(merged.active_topics.size() == 2);
    
    // Test context pruning (simplified test)
    auto pruned = context::pruneOldContext(merged, std::chrono::hours(1));
    // Context should remain since it's recent
    assert(pruned.variables.size() == 2);
    
    std::cout << "✓ Context utilities test passed" << std::endl;
}

void testFormattingUtilities() {
    std::cout << "Testing formatting utilities..." << std::endl;
    
    // Create test response
    AgentResponse response;
    response.content = "Test response content";
    response.suggestions = {"Suggestion 1", "Suggestion 2"};
    response.confidence = 0.85;
    
    // Test plain text formatting
    std::string plain_text = formatting::formatPlainText(response);
    assert(plain_text.find("Test response content") != std::string::npos);
    assert(plain_text.find("Suggestion 1") != std::string::npos);
    
    // Test rich text formatting
    std::string rich_text = formatting::formatRichText(response);
    assert(rich_text.find("<content>") != std::string::npos);
    assert(rich_text.find("<suggestions>") != std::string::npos);
    
    // Test JSON formatting
    std::string json_text = formatting::formatJson(response);
    assert(json_text.find("\"content\"") != std::string::npos);
    assert(json_text.find("\"confidence\"") != std::string::npos);
    
    // Test response truncation
    std::string long_text = std::string(500, 'a') + " end";
    std::string truncated = formatting::truncateResponse(long_text, 100);
    assert(truncated.length() <= 100);
    assert(truncated.find("...") != std::string::npos);
    
    std::cout << "✓ Formatting utilities test passed" << std::endl;
}

void testAnalyticsUtilities() {
    std::cout << "Testing analytics utilities..." << std::endl;
    
    // Test percentile calculation
    std::vector<Duration> response_times;
    for (int i = 1; i <= 100; i++) {
        response_times.push_back(Duration(i));
    }
    
    auto percentiles = analytics::calculatePercentiles(response_times);
    assert(percentiles.find(50) != percentiles.end());
    assert(percentiles.find(90) != percentiles.end());
    assert(percentiles[50] == Duration(50));
    
    // Test input pattern analysis
    std::vector<ProcessedInput> inputs;
    for (int i = 0; i < 10; i++) {
        ProcessedInput input;
        input.confidence = 0.8 + (i * 0.01);
        input.tokens = {"token1", "token2"};
        input.intents = {"test_intent"};
        inputs.push_back(input);
    }
    
    auto metrics = analytics::analyzeInputPatterns(inputs);
    assert(metrics.find("average_confidence") != metrics.end());
    assert(metrics["average_confidence"] > 0.8);
    
    std::cout << "✓ Analytics utilities test passed" << std::endl;
}

void testConfigurationUtilities() {
    std::cout << "Testing configuration utilities..." << std::endl;
    
    // Test default config
    auto default_config = config::getDefaultConfig();
    assert(default_config.enable_input_preprocessing == true);
    
    // Test config validation
    CommunicationConfig invalid_config;
    invalid_config.min_confidence_threshold = 2.0; // Invalid: > 1.0
    invalid_config.max_response_length = 5; // Invalid: < 10
    
    auto errors = config::validateConfig(invalid_config);
    assert(!errors.empty());
    
    // Test config merging
    CommunicationConfig base_config = default_config;
    CommunicationConfig override_config;
    override_config.max_response_length = 2000;
    
    auto merged = config::mergeConfigs(base_config, override_config);
    assert(merged.max_response_length == 2000);
    
    std::cout << "✓ Configuration utilities test passed" << std::endl;
}

void testErrorUtilities() {
    std::cout << "Testing error utilities..." << std::endl;
    
    // Test error response creation
    auto error_response = error::createErrorResponse("TEST_ERROR", "Test error message", "session123");
    assert(error_response.success == false);
    assert(error_response.error_message == "Test error message");
    assert(error_response.session_id == "session123");
    
    // Test input validation
    HumanInput valid_input;
    valid_input.user_id = "user123";
    valid_input.raw.content = "Valid input content";
    
    auto validation_errors = error::validateInput(valid_input);
    assert(validation_errors.empty());
    
    // Test invalid input
    HumanInput invalid_input;
    invalid_input.user_id = ""; // Invalid: empty user ID
    invalid_input.raw.content = ""; // Invalid: empty content
    
    validation_errors = error::validateInput(invalid_input);
    assert(!validation_errors.empty());
    
    std::cout << "✓ Error utilities test passed" << std::endl;
}

void testPerformanceUtilities() {
    std::cout << "Testing performance utilities..." << std::endl;
    
    // Test timer
    performance::Timer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto elapsed = timer.elapsed();
    assert(elapsed.count() >= 10);
    
    // Test metrics creation
    auto metrics = performance::createMetrics("test_operation", Duration(100), true);
    assert(metrics.find("test_operation_duration_ms") != metrics.end());
    assert(metrics["test_operation_duration_ms"] == 100.0);
    assert(metrics["test_operation_success"] == 1.0);
    
    std::cout << "✓ Performance utilities test passed" << std::endl;
}

int main() {
    std::cout << "Running CommunicationUtils unit tests..." << std::endl;
    
    try {
        testTextProcessing();
        testSessionUtilities();
        testContextUtilities();
        testFormattingUtilities();
        testAnalyticsUtilities();
        testConfigurationUtilities();
        testErrorUtilities();
        testPerformanceUtilities();
        
        std::cout << "\n=== All CommunicationUtils tests passed! ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}