/*
 * batch_processing_example.cpp
 *
 * Copyright (C) 2024 Agent-Zero-Genesis Project
 * 
 * Example demonstrating batch processing capabilities
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include "agentzero/communication/LanguageProcessor.h"

using namespace agentzero::communication;
using namespace opencog;

// Generate test sentences for batch processing
std::vector<std::string> generate_test_sentences(size_t count) {
    std::vector<std::string> sentences;
    
    // Base sentences with variations
    std::vector<std::string> base_sentences = {
        "The cat sits on the mat.",
        "She reads an interesting book.",
        "They will travel to the mountain.",
        "He enjoys playing musical instruments.",
        "The scientist discovered something important.",
        "Students learn new concepts every day.",
        "The weather is beautiful today.",
        "Technology changes our daily lives.",
        "Art expresses human creativity.",
        "Music brings people together."
    };
    
    for (size_t i = 0; i < count; i++) {
        sentences.push_back(base_sentences[i % base_sentences.size()]);
    }
    
    return sentences;
}

// Progress tracking callback
class ProgressTracker {
private:
    std::chrono::steady_clock::time_point start_time;
    size_t last_reported = 0;
    
public:
    ProgressTracker() : start_time(std::chrono::steady_clock::now()) {}
    
    void operator()(size_t current, size_t total) {
        // Report progress every 10% or at completion
        size_t percentage = (100 * current) / total;
        size_t last_percentage = (100 * last_reported) / total;
        
        if (percentage >= last_percentage + 10 || current == total) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
            
            double rate = current > 0 ? static_cast<double>(elapsed.count()) / current : 0.0;
            
            std::cout << "[" << percentage << "%] Processed " << current << "/" << total 
                     << " sentences (" << rate << " ms/sentence)" << std::endl;
            
            last_reported = current;
        }
    }
};

void analyze_batch_results(const std::vector<ParseResult>& results) {
    std::cout << "\n=== Batch Analysis Results ===" << std::endl;
    
    size_t valid_parses = 0;
    size_t total_words = 0;
    size_t total_links = 0;
    double total_cost = 0.0;
    
    std::map<size_t, size_t> word_count_distribution;
    std::map<size_t, size_t> link_count_distribution;
    
    for (const auto& result : results) {
        if (result.is_valid) {
            valid_parses++;
            total_words += result.words.size();
            total_links += result.links.size();
            total_cost += result.linkage_cost;
            
            word_count_distribution[result.words.size()]++;
            link_count_distribution[result.links.size()]++;
        }
    }
    
    std::cout << "Valid parses: " << valid_parses << "/" << results.size() 
             << " (" << (100.0 * valid_parses / results.size()) << "%)" << std::endl;
    
    if (valid_parses > 0) {
        std::cout << "Average words per sentence: " << (static_cast<double>(total_words) / valid_parses) << std::endl;
        std::cout << "Average links per sentence: " << (static_cast<double>(total_links) / valid_parses) << std::endl;
        std::cout << "Average parse cost: " << (total_cost / valid_parses) << std::endl;
        
        std::cout << "\nWord count distribution:" << std::endl;
        for (const auto& entry : word_count_distribution) {
            std::cout << "  " << entry.first << " words: " << entry.second << " sentences" << std::endl;
        }
        
        std::cout << "\nLink count distribution:" << std::endl;
        for (const auto& entry : link_count_distribution) {
            std::cout << "  " << entry.first << " links: " << entry.second << " sentences" << std::endl;
        }
    }
}

void performance_comparison(LanguageProcessor& processor) {
    std::cout << "\n=== Performance Comparison ===" << std::endl;
    
    std::vector<std::string> test_sentences = generate_test_sentences(20);
    
    // Test individual processing
    processor.reset_statistics();
    auto start_individual = std::chrono::steady_clock::now();
    
    std::vector<ParseResult> individual_results;
    for (const std::string& sentence : test_sentences) {
        individual_results.push_back(processor.parse_text(sentence));
    }
    
    auto end_individual = std::chrono::steady_clock::now();
    auto individual_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_individual - start_individual);
    
    // Test batch processing
    processor.reset_statistics();
    auto start_batch = std::chrono::steady_clock::now();
    
    auto batch_results = processor.parse_batch(test_sentences);
    
    auto end_batch = std::chrono::steady_clock::now();
    auto batch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_batch - start_batch);
    
    std::cout << "Individual processing: " << individual_duration.count() << " ms" << std::endl;
    std::cout << "Batch processing: " << batch_duration.count() << " ms" << std::endl;
    
    if (batch_duration.count() > 0) {
        double speedup = static_cast<double>(individual_duration.count()) / batch_duration.count();
        std::cout << "Speedup: " << speedup << "x" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "=== Batch Processing Example ===" << std::endl;
    
    size_t batch_size = 50;
    
    // Check command line arguments
    if (argc > 1) {
        batch_size = std::atoi(argv[1]);
        if (batch_size == 0) {
            batch_size = 50;
        }
    }
    
    std::cout << "Batch size: " << batch_size << " sentences" << std::endl;
    
    try {
        // Setup
        auto atomspace = std::make_shared<AtomSpace>();
        
        LanguageProcessorConfig config = create_english_config();
        config.verbosity_level = 0; // Minimize output for batch processing
        config.store_in_atomspace = true;
        
        LanguageProcessor processor(atomspace, config);
        
        if (!processor.is_initialized()) {
            std::cout << "ERROR: LanguageProcessor failed to initialize." << std::endl;
            std::cout << "Link Grammar may not be properly installed." << std::endl;
            return 1;
        }
        
        // Generate test data
        std::cout << "\nGenerating " << batch_size << " test sentences..." << std::endl;
        auto test_sentences = generate_test_sentences(batch_size);
        
        // Set up progress tracking
        ProgressTracker progress_tracker;
        processor.set_progress_callback(std::ref(progress_tracker));
        
        // Perform batch processing
        std::cout << "\nStarting batch processing..." << std::endl;
        auto start_time = std::chrono::steady_clock::now();
        
        auto results = processor.parse_batch(test_sentences);
        
        auto end_time = std::chrono::steady_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        std::cout << "\nBatch processing completed in " << total_duration.count() << " ms" << std::endl;
        
        // Analyze results
        analyze_batch_results(results);
        
        // Show processor statistics
        std::cout << "\n=== Processor Statistics ===" << std::endl;
        auto stats = processor.get_statistics();
        for (const auto& stat : stats) {
            std::cout << stat.first << ": " << stat.second << std::endl;
        }
        
        // Performance comparison (if reasonable batch size)
        if (batch_size <= 100) {
            performance_comparison(processor);
        }
        
        // AtomSpace statistics
        std::cout << "\n=== AtomSpace Statistics ===" << std::endl;
        std::cout << "Total atoms: " << atomspace->get_size() << std::endl;
        
        std::cout << "\n=== Batch processing example completed! ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Example failed with exception: " << e.what() << std::endl;
        return 1;
    }
}