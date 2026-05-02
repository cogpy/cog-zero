/*
 * src/LanguageProcessor.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * LanguageProcessor Implementation
 * Handles natural language processing tasks with OpenCog integration
 * Part of the AGENT-ZERO-GENESIS project Phase 6: Communication & NLP
 */

#include <algorithm>
#include <sstream>
#include <regex>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/LanguageProcessor.h"

#ifdef HAVE_LG_ATOMESE
// Include lg-atomese headers when available
// #include <opencog/lg-atomese/LGAtomese.h>
#endif

#ifdef HAVE_LINK_GRAMMAR
// Include link-grammar headers when available
// #include <link-grammar/link-includes.h>
#endif

using namespace opencog;
using namespace opencog::agentzero;
using opencog::HandleSeq;

LanguageProcessor::LanguageProcessor(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
    , _use_link_grammar(false)
    , _use_lg_atomese(false)
{
#ifdef HAVE_LG_ATOMESE
    _use_lg_atomese = true;
    logger().info("LanguageProcessor initialized with lg-atomese support");
#endif

#ifdef HAVE_LINK_GRAMMAR
    _use_link_grammar = true;
    logger().info("LanguageProcessor initialized with link-grammar support");
#endif

    if (!_use_lg_atomese && !_use_link_grammar) {
        logger().info("LanguageProcessor initialized with basic NLP only");
    }
}

LanguageProcessor::~LanguageProcessor() {
    logger().debug("LanguageProcessor destroyed");
}

ParseResult LanguageProcessor::parseText(const std::string& text) {
    ParseResult result;
    result.original_text = text;
    
    if (text.empty()) {
        result.success = false;
        return result;
    }
    
    try {
        // Basic parsing implementation
        // In a full implementation, this would use lg-atomese or link-grammar
        
        // For now, implement basic text analysis
        result.intent = detectIntent(text);
        result.detected_entities = extractEntities(text);
        
        // Create basic AtomSpace representation
        Handle text_atom = textToAtoms(text);
        if (text_atom != Handle::UNDEFINED) {
            result.parsed_atoms.push_back(text_atom);
        }
        
        // Simple confidence calculation based on text properties
        result.confidence = calculateConfidence(text);
        result.success = true;
        
    } catch (const std::exception& e) {
        logger().error("Error parsing text: %s", e.what());
        result.success = false;
        result.confidence = 0.0;
    }
    
    return result;
}

std::string LanguageProcessor::generateResponse(const std::string& input_text, const std::string& context) {
    if (input_text.empty()) {
        return "";
    }
    
    // Simple response generation
    // In a full implementation, this would use sophisticated NLP
    
    std::string input_lower = input_text;
    std::transform(input_lower.begin(), input_lower.end(), input_lower.begin(), ::tolower);
    
    // Context-aware responses
    if (!context.empty()) {
        std::string context_lower = context;
        std::transform(context_lower.begin(), context_lower.end(), context_lower.begin(), ::tolower);
        
        if (context_lower.find("topic:") != std::string::npos) {
            // Extract topic and provide topic-aware response
            size_t topic_start = context_lower.find("topic:") + 6;
            std::string topic = context.substr(topic_start);
            return "Regarding " + topic + ", I think " + generateTopicResponse(input_text, topic);
        }
    }
    
    // Intent-based responses
    std::string intent = detectIntent(input_text);
    
    if (intent == "greeting") {
        return "Hello! How can I assist you today?";
    } else if (intent == "question") {
        return generateQuestionResponse(input_text);
    } else if (intent == "farewell") {
        return "Goodbye! It was nice talking with you.";
    } else if (intent == "request") {
        return "I'll do my best to help you with that.";
    } else {
        return generateDefaultResponse(input_text);
    }
}

std::string LanguageProcessor::detectIntent(const std::string& text) {
    if (text.empty()) {
        return "unknown";
    }
    
    std::string text_lower = text;
    std::transform(text_lower.begin(), text_lower.end(), text_lower.begin(), ::tolower);
    
    // Simple rule-based intent detection
    if (std::regex_search(text_lower, std::regex(R"(\b(hello|hi|hey|greetings)\b)"))) {
        return "greeting";
    } else if (std::regex_search(text_lower, std::regex(R"(\b(bye|goodbye|farewell|see you)\b)"))) {
        return "farewell";
    } else if (text_lower.find("?") != std::string::npos || 
               std::regex_search(text_lower, std::regex(R"(\b(what|how|when|where|why|who|which)\b)"))) {
        return "question";
    } else if (std::regex_search(text_lower, std::regex(R"(\b(please|could you|can you|would you)\b)"))) {
        return "request";
    } else if (std::regex_search(text_lower, std::regex(R"(\b(thank|thanks|appreciate)\b)"))) {
        return "gratitude";
    } else {
        return "statement";
    }
}

std::vector<std::string> LanguageProcessor::extractEntities(const std::string& text) {
    std::vector<std::string> entities;
    
    // Simple entity extraction using regex patterns
    // In a full implementation, this would use NER (Named Entity Recognition)
    
    // Extract potential names (capitalized words)
    std::regex name_regex(R"(\b[A-Z][a-z]+\b)");
    std::sregex_iterator names_begin(text.begin(), text.end(), name_regex);
    std::sregex_iterator names_end;
    
    for (std::sregex_iterator i = names_begin; i != names_end; ++i) {
        entities.push_back(i->str());
    }
    
    // Extract potential numbers
    std::regex number_regex(R"(\b\d+\b)");
    std::sregex_iterator numbers_begin(text.begin(), text.end(), number_regex);
    std::sregex_iterator numbers_end;
    
    for (std::sregex_iterator i = numbers_begin; i != numbers_end; ++i) {
        entities.push_back("NUMBER:" + i->str());
    }
    
    return entities;
}

// -----------------------------------------------------------------------
// opencog::agentzero::LanguageProcessor private helpers
// These are not in the agentzero::communication namespace.
// -----------------------------------------------------------------------

double LanguageProcessor::calculateConfidence(const std::string& text) const {
    double confidence = 0.5;
    if (text.length() > 10) confidence += 0.2;
    if (text.find(' ') != std::string::npos) confidence += 0.1;
    if (std::regex_search(text, std::regex(R"([.!?]$)"))) confidence += 0.1;
    if (text.length() < 3) confidence -= 0.3;
    if (std::regex_search(text, std::regex(R"(^\W+$)"))) confidence -= 0.4;
    return std::max(0.0, std::min(1.0, confidence));
}

std::string LanguageProcessor::generateQuestionResponse(const std::string& question) const {
    std::string q = question;
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
    if (q.find("what") != std::string::npos) return "That's a good question about definitions or identity.";
    if (q.find("how")  != std::string::npos) return "That's asking about process or method.";
    if (q.find("why")  != std::string::npos) return "You're asking about reasons or causes.";
    if (q.find("when") != std::string::npos) return "That's a question about timing.";
    if (q.find("where")!= std::string::npos) return "You're asking about location or position.";
    if (q.find("who")  != std::string::npos) return "That's about identity or people involved.";
    return "That's an interesting question. Could you provide more context?";
}

std::string LanguageProcessor::generateTopicResponse(const std::string& /*input*/,
                                                      const std::string& topic) const {
    return "This relates to our discussion about " + topic + ". What specifically would you like to know?";
}

std::string LanguageProcessor::generateDefaultResponse(const std::string& input) const {
    if (input.find('!') != std::string::npos)
        return "I can sense your enthusiasm! Please tell me more.";
    if (input.length() > 100)
        return "Thank you for that detailed information. Let me process what you've shared.";
    return "I understand. Could you elaborate on that?";
}

// opencog::agentzero::LanguageProcessor helpers for AtomSpace integration
Handle LanguageProcessor::textToAtoms(const std::string& text) {
    if (text.empty() || !_atomspace) return Handle::UNDEFINED;

    Handle text_atom = _atomspace->add_node(CONCEPT_NODE, std::string("Text:" + text));
    std::istringstream iss(text);
    std::string word;
    std::vector<Handle> word_atoms;

    while (iss >> word) {
        word.erase(std::remove_if(word.begin(), word.end(),
                                  [](char c) { return !std::isalnum(c); }), word.end());
        if (!word.empty()) {
            Handle wa = _atomspace->add_node(CONCEPT_NODE, std::string("Word:" + word));
            word_atoms.push_back(wa);
            _atomspace->add_link(MEMBER_LINK, HandleSeq{wa, text_atom});
        }
    }
    if (word_atoms.size() > 1) {
        _atomspace->add_link(ORDERED_LINK, word_atoms);
    }
    return text_atom;
}

std::string LanguageProcessor::atomsToText(const std::vector<Handle>& atoms) {
    static constexpr size_t TEXT_PREFIX_LEN = 5; // length of "Text:"
    static constexpr size_t WORD_PREFIX_LEN = 5; // length of "Word:"
    std::stringstream ss;
    for (size_t i = 0; i < atoms.size(); ++i) {
        if (i > 0) ss << " ";
        std::string name = atoms[i]->get_name();
        if (name.rfind("Text:", 0) == 0)      ss << name.substr(TEXT_PREFIX_LEN);
        else if (name.rfind("Word:", 0) == 0) ss << name.substr(WORD_PREFIX_LEN);
        else                                   ss << name;
    }
    return ss.str();
}

void LanguageProcessor::setUseLinks(bool use_links) {
    _use_link_grammar = use_links && _use_link_grammar;
    logger().info("Link Grammar usage %s", _use_link_grammar ? "enabled" : "disabled");
}

void LanguageProcessor::setLanguageModel(const std::string& model_path) {
    _language_model = model_path;
    logger().info("Language model set to: %s", model_path.c_str());
}

/*
 * LanguageProcessor.cpp (agentzero::communication)
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

#include "agentzero/communication/LanguageProcessor.h"

#include <sstream>
#include <algorithm>
#include <chrono>
#include <regex>
#include <stdexcept>

// OpenCog includes
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/core/NumberNode.h>
#include <opencog/atoms/value/StringValue.h>
#include <opencog/atoms/value/FloatValue.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/atom_types/NameServer.h>
#include <opencog/atomspace/AtomSpace.h>

// Type definitions for AtomSpace
using namespace opencog;

namespace agentzero {
namespace communication {

// Constructor
LanguageProcessor::LanguageProcessor(AtomSpacePtr atomspace, const LanguageProcessorConfig& config)
    : _atomspace(atomspace), _config(config), _dictionary(nullptr), _parse_options(nullptr),
      _total_parses(0), _successful_parses(0), _failed_parses(0), _total_parse_time(0.0),
      _initialized(false)
{
    try {
        initialize_link_grammar();
        _initialized = true;
    } catch (const std::exception& e) {
#ifdef HAVE_LINK_GRAMMAR
        _last_error = "Failed to initialize Link Grammar: " + std::string(e.what());
        _initialized = false;
#else
        // No Link Grammar — basic mode always succeeds
        (void)e;
        _initialized = true;
#endif
    }
}

// Destructor
LanguageProcessor::~LanguageProcessor()
{
    cleanup_link_grammar();
}

// Initialize Link Grammar components
void LanguageProcessor::initialize_link_grammar()
{
#ifdef HAVE_LINK_GRAMMAR
    // Create parse options
    _parse_options = parse_options_create();
    if (!_parse_options) {
        throw std::runtime_error("Failed to create Link Grammar parse options");
    }
    
    // Configure parse options
    parse_options_set_verbosity(_parse_options, _config.verbosity_level);
    parse_options_set_max_parse_time(_parse_options, _config.max_parse_time);
    parse_options_set_islands_ok(_parse_options, true);
    parse_options_set_short_length(_parse_options, 16);
    parse_options_set_max_memory(_parse_options, 0); // No limit
    
    // Load dictionary
    std::string dict_path = _config.dictionary_path.empty() ? 
        _config.language : _config.dictionary_path;
    
    _dictionary = dictionary_create_lang(dict_path.c_str());
    if (!_dictionary) {
        parse_options_delete(_parse_options);
        _parse_options = nullptr;
        throw std::runtime_error("Failed to load Link Grammar dictionary for language: " + 
                               dict_path);
    }
#else
    // Link Grammar not available — use basic NLP fallback
    _dictionary = nullptr;
    _parse_options = nullptr;
#endif
}

// Cleanup Link Grammar resources
void LanguageProcessor::cleanup_link_grammar()
{
#ifdef HAVE_LINK_GRAMMAR
    if (_parse_options) {
        parse_options_delete(_parse_options);
        _parse_options = nullptr;
    }
    
    if (_dictionary) {
        dictionary_delete(_dictionary);
        _dictionary = nullptr;
    }
#endif
}

// Validate input text
bool LanguageProcessor::validate_input(const std::string& text)
{
    if (text.empty()) {
        _last_error = "Input text is empty";
        return false;
    }
    
    if (text.length() > 4096) {
        _last_error = "Input text too long (> 4096 characters)";
        return false;
    }
    
    // Check for only whitespace
    if (std::all_of(text.begin(), text.end(), ::isspace)) {
        _last_error = "Input text contains only whitespace";
        return false;
    }
    
    return true;
}

// Parse text using Link Grammar (or basic NLP fallback)
ParseResult LanguageProcessor::parse_text(const std::string& text)
{
    ParseResult result;
    result.text = text;
    result.is_valid = false;
    result.linkage_cost = 0.0;
    
    if (!_initialized) {
        _last_error = "LanguageProcessor not properly initialized";
        return result;
    }
    
    if (!validate_input(text)) {
        return result;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    _last_parse_start = start_time;
    _total_parses++;
    
#ifdef HAVE_LINK_GRAMMAR
    try {
        // Create sentence
        Sentence sent = sentence_create(text.c_str(), _dictionary);
        if (!sent) {
            _failed_parses++;
            _last_error = "Failed to create sentence from text: " + text;
            return result;
        }
        
        // Parse sentence
        int num_linkages = sentence_parse(sent, _parse_options);
        
        if (num_linkages < 0) {
            sentence_delete(sent);
            _failed_parses++;
            
            if (num_linkages == -1) {
                _last_error = "Parse failed - sentence too complex or invalid";
            } else if (num_linkages == -2) {
                _last_error = "Parse failed - sentence too long";
            } else {
                _last_error = "Parse failed with error code: " + std::to_string(num_linkages);
            }
            return result;
        }
        
        if (num_linkages > 0) {
            // Get the best linkage (index 0)
            Linkage lkg = linkage_create(0, sent, _parse_options);
            if (lkg) {
                result = create_parse_result(sent, lkg, text);
                linkage_delete(lkg);
                _successful_parses++;
            } else {
                _failed_parses++;
                _last_error = "Failed to create linkage from parse result";
            }
        } else {
            _failed_parses++;
            _last_error = "No valid linkages found for text: " + text;
        }
        
        sentence_delete(sent);
        
    } catch (const std::exception& e) {
        _failed_parses++;
        _last_error = "Exception during parsing: " + std::string(e.what());
    }
#else
    // Basic NLP fallback: tokenize by whitespace and punctuation
    try {
        std::istringstream iss(text);
        std::string token;
        while (iss >> token) {
            // Strip trailing punctuation.
            // Cast to unsigned char is required: std::ispunct has undefined behavior
            // for values outside [0, UCHAR_MAX] (i.e., negative plain-char values).
            while (!token.empty() && std::ispunct(static_cast<unsigned char>(token.back()))) {
                token.pop_back();
            }
            if (!token.empty()) {
                result.words.push_back(token);
            }
        }
        result.is_valid = !result.words.empty();
        result.linkage_cost = 0.0;
        _successful_parses++;
        
        // Store in AtomSpace if enabled
        if (_config.store_in_atomspace && _atomspace) {
            result.atomspace_representation = convert_to_atomspace(result);
        }
    } catch (const std::exception& e) {
        _failed_parses++;
        _last_error = "Exception during basic tokenization: " + std::string(e.what());
    }
#endif
    
    // Update timing statistics
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    _total_parse_time += duration.count() / 1000.0; // Convert to milliseconds
    
    return result;
}

#ifdef HAVE_LINK_GRAMMAR
// Create ParseResult from Link Grammar structures
ParseResult LanguageProcessor::create_parse_result(Sentence sent, Linkage lkg, const std::string& text)
{
    ParseResult result;
    result.text = text;
    result.is_valid = true;
    result.linkage_cost = linkage_disjunct_cost(lkg);
    
    // Extract words
    int num_words = linkage_get_num_words(lkg);
    for (int i = 0; i < num_words; i++) {
        const char* word = linkage_get_word(lkg, i);
        if (word) {
            result.words.push_back(std::string(word));
        }
    }
    
    // Extract links
    int num_links = linkage_get_num_links(lkg);
    for (int i = 0; i < num_links; i++) {
        const char* label = linkage_get_link_label(lkg, i);
        int left_word = linkage_get_link_lword(lkg, i);
        int right_word = linkage_get_link_rword(lkg, i);
        
        if (label) {
            result.links.push_back(std::make_pair(
                std::string(label), 
                std::make_pair(left_word, right_word)
            ));
        }
    }
    
    // Create AtomSpace representation if enabled
    if (_config.store_in_atomspace && _atomspace) {
        result.atomspace_representation = convert_to_atomspace(result);
    }
    
    return result;
}
#endif

// Convert parse result to AtomSpace representation
Handle LanguageProcessor::convert_to_atomspace(const ParseResult& parse_result)
{
    if (!_atomspace) {
        return Handle::UNDEFINED;
    }
    
    try {
        // Create sentence node
        Handle sentence_node = _atomspace->add_node(CONCEPT_NODE, 
            std::string("sentence:" + std::to_string(std::hash<std::string>{}(parse_result.text))));
        
        // Store original text
        _atomspace->set_value(sentence_node, 
            _atomspace->add_node(PREDICATE_NODE, std::string("original_text")),
            createStringValue(std::vector<std::string>{parse_result.text}));
        
        // Store parse cost
        _atomspace->set_value(sentence_node,
            _atomspace->add_node(PREDICATE_NODE, std::string("parse_cost")),
            createFloatValue(std::vector<double>{parse_result.linkage_cost}));
        
        // Create word nodes and sequence
        HandleSeq word_handles;
        for (size_t i = 0; i < parse_result.words.size(); i++) {
            Handle word_node = _atomspace->add_node(CONCEPT_NODE, 
                std::string(parse_result.words[i] + "#" + std::to_string(i)));
            word_handles.push_back(word_node);
            
            // Link word to sentence
            _atomspace->add_link(MEMBER_LINK, HandleSeq{word_node, sentence_node});
        }
        
        // Create word sequence
        if (!word_handles.empty()) {
            Handle word_sequence = _atomspace->add_link(LIST_LINK, std::move(word_handles));
            _atomspace->add_link(EVALUATION_LINK,
                HandleSeq{_atomspace->add_node(PREDICATE_NODE, std::string("word_sequence")),
                         _atomspace->add_link(LIST_LINK, HandleSeq{sentence_node, word_sequence})});
        }
        
        // Create link structures
        for (const auto& link_info : parse_result.links) {
            const std::string& label = link_info.first;
            int left_idx = link_info.second.first;
            int right_idx = link_info.second.second;
            
            if (left_idx < static_cast<int>(word_handles.size()) && 
                right_idx < static_cast<int>(word_handles.size())) {
                
                Handle link_relation = _atomspace->add_node(PREDICATE_NODE, std::string("lg_link:" + label));
                Handle link_eval = _atomspace->add_link(EVALUATION_LINK,
                    HandleSeq{link_relation,
                             _atomspace->add_link(LIST_LINK, 
                                                 HandleSeq{word_handles[left_idx], word_handles[right_idx]})});
                
                // Associate with sentence
                _atomspace->add_link(MEMBER_LINK, HandleSeq{link_eval, sentence_node});
            }
        }
        
        return sentence_node;
        
    } catch (const std::exception& e) {
        _last_error = "Failed to convert to AtomSpace: " + std::string(e.what());
        return Handle::UNDEFINED;
    }
}

// Perform comprehensive semantic analysis
SemanticResult LanguageProcessor::analyze_text(const std::string& text)
{
    SemanticResult result;
    
    if (!_config.enable_semantic_analysis) {
        return result;
    }
    
    // First get syntactic parse
    ParseResult parse_result = parse_text(text);
    if (!parse_result.is_valid) {
        return result;
    }
    
    return analyze_semantics(parse_result);
}

// Analyze semantics from parse result
SemanticResult LanguageProcessor::analyze_semantics(const ParseResult& parse_result)
{
    SemanticResult result;
    
    try {
        // Extract entities (simple heuristic-based approach)
        result.entities = extract_entities(parse_result);
        
        // Extract concepts
        result.concepts = extract_concepts(parse_result);
        
        // Analyze sentiment
        result.sentiment_scores = analyze_sentiment(parse_result.text);
        
        // Create AtomSpace representations for concepts
        if (_atomspace && _config.store_in_atomspace) {
            for (const std::string& concept : result.concepts) {
                Handle concept_node = _atomspace->add_node(CONCEPT_NODE, std::string(concept));
                result.concept_atoms.push_back(concept_node);
                
                // Link to original parse if available
                if (parse_result.atomspace_representation != Handle::UNDEFINED) {
                    _atomspace->add_link(INHERITANCE_LINK, 
                        HandleSeq{concept_node, parse_result.atomspace_representation});
                }
            }
        }
        
    } catch (const std::exception& e) {
        _last_error = "Failed to analyze semantics: " + std::string(e.what());
    }
    
    return result;
}

// Extract named entities (simple pattern-based approach)
std::vector<std::string> LanguageProcessor::extract_entities(const ParseResult& parse_result)
{
    std::vector<std::string> entities;
    
    // Simple heuristics for entity extraction
    // Look for capitalized words that might be proper nouns
    std::regex proper_noun_pattern(R"(\b[A-Z][a-z]+(?:\s+[A-Z][a-z]+)*\b)");
    std::sregex_iterator begin(parse_result.text.begin(), parse_result.text.end(), proper_noun_pattern);
    std::sregex_iterator end;
    
    for (auto it = begin; it != end; ++it) {
        std::string entity = it->str();
        // Filter common false positives
        if (entity != "I" && entity != "The" && entity != "A" && entity != "An") {
            entities.push_back(entity);
        }
    }
    
    return entities;
}

// Extract key concepts from parse
std::vector<std::string> LanguageProcessor::extract_concepts(const ParseResult& parse_result)
{
    std::vector<std::string> concepts;
    
    // Extract nouns and important verbs as concepts
    for (const std::string& word : parse_result.words) {
        // Simple heuristic: words longer than 3 characters that aren't common function words
        if (word.length() > 3) {
            std::string lower_word = word;
            std::transform(lower_word.begin(), lower_word.end(), lower_word.begin(), ::tolower);
            
            // Skip common function words
            if (lower_word != "this" && lower_word != "that" && lower_word != "with" && 
                lower_word != "from" && lower_word != "they" && lower_word != "have" &&
                lower_word != "will" && lower_word != "been" && lower_word != "were" &&
                lower_word != "said" && lower_word != "what" && lower_word != "when") {
                concepts.push_back(lower_word);
            }
        }
    }
    
    // Remove duplicates
    std::sort(concepts.begin(), concepts.end());
    concepts.erase(std::unique(concepts.begin(), concepts.end()), concepts.end());
    
    return concepts;
}

// Simple sentiment analysis
std::map<std::string, double> LanguageProcessor::analyze_sentiment(const std::string& text)
{
    std::map<std::string, double> sentiment;
    
    // Simple lexicon-based approach
    std::vector<std::string> positive_words = {"good", "great", "excellent", "amazing", "wonderful", "fantastic", "love", "like", "enjoy", "happy", "joy", "pleased"};
    std::vector<std::string> negative_words = {"bad", "terrible", "awful", "horrible", "hate", "dislike", "sad", "angry", "frustrated", "disappointed", "worried", "concerned"};
    
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
    
    double positive_score = 0.0;
    double negative_score = 0.0;
    
    for (const std::string& word : positive_words) {
        size_t pos = 0;
        while ((pos = lower_text.find(word, pos)) != std::string::npos) {
            positive_score += 1.0;
            pos += word.length();
        }
    }
    
    for (const std::string& word : negative_words) {
        size_t pos = 0;
        while ((pos = lower_text.find(word, pos)) != std::string::npos) {
            negative_score += 1.0;
            pos += word.length();
        }
    }
    
    sentiment["positive"] = positive_score;
    sentiment["negative"] = negative_score;
    sentiment["neutral"] = std::max(0.0, 1.0 - positive_score - negative_score);
    
    return sentiment;
}

// Batch processing
std::vector<ParseResult> LanguageProcessor::parse_batch(const std::vector<std::string>& texts)
{
    std::vector<ParseResult> results;
    results.reserve(texts.size());
    
    for (size_t i = 0; i < texts.size(); i++) {
        if (_progress_callback) {
            _progress_callback(i, texts.size());
        }
        
        results.push_back(parse_text(texts[i]));
    }
    
    if (_progress_callback) {
        _progress_callback(texts.size(), texts.size());
    }
    
    return results;
}

// Get AtomSpace representation
Handle LanguageProcessor::get_atomspace_representation(const std::string& text)
{
    ParseResult result = parse_text(text);
    return result.atomspace_representation;
}

// Update configuration
bool LanguageProcessor::update_config(const LanguageProcessorConfig& new_config)
{
    if (!validate_config(new_config)) {
        return false;
    }
    
    try {
        // If language changed, reinitialize Link Grammar
        if (new_config.language != _config.language || 
            new_config.dictionary_path != _config.dictionary_path) {
            cleanup_link_grammar();
            _config = new_config;
            initialize_link_grammar();
        } else {
            _config = new_config;
            
            // Update parse options
            if (_parse_options) {
                parse_options_set_verbosity(_parse_options, _config.verbosity_level);
                parse_options_set_max_parse_time(_parse_options, _config.max_parse_time);
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        _last_error = "Failed to update configuration: " + std::string(e.what());
        return false;
    }
}

// Get configuration
const LanguageProcessorConfig& LanguageProcessor::get_config() const
{
    return _config;
}

// Check if initialized
bool LanguageProcessor::is_initialized() const
{
    return _initialized;
}

// Get statistics
std::map<std::string, double> LanguageProcessor::get_statistics() const
{
    std::map<std::string, double> stats;
    
    stats["total_parses"] = static_cast<double>(_total_parses);
    stats["successful_parses"] = static_cast<double>(_successful_parses);
    stats["failed_parses"] = static_cast<double>(_failed_parses);
    stats["success_rate"] = _total_parses > 0 ? 
        static_cast<double>(_successful_parses) / _total_parses : 0.0;
    stats["average_parse_time_ms"] = _total_parses > 0 ? 
        _total_parse_time / _total_parses : 0.0;
    
    return stats;
}

// Reset statistics
void LanguageProcessor::reset_statistics()
{
    _total_parses = 0;
    _successful_parses = 0;
    _failed_parses = 0;
    _total_parse_time = 0.0;
}

// Set progress callback
void LanguageProcessor::set_progress_callback(std::function<void(size_t, size_t)> callback)
{
    _progress_callback = callback;
}

// Utility functions

LanguageProcessorConfig create_english_config()
{
    LanguageProcessorConfig config;
    config.dictionary_path = "";
    config.language = "en";
    config.max_parse_time = 10;
    config.verbosity_level = 0;
    config.store_in_atomspace = true;
    config.enable_semantic_analysis = true;
    config.confidence_threshold = 0.5;
    return config;
}

LanguageProcessorConfig create_russian_config()
{
    LanguageProcessorConfig config;
    config.dictionary_path = "";
    config.language = "ru";
    config.max_parse_time = 10;
    config.verbosity_level = 0;
    config.store_in_atomspace = true;
    config.enable_semantic_analysis = true;
    config.confidence_threshold = 0.5;
    return config;
}

bool validate_config(const LanguageProcessorConfig& config)
{
    if (config.language.empty()) {
        return false;
    }
    
    if (config.max_parse_time <= 0) {
        return false;
    }
    
    if (config.verbosity_level < 0 || config.verbosity_level > 6) {
        return false;
    }
    
    if (config.confidence_threshold < 0.0 || config.confidence_threshold > 1.0) {
        return false;
    }
    
    return true;
}

} // namespace communication
} // namespace agentzero
