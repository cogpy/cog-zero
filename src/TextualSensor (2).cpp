/*
 * src/TextualSensor.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * TextualSensor Implementation
 * Streaming text ingestion with salience scoring
 * Part of the AGENT-ZERO-GENESIS project - Phase 2 Perception
 */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/TextualSensor.h"

using namespace opencog;
using namespace opencog::agentzero;

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

TextualSensor::TextualSensor(AtomSpacePtr atomspace,
                             Handle agent_self,
                             TextProcessingMode mode)
    : _atomspace(atomspace)
    , _agent_self(agent_self)
    , _mode(mode)
{
    if (!_atomspace) {
        throw std::invalid_argument("[TextualSensor] AtomSpace cannot be null");
    }
    if (_agent_self == Handle::UNDEFINED) {
        throw std::invalid_argument("[TextualSensor] agent_self cannot be undefined");
    }
    logger().info() << "[TextualSensor] Initialized";
}

TextualSensor::~TextualSensor()
{
    logger().info() << "[TextualSensor] Destroyed after processing "
                    << _processed_units.load() << " units";
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::string TextualSensor::normalizeText(const std::string& text) const
{
    std::string out;
    out.reserve(text.size());
    bool prev_space = false;
    for (unsigned char c : text) {
        if (std::isspace(c)) {
            if (!prev_space && !out.empty()) {
                out += ' ';
            }
            prev_space = true;
        } else {
            out += static_cast<char>(c);
            prev_space = false;
        }
    }
    // Trim trailing space
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::vector<std::string> TextualSensor::tokenize(const std::string& text) const
{
    std::vector<std::string> tokens;
    std::istringstream iss(text);
    std::string token;
    while (iss >> token) {
        // Strip leading/trailing punctuation for vocab tracking
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<std::string> TextualSensor::splitSentences(const std::string& text) const
{
    std::vector<std::string> sentences;
    std::string current;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        current += c;
        if (c == '.' || c == '!' || c == '?') {
            // Peek to make sure it's really an end (not abbreviation)
            if (i + 1 >= text.size() || std::isspace(static_cast<unsigned char>(text[i + 1]))) {
                std::string s = normalizeText(current);
                if (!s.empty()) sentences.push_back(s);
                current.clear();
            }
        }
    }
    // Remaining text without terminal punctuation
    std::string remainder = normalizeText(current);
    if (!remainder.empty()) sentences.push_back(remainder);
    return sentences;
}

// ---------------------------------------------------------------------------
// Salience scoring
// ---------------------------------------------------------------------------

TextSalienceScore TextualSensor::scoreUnit(const std::string& unit)
{
    return calculateSalience(unit);
}

TextSalienceScore TextualSensor::calculateSalience(const std::string& unit)
{
    if (unit.empty()) {
        return TextSalienceScore{0.0, 0.0, 0.0, 0.0};
    }

    std::vector<std::string> words = tokenize(unit);
    if (words.empty()) {
        return TextSalienceScore{0.0, 0.0, 0.0, 0.0};
    }

    // --- Lexical: unique word ratio (type-token ratio capped at 1)
    std::unordered_map<std::string, size_t> local_freq;
    for (const auto& w : words) {
        std::string lower = w;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        local_freq[lower]++;
    }
    double lexical = static_cast<double>(local_freq.size()) /
                     static_cast<double>(words.size());

    // --- Length: logarithmic scaling, saturates at ~50 words
    double length = std::min(1.0, std::log1p(static_cast<double>(words.size())) /
                                      std::log1p(50.0));

    // --- Novelty: fraction of words not yet in global vocabulary
    size_t novel_count = 0;
    {
        std::lock_guard<std::mutex> lock(_vocab_mutex);
        for (const auto& [word, freq] : local_freq) {
            (void)freq; // only the key (word) is needed here
            if (_word_freq.find(word) == _word_freq.end()) {
                novel_count++;
            }
            _word_freq[word]++;
        }
    }
    double novelty = (local_freq.empty())
                     ? 0.0
                     : static_cast<double>(novel_count) /
                           static_cast<double>(local_freq.size());

    // --- Overall: weighted combination
    double overall = 0.35 * lexical + 0.25 * length + 0.40 * novelty;
    overall = std::max(0.0, std::min(1.0, overall));

    return TextSalienceScore{lexical, length, novelty, overall};
}

// ---------------------------------------------------------------------------
// Atom creation
// ---------------------------------------------------------------------------

Handle TextualSensor::createTextAtom(const std::string& unit,
                                     const TextSalienceScore& score)
{
    // Create a concept node for this text unit
    Handle text_node = _atomspace->add_node(CONCEPT_NODE, "text:" + unit);

    // Annotate with salience as truth value
    TruthValuePtr tv = SimpleTruthValue::createTV(score.overall, score.lexical);
    text_node->setTruthValue(tv);

    return text_node;
}

Handle TextualSensor::linkToAgent(const Handle& text_atom,
                                  const TextSalienceScore& score)
{
    // (EvaluationLink
    //   (PredicateNode "perceives_text")
    //   (ListLink agent_self text_atom))
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "perceives_text");

    HandleSeq list_seq;
    list_seq.push_back(_agent_self);
    list_seq.push_back(text_atom);
    Handle list_lnk = _atomspace->add_link(LIST_LINK, std::move(list_seq));

    HandleSeq eval_seq;
    eval_seq.push_back(pred);
    eval_seq.push_back(list_lnk);
    Handle eval_lnk = _atomspace->add_link(EVALUATION_LINK, std::move(eval_seq));

    TruthValuePtr tv = SimpleTruthValue::createTV(score.overall, 0.9);
    eval_lnk->setTruthValue(tv);

    return eval_lnk;
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void TextualSensor::add_text(const std::string& text)
{
    if (text.empty()) return;
    std::lock_guard<std::mutex> lock(_queue_mutex);
    _input_queue.push(text);
    _queued_count++;
}

size_t TextualSensor::queue_size() const
{
    std::lock_guard<std::mutex> lock(_queue_mutex);
    return _input_queue.size();
}

std::vector<Handle> TextualSensor::processText(const std::string& text)
{
    std::vector<Handle> results;

    // Segment according to mode
    std::vector<std::string> units;
    switch (_mode) {
        case TextProcessingMode::WORDS:
            units = tokenize(text);
            break;
        case TextProcessingMode::SENTENCES:
            units = splitSentences(text);
            break;
        case TextProcessingMode::DOCUMENTS:
            units.push_back(normalizeText(text));
            break;
        case TextProcessingMode::STREAM:
            units.push_back(text);
            break;
    }

    for (const auto& unit : units) {
        if (unit.empty()) continue;
        TextSalienceScore score = scoreUnit(unit);
        Handle text_atom = createTextAtom(unit, score);
        Handle eval_link = linkToAgent(text_atom, score);
        results.push_back(eval_link);
        _processed_units++;
    }

    logger().debug() << "[TextualSensor] Processed " << units.size()
                     << " unit(s) from text";
    return results;
}

std::vector<Handle> TextualSensor::processNext()
{
    std::string text;
    {
        std::lock_guard<std::mutex> lock(_queue_mutex);
        if (_input_queue.empty()) return {};
        text = std::move(_input_queue.front());
        _input_queue.pop();
    }
    return processText(text);
}

std::vector<Handle> TextualSensor::processAll()
{
    std::vector<Handle> all_results;
    while (true) {
        std::string text;
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            if (_input_queue.empty()) break;
            text = std::move(_input_queue.front());
            _input_queue.pop();
        }
        auto batch = processText(text);
        all_results.insert(all_results.end(), batch.begin(), batch.end());
    }
    return all_results;
}

SensoryInput TextualSensor::toSensoryInput(const std::string& unit,
                                           const TextSalienceScore& score) const
{
    // Encode text as a vector of character code-points (simplified)
    std::vector<double> data;
    data.reserve(unit.size());
    for (unsigned char c : unit) {
        data.push_back(static_cast<double>(c) / 255.0);
    }
    return SensoryInput("textual", "text_stream", data, score.overall);
}

std::string TextualSensor::getStats() const
{
    size_t q_size;
    {
        std::lock_guard<std::mutex> lock(_queue_mutex);
        q_size = _input_queue.size();
    }
    size_t vocab_size;
    {
        std::lock_guard<std::mutex> lock(_vocab_mutex);
        vocab_size = _word_freq.size();
    }

    std::ostringstream ss;
    ss << "{";
    ss << "\"processed_units\":" << _processed_units.load() << ",";
    ss << "\"queued_count\":" << _queued_count.load() << ",";
    ss << "\"queue_size\":" << q_size << ",";
    ss << "\"vocabulary_size\":" << vocab_size;
    ss << "}";
    return ss.str();
}

void TextualSensor::resetVocabulary()
{
    std::lock_guard<std::mutex> lock(_vocab_mutex);
    _word_freq.clear();
    logger().info() << "[TextualSensor] Vocabulary reset";
}
