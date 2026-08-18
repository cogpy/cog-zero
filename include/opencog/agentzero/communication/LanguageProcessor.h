/*
 * opencog/agentzero/communication/LanguageProcessor.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * LanguageProcessor — NLU/NLG with optional Link Grammar (lg-atomese)
 * Part of Phase 6: Communication & NLP (AZ-NLP-001)
 */

#ifndef _OPENCOG_AGENTZERO_COMMUNICATION_LANGUAGE_PROCESSOR_H
#define _OPENCOG_AGENTZERO_COMMUNICATION_LANGUAGE_PROCESSOR_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {
namespace communication {

/**
 * Outcome of parsing natural language text into AtomSpace form.
 */
struct ParseResult {
    std::string original_text;
    std::vector<Handle> parsed_atoms;
    std::vector<std::string> tokens;
    std::vector<std::string> detected_entities;
    std::string intent;
    double confidence = 0.0;
    bool success = false;
};

/**
 * LanguageProcessor — natural language understanding and generation.
 *
 * Responsibilities:
 * - Tokenize and parse free-form text into AtomSpace atoms
 * - Detect intents and extract simple entities
 * - Generate natural-language responses from intent/context
 * - Optionally use lg-atomese / Link Grammar when HAVE_LG_ATOMESE is set
 *
 * Without OpenCog NLP packages a deterministic rule-based fallback is used
 * so the module remains fully testable via the in-tree AtomSpace shim.
 */
class LanguageProcessor
{
public:
    explicit LanguageProcessor(AtomSpacePtr atomspace);
    virtual ~LanguageProcessor();

    LanguageProcessor(const LanguageProcessor&) = delete;
    LanguageProcessor& operator=(const LanguageProcessor&) = delete;

    /** Parse text into tokens, intent, entities, and AtomSpace atoms. */
    ParseResult parseText(const std::string& text);

    /** Generate a response for the given input (optional dialogue context). */
    std::string generateResponse(const std::string& input_text,
                                 const std::string& context = "");

    /** Rule-based intent label (greeting, question, farewell, request, statement…). */
    std::string detectIntent(const std::string& text) const;

    /** Extract capitalized / quoted entity-like spans. */
    std::vector<std::string> extractEntities(const std::string& text) const;

    /** Tokenize on whitespace and simple punctuation. */
    std::vector<std::string> tokenize(const std::string& text) const;

    /** Represent text as a ConceptNode (+ optional LIST_LINK of tokens). */
    Handle textToAtoms(const std::string& text);

    /** Best-effort reverse of textToAtoms for node names / list children. */
    std::string atomsToText(const std::vector<Handle>& atoms) const;

    void setUseLinkGrammar(bool enabled);
    bool usesLinkGrammar() const;

    void setLanguageModel(const std::string& model_path);
    const std::string& getLanguageModel() const { return _language_model; }

    size_t getParseCount() const;
    size_t getResponseCount() const;

    AtomSpacePtr getAtomSpace() const { return _atomspace; }

private:
    AtomSpacePtr _atomspace;
    bool _use_link_grammar = false;
    bool _have_lg_atomese = false;
    std::string _language_model;

    mutable std::mutex _stats_mutex;
    size_t _parses_performed = 0;
    size_t _responses_generated = 0;

    Handle _nlp_context = Handle::UNDEFINED;

    double calculateConfidence(const std::string& text,
                               const std::string& intent,
                               size_t entity_count) const;
    std::string toLower(const std::string& s) const;
    std::string generateQuestionResponse(const std::string& question) const;
    std::string generateDefaultResponse(const std::string& input,
                                        const std::string& intent) const;
    void ensureNlpContext();
};

} // namespace communication
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_COMMUNICATION_LANGUAGE_PROCESSOR_H
