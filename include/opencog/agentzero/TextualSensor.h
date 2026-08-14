/*
 * opencog/agentzero/TextualSensor.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Streaming text ingestion with salience scoring and AtomSpace encoding.
 * Part of AGENT-ZERO-GENESIS Phase 2 (Perception & Sensory Processing).
 */
#ifndef _OPENCOG_AGENTZERO_TEXTUAL_SENSOR_H
#define _OPENCOG_AGENTZERO_TEXTUAL_SENSOR_H

#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

#include "opencog/agentzero/MultiModalSensor.h"

namespace opencog {
namespace agentzero {

enum class TextProcessingMode {
    SENTENCES,
    WORDS,
    DOCUMENTS,
    STREAM
};

/**
 * Salience breakdown for a text fragment.
 */
struct TextSalienceScore {
    double lexical = 0.0;
    double length = 0.0;
    double novelty = 0.0;
    double overall = 0.0;

    TextSalienceScore() = default;
    TextSalienceScore(double lex, double len, double nov, double over)
        : lexical(lex), length(len), novelty(nov), overall(over)
    {}
};

/**
 * TextualSensor — queues streaming text, splits by mode, scores salience,
 * and creates CONCEPT atoms in AtomSpace associated with the agent self.
 */
class TextualSensor {
public:
    TextualSensor(AtomSpacePtr atomspace, Handle agent_self,
                  TextProcessingMode mode = TextProcessingMode::SENTENCES);
    ~TextualSensor();

    TextProcessingMode getMode() const;
    void setMode(TextProcessingMode mode);

    size_t queue_size() const;
    size_t processedUnitCount() const;

    /** Enqueue raw text (empty strings ignored). */
    void add_text(const std::string& text);

    /** Process the next queued item; returns created handles. */
    HandleSeq processNext();

    /** Drain the entire queue. */
    HandleSeq processAll();

    /**
     * Immediately process text according to the current mode
     * (does not use the queue).
     */
    HandleSeq processText(const std::string& text);

    TextSalienceScore calculateSalience(const std::string& text);

    /** Convert text + score into a SensoryInput sample. */
    SensoryInput toSensoryInput(const std::string& text,
                                const TextSalienceScore& score) const;

    std::string getStats() const;
    void resetVocabulary();

private:
    std::vector<std::string> splitUnits(const std::string& text) const;
    static std::vector<std::string> splitSentences(const std::string& text);
    static std::vector<std::string> splitWords(const std::string& text);
    Handle encodeUnit(const std::string& unit, const TextSalienceScore& score);
    static double clamp01(double v);

    AtomSpacePtr _atomspace;
    Handle _agent_self;
    Handle _text_root = Handle::UNDEFINED;

    mutable std::mutex _mu;
    TextProcessingMode _mode;
    std::queue<std::string> _queue;
    size_t _processed_units = 0;
    std::map<std::string, size_t> _vocabulary;
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_TEXTUAL_SENSOR_H
