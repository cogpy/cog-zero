/*
 * opencog/agentzero/LanguageProcessor.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * LanguageProcessor - Handles natural language processing tasks
 * Integrates with Link Grammar and lg-atomese for language understanding
 * Part of the AGENT-ZERO-GENESIS project Phase 6: Communication & NLP
 */

#ifndef _OPENCOG_AGENTZERO_LANGUAGE_PROCESSOR_H
#define _OPENCOG_AGENTZERO_LANGUAGE_PROCESSOR_H

#include <string>
#include <vector>
#include <memory>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

namespace opencog {
namespace agentzero {

/**
 * ParseResult represents the outcome of parsing natural language text
 */
struct ParseResult {
    std::string original_text;
    std::vector<Handle> parsed_atoms;
    std::vector<std::string> detected_entities;
    std::string intent;
    double confidence;
    bool success;
    
    ParseResult() : confidence(0.0), success(false) {}
};

/**
 * LanguageProcessor - Natural language processing integration
 *
 * This class provides natural language processing capabilities using
 * OpenCog's language processing stack including Link Grammar and lg-atomese.
 */
class LanguageProcessor {
private:
    AtomSpacePtr _atomspace;
    
    // Configuration
    bool _use_link_grammar;
    bool _use_lg_atomese;
    std::string _language_model;
    
    // Private helper methods
    double calculateConfidence(const std::string& text) const;
    std::string generateQuestionResponse(const std::string& question) const;
    std::string generateTopicResponse(const std::string& input, const std::string& topic) const;
    std::string generateDefaultResponse(const std::string& input) const;
    
public:
    LanguageProcessor(AtomSpacePtr atomspace);
    virtual ~LanguageProcessor();
    
    // Core NLP operations
    ParseResult parseText(const std::string& text);
    std::string generateResponse(const std::string& input_text, const std::string& context = "");
    
    // Intent and entity recognition
    std::string detectIntent(const std::string& text);
    std::vector<std::string> extractEntities(const std::string& text);
    
    // AtomSpace integration
    Handle textToAtoms(const std::string& text);
    std::string atomsToText(const std::vector<Handle>& atoms);
    
    // Configuration
    void setUseLinks(bool use_links);
    void setLanguageModel(const std::string& model_path);
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_LANGUAGE_PROCESSOR_H