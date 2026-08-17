/*
 * opencog/agentzero/communication/LanguageProcessor.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * LanguageProcessor — NLU/NLG implementation (Phase 6 / AZ-NLP-001)
 */

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/communication/LanguageProcessor.h"

#ifdef HAVE_LG_ATOMESE
// Native lg-atomese integration point — linked when the package is present.
// Parsing still uses the portable fallback so unit tests stay deterministic.
#endif

using namespace opencog;
using namespace opencog::agentzero::communication;

LanguageProcessor::LanguageProcessor(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
{
    if (!_atomspace)
        throw std::runtime_error("LanguageProcessor requires a valid AtomSpace");

#ifdef HAVE_LG_ATOMESE
    _have_lg_atomese = true;
    _use_link_grammar = true;
    logger().info() << "[LanguageProcessor] lg-atomese support enabled";
#else
    _have_lg_atomese = false;
    _use_link_grammar = false;
    logger().info() << "[LanguageProcessor] basic NLP fallback active";
#endif

    ensureNlpContext();
}

LanguageProcessor::~LanguageProcessor() = default;

void LanguageProcessor::ensureNlpContext()
{
    if (_nlp_context != Handle::UNDEFINED) return;
    _nlp_context = _atomspace->add_node(CONCEPT_NODE, "LanguageProcessorContext");
    _nlp_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
}

std::string LanguageProcessor::toLower(const std::string& s) const
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::vector<std::string> LanguageProcessor::tokenize(const std::string& text) const
{
    std::vector<std::string> tokens;
    std::string current;
    for (char ch : text) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '\'' || ch == '-') {
            current.push_back(ch);
        } else {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            // Keep sentence-level punctuation as its own token
            if (ch == '?' || ch == '!' || ch == '.' || ch == ',') {
                tokens.emplace_back(1, ch);
            }
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

std::string LanguageProcessor::detectIntent(const std::string& text) const
{
    if (text.empty()) return "unknown";
    const std::string lower = toLower(text);

    auto has_word = [&](const std::string& w) {
        // crude whole-word check
        size_t pos = 0;
        while ((pos = lower.find(w, pos)) != std::string::npos) {
            bool left_ok = (pos == 0) || !std::isalnum(static_cast<unsigned char>(lower[pos - 1]));
            size_t end = pos + w.size();
            bool right_ok = (end >= lower.size()) || !std::isalnum(static_cast<unsigned char>(lower[end]));
            if (left_ok && right_ok) return true;
            pos = end;
        }
        return false;
    };

    if (has_word("hello") || has_word("hi") || has_word("hey") || has_word("greetings"))
        return "greeting";
    if (has_word("bye") || has_word("goodbye") || has_word("farewell"))
        return "farewell";
    if (has_word("please") || has_word("could you") || has_word("would you") ||
        has_word("help") || has_word("make me") || has_word("can you"))
        return "request";

    // Question: explicit '?' or interrogative leading words / auxiliaries.
    auto starts_with_word = [&](const std::string& w) {
        if (lower.rfind(w, 0) != 0) return false;
        if (lower.size() == w.size()) return true;
        return !std::isalnum(static_cast<unsigned char>(lower[w.size()]));
    };
    if (lower.find('?') != std::string::npos ||
        has_word("what") || has_word("why") || has_word("how") ||
        has_word("when") || has_word("where") || has_word("who") ||
        has_word("which") ||
        starts_with_word("is") || starts_with_word("are") ||
        starts_with_word("can") || starts_with_word("do") ||
        starts_with_word("does") || starts_with_word("did"))
        return "question";
    return "statement";
}

std::vector<std::string> LanguageProcessor::extractEntities(const std::string& text) const
{
    std::vector<std::string> entities;
    // Quoted spans
    bool in_quote = false;
    std::string quoted;
    for (char ch : text) {
        if (ch == '"' || ch == '\'') {
            if (in_quote) {
                if (!quoted.empty()) entities.push_back(quoted);
                quoted.clear();
                in_quote = false;
            } else {
                in_quote = true;
            }
            continue;
        }
        if (in_quote) quoted.push_back(ch);
    }

    // Capitalized multi-word / single-word tokens (skip sentence start noise lightly)
    auto tokens = tokenize(text);
    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& t = tokens[i];
        if (t.size() < 2) continue;
        if (std::isupper(static_cast<unsigned char>(t[0])) &&
            std::islower(static_cast<unsigned char>(t[1]))) {
            // Skip pure intent words even if capitalized
            std::string low = toLower(t);
            if (low == "i" || low == "hello" || low == "hi") continue;
            entities.push_back(t);
        }
    }

    // Deduplicate preserving order
    std::vector<std::string> unique;
    for (const auto& e : entities) {
        if (std::find(unique.begin(), unique.end(), e) == unique.end())
            unique.push_back(e);
    }
    return unique;
}

double LanguageProcessor::calculateConfidence(const std::string& text,
                                              const std::string& intent,
                                              size_t entity_count) const
{
    if (text.empty()) return 0.0;
    double conf = 0.45;
    if (intent != "unknown" && intent != "statement") conf += 0.25;
    if (text.find('?') != std::string::npos || text.find('!') != std::string::npos)
        conf += 0.1;
    if (entity_count > 0) conf += std::min(0.15, 0.05 * static_cast<double>(entity_count));
    if (text.size() >= 8) conf += 0.05;
    if (_use_link_grammar && _have_lg_atomese) conf += 0.05;
    return std::min(0.99, conf);
}

ParseResult LanguageProcessor::parseText(const std::string& text)
{
    ParseResult result;
    result.original_text = text;

    if (text.empty()) {
        result.success = false;
        result.intent = "unknown";
        return result;
    }

    ensureNlpContext();

    result.tokens = tokenize(text);
    result.intent = detectIntent(text);
    result.detected_entities = extractEntities(text);

    Handle text_atom = textToAtoms(text);
    if (text_atom != Handle::UNDEFINED)
        result.parsed_atoms.push_back(text_atom);

    // Attach intent as EvaluationLink(intent-is, (text, intent-concept))
    Handle intent_pred = _atomspace->add_node(PREDICATE_NODE, "intent-is");
    Handle intent_node = _atomspace->add_node(CONCEPT_NODE, "intent:" + result.intent);
    Handle intent_link = _atomspace->add_link(
        EVALUATION_LINK,
        intent_pred,
        _atomspace->add_link(LIST_LINK, text_atom, intent_node));
    result.parsed_atoms.push_back(intent_link);

    for (const auto& ent : result.detected_entities) {
        Handle ent_node = _atomspace->add_node(CONCEPT_NODE, "entity:" + ent);
        Handle mentions = _atomspace->add_node(PREDICATE_NODE, "mentions");
        Handle elink = _atomspace->add_link(
            EVALUATION_LINK,
            mentions,
            _atomspace->add_link(LIST_LINK, text_atom, ent_node));
        result.parsed_atoms.push_back(elink);
    }

    result.confidence = calculateConfidence(text, result.intent,
                                            result.detected_entities.size());
    result.success = true;

    {
        std::lock_guard<std::mutex> lock(_stats_mutex);
        ++_parses_performed;
    }

    logger().debug() << "[LanguageProcessor] parsed intent=" << result.intent
                     << " conf=" << result.confidence
                     << " tokens=" << result.tokens.size();
    return result;
}

std::string LanguageProcessor::generateQuestionResponse(const std::string& question) const
{
    const std::string lower = toLower(question);
    if (lower.find("who are you") != std::string::npos ||
        lower.find("your name") != std::string::npos)
        return "I am Agent-Zero, a cognitive agent with AtomSpace-backed dialogue.";
    if (lower.find("what can you") != std::string::npos ||
        lower.find("help") != std::string::npos)
        return "I can parse language, manage multi-turn dialogue, and exchange messages with other agents.";
    if (lower.find("how are you") != std::string::npos)
        return "I am operational and ready to assist.";
    if (lower.find("time") != std::string::npos)
        return "I track dialogue turns rather than wall-clock scheduling in this module.";
    return "That is an interesting question. Could you provide a bit more detail?";
}

std::string LanguageProcessor::generateDefaultResponse(const std::string& input,
                                                       const std::string& intent) const
{
    if (intent == "request")
        return "I will do my best to help with that request.";
    if (intent == "statement")
        return "Understood. I have noted: \"" + input + "\".";
    return "I heard you. Please tell me more.";
}

std::string LanguageProcessor::generateResponse(const std::string& input_text,
                                                const std::string& context)
{
    if (input_text.empty()) return "";

    std::string intent = detectIntent(input_text);
    std::string response;

    if (!context.empty()) {
        std::string ctx_lower = toLower(context);
        auto pos = ctx_lower.find("topic:");
        if (pos != std::string::npos) {
            std::string topic = context.substr(pos + 6);
            // trim
            while (!topic.empty() && std::isspace(static_cast<unsigned char>(topic.front())))
                topic.erase(topic.begin());
            response = "Regarding " + topic + ", " +
                       generateDefaultResponse(input_text, intent);
        }
    }

    if (response.empty()) {
        if (intent == "greeting")
            response = "Hello! How can I assist you today?";
        else if (intent == "farewell")
            response = "Goodbye! It was nice talking with you.";
        else if (intent == "question")
            response = generateQuestionResponse(input_text);
        else
            response = generateDefaultResponse(input_text, intent);
    }

    {
        std::lock_guard<std::mutex> lock(_stats_mutex);
        ++_responses_generated;
    }
    return response;
}

Handle LanguageProcessor::textToAtoms(const std::string& text)
{
    ensureNlpContext();
    if (text.empty()) return Handle::UNDEFINED;

    Handle text_node = _atomspace->add_node(CONCEPT_NODE, "utterance:" + text);
    text_node->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));

    auto tokens = tokenize(text);
    if (!tokens.empty()) {
        HandleSeq token_handles;
        token_handles.reserve(tokens.size());
        for (const auto& tok : tokens) {
            token_handles.push_back(_atomspace->add_node(CONCEPT_NODE, "token:" + tok));
        }
        Handle token_list = _atomspace->add_link(LIST_LINK, token_handles);
        Handle has_tokens = _atomspace->add_node(PREDICATE_NODE, "has-tokens");
        _atomspace->add_link(EVALUATION_LINK, has_tokens,
                             _atomspace->add_link(LIST_LINK, text_node, token_list));
    }

    _atomspace->add_link(MEMBER_LINK, text_node, _nlp_context);
    return text_node;
}

