/*
 * standalone/src/TextualSensor.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "cog0/TextualSensor.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace cog0 {

TextualSensor::TextualSensor(const std::string& name)
    : _name(name)
{
}

PerceptInput TextualSensor::ingest(const std::string& text)
{
    double salience = computeSalience(text);
    return ingestWithSalience(text, salience);
}

PerceptInput TextualSensor::ingestWithSalience(const std::string& text, double salience)
{
    PerceptInput p;
    p.source   = _name;
    p.modality = "text";
    p.content  = text;
    p.salience = salience;
    ++_processedCount;
    return p;
}

double TextualSensor::computeSalience(const std::string& text) const
{
    if (text.empty()) return 0.0;

    // Base score from text length (longer text = higher base salience, capped)
    double lengthScore = std::min(1.0, static_cast<double>(text.size()) / 200.0);

    // Keyword boost: check for presence of salient keywords (case-insensitive)
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    double keywordBoost = 0.0;
    for (const auto& kw : salientKeywords()) {
        if (lower.find(kw) != std::string::npos) {
            keywordBoost += 0.1;
        }
    }

    double salience = 0.3 * lengthScore + 0.7 * std::min(1.0, 0.3 + keywordBoost);
    return std::min(1.0, std::max(0.0, salience));
}

// static
const std::vector<std::string>& TextualSensor::salientKeywords()
{
    static const std::vector<std::string> keywords = {
        "goal", "task", "important", "critical", "urgent",
        "error", "fail", "success", "complete", "warning",
        "alert", "priority", "key", "main", "primary"
    };
    return keywords;
}

} // namespace cog0
