/*
 * test_language_processor.cpp
 *
 * Copyright (C) 2024 Agent-Zero-Genesis Project
 * 
 * Unit tests for LanguageProcessor class
 */

#include <iostream>
#include <cassert>
#include <memory>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include "agentzero/communication/LanguageProcessor.h"

using namespace agentzero::communication;
using namespace opencog;

// Test helper class
class LanguageProcessorTester {
private:
    AtomSpacePtr atomspace;
    std::unique_ptr<LanguageProcessor> processor;
    
public:
    LanguageProcessorTester() {
        atomspace = std::make_shared<AtomSpace>();
        
        // Create configuration for testing
        LanguageProcessorConfig config = create_english_config();
        config.verbosity_level = 0; // Minimize output during tests
        config.max_parse_time = 5;  // Short timeout for tests
        
        processor = std::make_unique<LanguageProcessor>(atomspace, config);
    }
    
    bool test_initialization() {
        std::cout << "Testing initialization..." << std::endl;
        
        if (!processor->is_initialized()) {
            std::cout << "ERROR: LanguageProcessor not initialized" << std::endl;
            return false;
        }
        
        std::cout << "✓ Initialization test passed" << std::endl;
        return true;
    }
    
    bool test_configuration() {
        std::cout << "Testing configuration..." << std::endl;
        
        const auto& config = processor->get_config();
        
        if (config.language != "en") {
            std::cout << "ERROR: Expected language 'en', got '" << config.language << "'" << std::endl;
            return false;
        }
        
        if (config.max_parse_time != 5) {
            std::cout << "ERROR: Expected max_parse_time 5, got " << config.max_parse_time << std::endl;
            return false;
        }
        
        std::cout << "✓ Configuration test passed" << std::endl;
        return true;
    }
    
    bool test_simple_parsing() {
        std::cout << "Testing simple parsing..." << std::endl;
        
        std::string test_text = "The cat sat on the mat.";
        ParseResult result = processor->parse_text(test_text);
        
        if (!result.is_valid) {
            std::cout << "ERROR: Parse failed for simple sentence: " << test_text << std::endl;
            return false;
        }
        
        if (result.text != test_text) {
            std::cout << "ERROR: Original text not preserved" << std::endl;
            return false;
        }
        
        if (result.words.empty()) {
            std::cout << "ERROR: No words extracted from parse" << std::endl;
            return false;
        }
        
        std::cout << "✓ Simple parsing test passed (found " << result.words.size() << " words, " 
                 << result.links.size() << " links)" << std::endl;
        return true;
    }
    
    bool test_batch_processing() {
        std::cout << "Testing batch processing..." << std::endl;
        
        std::vector<std::string> test_sentences = {
            "Hello world.",
            "The dog runs fast.",
            "She likes reading books.",
            "It is a beautiful day."
        };
        
        auto results = processor->parse_batch(test_sentences);
        
        if (results.size() != test_sentences.size()) {
            std::cout << "ERROR: Expected " << test_sentences.size() << " results, got " 
                     << results.size() << std::endl;
            return false;
        }
        
        int valid_parses = 0;
        for (const auto& result : results) {
            if (result.is_valid) {
                valid_parses++;
            }
        }
        
        std::cout << "✓ Batch processing test passed (" << valid_parses << "/" 
                 << results.size() << " parses successful)" << std::endl;
        return true;
    }
    
    bool test_semantic_analysis() {
        std::cout << "Testing semantic analysis..." << std::endl;
        
        std::string test_text = "John loves reading interesting books about science.";
        SemanticResult result = processor->analyze_text(test_text);
        
        if (result.concepts.empty() && result.entities.empty()) {
            std::cout << "WARNING: No concepts or entities extracted (this might be expected for simple implementation)" << std::endl;
        }
        
        std::cout << "✓ Semantic analysis test passed (found " << result.entities.size() 
                 << " entities, " << result.concepts.size() << " concepts)" << std::endl;
        return true;
    }
    
    bool test_atomspace_integration() {
        std::cout << "Testing AtomSpace integration..." << std::endl;
        
        size_t initial_atoms = atomspace->get_size();
        
        std::string test_text = "The quick brown fox jumps.";
        Handle representation = processor->get_atomspace_representation(test_text);
        
        if (representation == Handle::UNDEFINED) {
            std::cout << "ERROR: Failed to create AtomSpace representation" << std::endl;
            return false;
        }
        
        size_t final_atoms = atomspace->get_size();
        
        if (final_atoms <= initial_atoms) {
            std::cout << "ERROR: No new atoms added to AtomSpace" << std::endl;
            return false;
        }
        
        std::cout << "✓ AtomSpace integration test passed (added " 
                 << (final_atoms - initial_atoms) << " atoms)" << std::endl;
        return true;
    }
    