std::string LanguageProcessor::atomsToText(const std::vector<Handle>& atoms) const
{
    if (atoms.empty()) return "";
    std::ostringstream oss;
    bool first = true;
    for (const auto& h : atoms) {
        if (h == Handle::UNDEFINED || !h) continue;
        std::string name = h->get_name();
        // Strip common prefixes used by textToAtoms
        const char* prefixes[] = {"utterance:", "token:", "entity:", "intent:"};
        for (const char* p : prefixes) {
            std::string pref(p);
            if (name.rfind(pref, 0) == 0) {
                name = name.substr(pref.size());
                break;
            }
        }
        if (name.empty()) continue;
        if (!first) oss << " ";
        oss << name;
        first = false;
    }
    return oss.str();
}

void LanguageProcessor::setUseLinkGrammar(bool enabled)
{
#ifndef HAVE_LG_ATOMESE
    if (enabled) {
        logger().warn() << "[LanguageProcessor] Link Grammar requested but lg-atomese not available";
    }
    _use_link_grammar = false;
#else
    _use_link_grammar = enabled && _have_lg_atomese;
#endif
}

bool LanguageProcessor::usesLinkGrammar() const
{
    return _use_link_grammar && _have_lg_atomese;
}

void LanguageProcessor::setLanguageModel(const std::string& model_path)
{
    _language_model = model_path;
}

size_t LanguageProcessor::getParseCount() const
{
    std::lock_guard<std::mutex> lock(_stats_mutex);
    return _parses_performed;
}

size_t LanguageProcessor::getResponseCount() const
{
    std::lock_guard<std::mutex> lock(_stats_mutex);
    return _responses_generated;
}
