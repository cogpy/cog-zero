/*
 * language_processor_example.cpp
 *
 * Copyright (C) 2024 Agent-Zero-Genesis Project
 * 
 * Example demonstrating LanguageProcessor usage with Link Grammar
 */

#include <iostream>
#include <memory>

#include <opencog/atomspace/AtomSpace.h>
#include "agentzero/communication/LanguageProcessor.h"

using namespace agentzero::communication;
using namespace opencog;

void print_parse_result(const ParseResult& result) {
    std::cout << "\n--- Parse Result ---" << std::endl;
    std::cout << "Original text: " << result.text << std::endl;
    std::cout << "Valid parse: " << (result.is_valid ? "Yes" : "No") << std::endl;
    
    if (result.is_valid) {
        std::cout << "Parse cost: " << result.linkage_cost << std::endl;
        
        std::cout << "Words (" << result.words.size() << "): ";
        for (size_t i = 0; i < result.words.size(); i++) {
            std::cout << "[" << i << "]" << result.words[i] << " ";
        }
        std::cout << std::endl;
        
        std::cout << "Links (" << result.links.size() << "):" << std::endl;
        for (const auto& link : result.links) {
            std::cout << "  " << link.first << ": " << link.second.first 
                     << " -> " << link.second.second << std::endl;
        }
        
        if (result.atomspace_representation != Handle::UNDEFINED) {
            std::cout << "AtomSpace representation created: " 
                     << result.atomspace_representation << std::endl;
        }
    }
}

void print_semantic_result(const SemanticResult& result) {
    std::cout << "\n--- Semantic Analysis ---" << std::endl;
    
    if (!result.entities.empty()) {
        std::cout << "Entities: ";
        for (const std::string& entity : result.entities) {
            std::cout << "'" << entity << "' ";
        }
        std::cout << std::endl;
    }
    
    if (!result.concepts.empty()) {
        std::cout << "Concepts: ";
        for (const std::string& concept : result.concepts) {
            std::cout << "'" << concept << "' ";
        }
        std::cout << std::endl;
    }
    
    if (!result.sentiment_scores.empty()) {
        std::cout << "Sentiment scores:" << std::endl;
        for (const auto& score : result.sentiment_scores) {
            std::cout << "  " << score.first << ": " << score.second << std::endl;
        }
    }
    
    if (!result.concept_atoms.empty()) {
        std::cout << "Created " << result.concept_atoms.size() 
                 << " concept atoms in AtomSpace" << std::endl;
    }
}

void print_statistics(const std::map<std::string, double>& stats) {
    std::cout << "\n--- Processor Statistics ---" << std::endl;
    for (const auto& stat : stats) {
        std::cout << stat.first << ": " << stat.second << std::endl;
    }
}

int main() {
    std::cout << "=== LanguageProcessor Example ===" << std::endl;
    
    try {
        // Create AtomSpace for knowledge storage
        auto atomspace = std::make_shared<AtomSpace>();
        std::cout << "Created AtomSpace with " << atomspace->get_size() << " initial atoms" << std::endl;
        
        // Create LanguageProcessor with English configuration
        LanguageProcessorConfig config = create_english_config();
        config.verbosity_level = 1; // Show some Link Grammar output
        config.store_in_atomspace = true;
        config.enable_semantic_analysis = true;
        
        LanguageProcessor processor(atomspace, config);
        
        if (!processor.is_initialized()) {
            std::cout << "ERROR: LanguageProcessor failed to initialize." << std::endl;
            std::cout << "This likely means Link Grammar is not properly installed." << std::endl;
            std::cout << "Please install Link Grammar development libraries." << std::endl;
            return 1;
        }
        
        std::cout << "LanguageProcessor initialized successfully!" << std::endl;
        
        // Example 1: Simple sentence parsing
        std::cout << "\n=== Example 1: Simple Parsing ===" << std::endl;
        std::string simple_sentence = "The cat sat on the mat.";
        ParseResult simple_result = processor.parse_text(simple_sentence);
        print_parse_result(simple_result);
        
        // Example 2: Complex sentence with semantic analysis
        std::cout << "\n=== Example 2: Complex Sentence with Semantics ===" << std::endl;
        std::string complex_sentence = "John loves reading interesting books about artificial intelligence.";
        SemanticResult semantic_result = processor.analyze_text(complex_sentence);
        print_parse_result({complex_sentence, {}, {}, 0.0, true, Handle::UNDEFINED}); // Simplified for demo
        print_semantic_result(semantic_result);
        
        // Example 3: Multiple language constructs
        std::cout << "\n=== Example 3: Various Language Constructs ===" << std::endl;
        std::vector<std::string> test_sentences = {
            "What is your name?",
            "I am happy to see you.",
            "The quick brown fox jumps over the lazy dog.",
            "She will go to the store tomorrow.",
            "Programming languages are fascinating tools."
        };
        
        for (const std::string& sentence : test_sentences) {
            std::cout << "\nProcessing: " << sentence << std::endl;
            ParseResult result = processor.parse_text(sentence);
            if (result.is_valid) {
                std::cout << "✓ Parsed successfully (" << result.words.size() 
                         << " words, " << result.links.size() << " links)" << std::endl;
            } else {
                std::cout << "✗ Parse failed" << std::endl;
            }
        }
        
        // Example 4: Batch processing with progress callback
        std::cout << "\n=== Example 4: Batch Processing ===" << std::endl;
        processor.set_progress_callback([](size_t current, size_t total) {
            std::cout << "Progress: " << current << "/" << total 
                     << " (" << (100 * current / total) << "%)" << std::endl;
        });
        
        auto batch_results = processor.parse_batch(test_sentences);
        
        int successful_parses = 0;
        for (const auto& result : batch_results) {
            if (result.is_valid) successful_parses++;
        }
        
        std::cout << "Batch processing completed: " << successful_parses 
                 << "/" << batch_results.size() << " successful parses" << std::endl;
        
        // Show final statistics
        print_statistics(processor.get_statistics());
        
        // Show AtomSpace growth
        std::cout << "\nFinal AtomSpace size: " << atomspace->get_size() << " atoms" << std::endl;
        
        std::cout << "\n=== Example completed successfully! ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Example failed with exception: " << e.what() << std::endl;
        std::cerr << "This might indicate that Link Grammar is not properly installed." << std::endl;
        return 1;
    }
}