/*
 * standalone/include/cog0/TextualSensor.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * TextualSensor — streaming text ingestion with automatic salience scoring.
 * Part of Phase 2: Perception & Sensory Processing.
 */

#pragma once

#include <string>
#include <vector>

#include "CognitiveLoop.h"  // PerceptInput

namespace cog0 {

// -----------------------------------------------------------------------
// TextualSensor

class TextualSensor {
public:
    explicit TextualSensor(const std::string& name = "textual-sensor");

    // Ingest a text string, computing salience automatically from the content.
    PerceptInput ingest(const std::string& text);

    // Ingest with an explicit salience value.
    PerceptInput ingestWithSalience(const std::string& text, double salience);

    // Compute a salience score for the given text in [0, 1].
    // Higher score for longer text, presence of salient keywords, etc.
    double computeSalience(const std::string& text) const;

    // Accessors
    const std::string& name()           const { return _name; }
    size_t             processedCount() const { return _processedCount; }

private:
    std::string _name;
    size_t      _processedCount = 0;

    // Singleton list of keywords that boost salience.
    static const std::vector<std::string>& salientKeywords();
};

} // namespace cog0