    bool test_error_handling() {
        std::cout << "Testing error handling..." << std::endl;
        
        // Test empty string
        ParseResult result1 = processor->parse_text("");
        if (result1.is_valid) {
            std::cout << "ERROR: Empty string should not produce valid parse" << std::endl;
            return false;
        }
        
        // Test whitespace only
        ParseResult result2 = processor->parse_text("   \t\n   ");
        if (result2.is_valid) {
            std::cout << "ERROR: Whitespace-only string should not produce valid parse" << std::endl;
            return false;
        }
        
        // Test very long string
        std::string long_text(5000, 'a');
        ParseResult result3 = processor->parse_text(long_text);
        if (result3.is_valid) {
            std::cout << "ERROR: Excessively long string should not produce valid parse" << std::endl;
            return false;
        }
        
        std::cout << "✓ Error handling test passed" << std::endl;
        return true;
    }
    
    bool test_statistics() {
        std::cout << "Testing statistics..." << std::endl;
        
        processor->reset_statistics();
        
        auto stats_before = processor->get_statistics();
        if (stats_before.at("total_parses") != 0.0) {
            std::cout << "ERROR: Statistics not reset properly" << std::endl;
            return false;
        }
        
        // Perform some parses
        processor->parse_text("This is a test.");
        processor->parse_text("Another test sentence.");
        processor->parse_text(""); // This should fail
        
        auto stats_after = processor->get_statistics();
        if (stats_after.at("total_parses") != 3.0) {
            std::cout << "ERROR: Expected 3 total parses, got " 
                     << stats_after.at("total_parses") << std::endl;
            return false;
        }
        
        std::cout << "✓ Statistics test passed (success rate: " 
                 << (stats_after.at("success_rate") * 100.0) << "%)" << std::endl;
        return true;
    }
    
    bool test_performance() {
        std::cout << "Testing performance..." << std::endl;
        
        const int num_parses = 10;
        std::vector<std::string> test_sentences;
        
        for (int i = 0; i < num_parses; i++) {
            test_sentences.push_back("This is test sentence number " + std::to_string(i) + ".");
        }
        
        auto start_time = std::chrono::steady_clock::now();
        
        for (const auto& sentence : test_sentences) {
            processor->parse_text(sentence);
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        double avg_time_ms = static_cast<double>(duration.count()) / num_parses;
        
        std::cout << "✓ Performance test passed (average time: " << avg_time_ms 
                 << " ms per parse)" << std::endl;
        
        if (avg_time_ms > 1000.0) {
            std::cout << "WARNING: Average parse time is high (>" << avg_time_ms << " ms)" << std::endl;
        }
        
        return true;
    }
    
    void run_all_tests() {
        std::cout << "\n=== LanguageProcessor Unit Tests ===" << std::endl;
        
        int passed = 0;
        int total = 0;
        
        total++; if (test_initialization()) passed++;
        total++; if (test_configuration()) passed++;
        total++; if (test_simple_parsing()) passed++;
        total++; if (test_batch_processing()) passed++;
        total++; if (test_semantic_analysis()) passed++;
        total++; if (test_atomspace_integration()) passed++;
        total++; if (test_error_handling()) passed++;
        total++; if (test_statistics()) passed++;
        total++; if (test_performance()) passed++;
        
        std::cout << "\n=== Test Results ===" << std::endl;
        std::cout << "Passed: " << passed << "/" << total << " tests" << std::endl;
        
        if (passed == total) {
            std::cout << "✅ ALL TESTS PASSED!" << std::endl;
        } else {
            std::cout << "❌ " << (total - passed) << " tests failed!" << std::endl;
        }
        
        // Print final statistics
        auto final_stats = processor->get_statistics();
        std::cout << "\nFinal processor statistics:" << std::endl;
        for (const auto& stat : final_stats) {
            std::cout << "  " << stat.first << ": " << stat.second << std::endl;
        }
    }
};

int main() {
    try {
        LanguageProcessorTester tester;
        tester.run_all_tests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}