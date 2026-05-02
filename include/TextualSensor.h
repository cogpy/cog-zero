/*
 * include/opencog/agentzero/TextualSensor.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * TextualSensor - Streaming text ingestion with salience scoring
 * Part of the AGENT-ZERO-GENESIS project - Phase 2 Perception
 */

#ifndef _OPENCOG_AGENTZERO_TEXTUAL_SENSOR_H
#define _OPENCOG_AGENTZERO_TEXTUAL_SENSOR_H

#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <sstream>
#include <chrono>

#include <opencog/atoms/base/Handle.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

#include "PerceptualProcessor.h"

namespace opencog
{
namespace agentzero
{

/**
 * @brief Text processing granularity modes
 */
enum class TextProcessingMode
{
    WORDS,      ///< Tokenise into individual words
    SENTENCES,  ///< Split on sentence boundaries (. ! ?)
    DOCUMENTS,  ///< Treat each add_text() call as a whole document
    STREAM      ///< Pass every character sequence straight through
};

/**
 * @brief Per-unit salience breakdown for ingested text
 */
struct TextSalienceScore
{
    double lexical;    ///< Vocabulary richness / term frequency inverse (0-1)
    double length;     ///< Normalised unit length influence (0-1)
    double novelty;    ///< Fraction of unseen word forms (0-1)
    double overall;    ///< Weighted combination (0-1)

    TextSalienceScore()
        : lexical(0.5), length(0.5), novelty(0.5), overall(0.5) {}
    TextSalienceScore(double lex, double len, double nov, double ov)
        : lexical(lex), length(len), novelty(nov), overall(ov) {}
};

/**
 * @brief TextualSensor — streaming text ingestion with salience scoring
 *
 * Accepts raw text via add_text(), segments it according to the selected
 * TextProcessingMode, calculates a per-segment salience score, and converts
 * each segment into an AtomSpace representation using linked CONCEPT_NODE and
 * EVALUATION_LINK atoms.  Each produced atom is associated with a SensoryInput
 * so that it can flow through the PerceptualProcessor pipeline.
 *
 * Thread-safety: add_text() and process*() methods are individually thread-safe.
 */
class TextualSensor
{
private:
    AtomSpacePtr _atomspace;
    Handle _agent_self;
    TextProcessingMode _mode;

    // Input queue (thread-safe)
    mutable std::mutex _queue_mutex;
    std::queue<std::string> _input_queue;

    // Vocabulary for novelty scoring
    mutable std::mutex _vocab_mutex;
    std::unordered_map<std::string, size_t> _word_freq;

    // Statistics
    std::atomic<size_t> _processed_units{0};
    std::atomic<size_t> _queued_count{0};

    // Internal helpers
    std::vector<std::string> tokenize(const std::string& text) const;
    std::vector<std::string> splitSentences(const std::string& text) const;
    std::string normalizeText(const std::string& text) const;

    Handle createTextAtom(const std::string& unit,
                          const TextSalienceScore& score);
    Handle linkToAgent(const Handle& text_atom,
                       const TextSalienceScore& score);

    TextSalienceScore scoreUnit(const std::string& unit);

public:
    /**
     * @brief Constructor
     * @param atomspace  Shared pointer to the AtomSpace
     * @param agent_self Handle to the agent's self-representation atom
     * @param mode       Text segmentation mode (default: SENTENCES)
     */
    TextualSensor(AtomSpacePtr atomspace,
                  Handle agent_self,
                  TextProcessingMode mode = TextProcessingMode::SENTENCES);

    /**
     * @brief Destructor
     */
    ~TextualSensor();

    // -----------------------------------------------------------------------
    // Input management
    // -----------------------------------------------------------------------

    /**
     * @brief Enqueue text for processing
     *
     * Thread-safe.  The text is stored in an internal queue and processed
     * lazily when processNext() or processAll() is called.
     *
     * @param text Input text to enqueue
     */
    void add_text(const std::string& text);

    /**
     * @brief Return the number of items currently in the queue
     */
    size_t queue_size() const;

    // -----------------------------------------------------------------------
    // Processing
    // -----------------------------------------------------------------------

    /**
     * @brief Process the next item from the queue
     *
     * Segments the next queued text into units according to the current mode,
     * scores each unit for salience, and converts each to an AtomSpace atom.
     *
     * @return Handles to created atoms (empty if queue was empty)
     */
    std::vector<Handle> processNext();

    /**
     * @brief Process all queued items
     * @return All created handles in insertion order
     */
    std::vector<Handle> processAll();

    /**
     * @brief Process a single text string directly (bypasses the queue)
     *
     * Useful for synchronous, one-shot ingestion.
     *
     * @param text Input text
     * @return Handles to created atoms
     */
    std::vector<Handle> processText(const std::string& text);

    // -----------------------------------------------------------------------
    // Salience scoring (public for use by AttentionManager)
    // -----------------------------------------------------------------------

    /**
     * @brief Calculate salience of a text unit
     * @param unit  The text fragment to score
     * @return      Decomposed TextSalienceScore
     */
    TextSalienceScore calculateSalience(const std::string& unit);

    /**
     * @brief Build a SensoryInput from a text unit for pipeline integration
     * @param unit  Text fragment
     * @param score Pre-computed salience score
     * @return      SensoryInput ready for PerceptualProcessor
     */
    SensoryInput toSensoryInput(const std::string& unit,
                                const TextSalienceScore& score) const;

    // -----------------------------------------------------------------------
    // Configuration
    // -----------------------------------------------------------------------

    /**
     * @brief Change the processing mode
     * @param mode New TextProcessingMode
     */
    void setMode(TextProcessingMode mode) { _mode = mode; }

    /**
     * @brief Get the current processing mode
     */
    TextProcessingMode getMode() const { return _mode; }

    // -----------------------------------------------------------------------
    // Diagnostics
    // -----------------------------------------------------------------------

    /**
     * @brief Return JSON-formatted processing statistics
     */
    std::string getStats() const;

    /**
     * @brief Number of text units processed since construction (or last reset)
     */
    size_t processedUnitCount() const { return _processed_units.load(); }

    /**
     * @brief Clear internal vocabulary (resets novelty baseline)
     */
    void resetVocabulary();
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_TEXTUAL_SENSOR_H
