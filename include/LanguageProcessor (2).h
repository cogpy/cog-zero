/*
 * LanguageProcessor.h
 *
 * Copyright (C) 2024 Agent-Zero-Genesis Project
 * 
 * Natural Language Processing with Link Grammar integration for Agent-Zero
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _AGENTZERO_LANGUAGE_PROCESSOR_H
#define _AGENTZERO_LANGUAGE_PROCESSOR_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>

// Link Grammar includes (optional - guarded by HAVE_LINK_GRAMMAR)
#ifdef HAVE_LINK_GRAMMAR
#include <link-grammar/link-includes.h>
#endif

// OpenCog includes
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/value/Value.h>

#ifdef HAVE_LG_ATOMESE
#include <opencog/nlp/lg-parse/LGParseLink.h>
#endif

namespace agentzero {
namespace communication {

/** \addtogroup agentzero_communication
 *  @{
 */

/// Parse result structure containing linguistic information
struct ParseResult {
    std::string text;           ///< Original input text
    std::vector<std::string> words; ///< Tokenized words
    std::vector<std::pair<std::string, std::pair<int, int>>> links; ///< Link labels and word indices
    double linkage_cost;        ///< Parse quality cost (lower is better)
    bool is_valid;              ///< Whether parse was successful
    opencog::Handle atomspace_representation; ///< AtomSpace representation of parse
};

/// Semantic analysis result
struct SemanticResult {
    std::vector<std::string> entities;      ///< Named entities found
    std::vector<std::string> concepts;      ///< Key concepts identified
    std::vector<std::pair<std::string, std::string>> relations; ///< Semantic relations
    std::map<std::string, double> sentiment_scores; ///< Sentiment analysis
    opencog::HandleSeq concept_atoms;       ///< AtomSpace concept representations
};

/// Configuration for LanguageProcessor
struct LanguageProcessorConfig {
    std::string dictionary_path;    ///< Path to Link Grammar dictionary
    std::string language;          ///< Language code (e.g., "en", "ru")
    int max_parse_time;           ///< Maximum parse time in seconds
    int verbosity_level;          ///< Link Grammar verbosity (0-6)
    bool store_in_atomspace;      ///< Whether to store results in AtomSpace
    bool enable_semantic_analysis; ///< Enable semantic processing
    double confidence_threshold;   ///< Minimum confidence for results
};

/**
 * LanguageProcessor provides comprehensive natural language processing
 * capabilities using Link Grammar parser integrated with OpenCog's AtomSpace.
 * 
 * Features:
 * - Syntactic parsing using Link Grammar
 * - Semantic analysis and concept extraction
 * - AtomSpace integration for knowledge representation
 * - Multi-language support
 * - Robust error handling
 * - Performance optimization
 * 
 * This class follows OpenCog architectural patterns and integrates
 * seamlessly with the Agent-Zero cognitive architecture.
 */
class LanguageProcessor
{
private:
#ifdef HAVE_LINK_GRAMMAR
    Dictionary _dictionary;             ///< Link Grammar dictionary
    Parse_Options _parse_options;      ///< Link Grammar parse options
#else
    void* _dictionary;                  ///< Link Grammar dictionary (unavailable)
    void* _parse_options;              ///< Link Grammar parse options (unavailable)
#endif
    opencog::AtomSpacePtr _atomspace;  ///< AtomSpace for knowledge storage
    LanguageProcessorConfig _config;   ///< Configuration settings
    
    // Private helper methods
    void initialize_link_grammar();
    void cleanup_link_grammar();
#ifdef HAVE_LINK_GRAMMAR
    ParseResult create_parse_result(Sentence sent, Linkage lkg, const std::string& text);
#endif
    SemanticResult analyze_semantics(const ParseResult& parse_result);
    opencog::Handle convert_to_atomspace(const ParseResult& parse_result);
    std::vector<std::string> extract_entities(const ParseResult& parse_result);
    std::vector<std::string> extract_concepts(const ParseResult& parse_result);
    std::map<std::string, double> analyze_sentiment(const std::string& text);
    
    // Error handling helpers
    void handle_parse_error(const std::string& text, int error_code);
    bool validate_input(const std::string& text);

public:
    /**
     * Constructor
     * @param atomspace Shared pointer to AtomSpace for knowledge storage
     * @param config Configuration settings for the processor
     */
    explicit LanguageProcessor(opencog::AtomSpacePtr atomspace,
                              const LanguageProcessorConfig& config = LanguageProcessorConfig());
    
    /**
     * Destructor - cleans up Link Grammar resources
     */
    ~LanguageProcessor();
    
    // Delete copy constructor and assignment operator
    LanguageProcessor(const LanguageProcessor&) = delete;
    LanguageProcessor& operator=(const LanguageProcessor&) = delete;
    
    /**
     * Parse text using Link Grammar and return syntactic structure
     * @param text Input text to parse
     * @return ParseResult containing syntactic information
     */
    ParseResult parse_text(const std::string& text);
    
    /**
     * Perform comprehensive NLP analysis including syntax and semantics
     * @param text Input text to analyze
     * @return SemanticResult containing both syntactic and semantic information
     */
    SemanticResult analyze_text(const std::string& text);
    
    /**
     * Parse multiple sentences in batch for efficiency
     * @param texts Vector of input texts
     * @return Vector of ParseResults
     */
    std::vector<ParseResult> parse_batch(const std::vector<std::string>& texts);
    
    /**
     * Get AtomSpace representation for a given text
     * @param text Input text
     * @return Handle to the AtomSpace representation
     */
    opencog::Handle get_atomspace_representation(const std::string& text);
    
    /**
     * Update configuration settings
     * @param new_config Updated configuration
     * @return true if update successful
     */
    bool update_config(const LanguageProcessorConfig& new_config);
    
    /**
     * Get current configuration
     * @return Current configuration settings
     */
    const LanguageProcessorConfig& get_config() const;
    
    /**
     * Check if processor is properly initialized
     * @return true if ready to process text
     */
    bool is_initialized() const;
    
    /**
     * Get parser statistics
     * @return Map of statistics (parses_completed, errors, average_time, etc.)
     */
    std::map<std::string, double> get_statistics() const;
    
    /**
     * Reset statistics counters
     */
    void reset_statistics();
    
    /**
     * Set callback for progress reporting during batch processing
     * @param callback Function to call with progress updates
     */
    void set_progress_callback(std::function<void(size_t current, size_t total)> callback);

private:
    // Statistics tracking
    mutable size_t _total_parses;
    mutable size_t _successful_parses;
    mutable size_t _failed_parses;
    mutable double _total_parse_time;
    mutable std::chrono::steady_clock::time_point _last_parse_start;
    
    // Progress callback
    std::function<void(size_t, size_t)> _progress_callback;
    
    // Internal state tracking
    bool _initialized;
    std::string _last_error;
};

/** @}*/

// Utility functions for common NLP tasks

/**
 * Create default configuration for English language processing
 * @return Default LanguageProcessorConfig for English
 */
LanguageProcessorConfig create_english_config();

/**
 * Create default configuration for Russian language processing  
 * @return Default LanguageProcessorConfig for Russian
 */
LanguageProcessorConfig create_russian_config();

/**
 * Validate configuration settings
 * @param config Configuration to validate
 * @return true if configuration is valid
 */
bool validate_config(const LanguageProcessorConfig& config);

} // namespace communication
} // namespace agentzero

#endif // _AGENTZERO_LANGUAGE_PROCESSOR_H