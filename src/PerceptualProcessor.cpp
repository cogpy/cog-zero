/*
 * standalone/src/PerceptualProcessor.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "cog0/PerceptualProcessor.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace cog0 {

PerceptualProcessor::PerceptualProcessor(std::shared_ptr<AtomStore> store)
    : _store(std::move(store))
{
}

Handle PerceptualProcessor::process(const PerceptInput& p)
{
    if (p.modality == "text") {
        auto handles = processText(p.content, p.salience, p.source);
        if (!handles.empty()) return handles.front();
        // Fallback: create a single concept for the whole content
    }

    if (p.modality == "numeric") {
        try {
            double v = std::stod(p.content);
            return processNumeric(v, p.salience, p.source);
        } catch (...) {}
    }

    if (p.modality == "event") {
        return processEvent(p.content, p.salience, p.source);
    }

    // Generic fallback: create a CONCEPT atom with the raw content
    Handle h = _store->addNode(AtomType::CONCEPT, p.content);
    h->setSTI(p.salience);

    std::lock_guard<std::mutex> lock(_mutex);
    _history.push_back(h);
    return h;
}

std::vector<Handle> PerceptualProcessor::processText(const std::string& text,
                                                      double salience,
                                                      const std::string& source)
{
    auto tokens = tokenize(text);
    std::vector<Handle> result;
    result.reserve(tokens.size());

    for (const auto& tok : tokens) {
        if (tok.empty()) continue;

        std::string atomName = tok;
        if (!source.empty())
            atomName = source + "/" + tok;

        Handle h = _store->addNode(AtomType::CONCEPT, atomName);
        h->setSTI(salience);

        std::lock_guard<std::mutex> lock(_mutex);
        _history.push_back(h);

        result.push_back(h);
    }
    return result;
}

Handle PerceptualProcessor::processNumeric(double value, double salience,
                                            const std::string& source)
{
    std::ostringstream oss;
    oss << "numeric:" << value;
    if (!source.empty()) oss << "@" << source;

    Handle h = _store->addNode(AtomType::CONCEPT, oss.str());
    h->setSTI(salience);
    h->setTV({salience, 1.0});

    std::lock_guard<std::mutex> lock(_mutex);
    _history.push_back(h);
    return h;
}

Handle PerceptualProcessor::processEvent(const std::string& eventName, double salience,
                                          const std::string& source)
{
    std::string atomName = "event:" + eventName;
    if (!source.empty()) atomName += "@" + source;

    Handle h = _store->addNode(AtomType::CONCEPT, atomName);
    h->setSTI(salience);
    h->setTV({salience, 1.0});

    std::lock_guard<std::mutex> lock(_mutex);
    _history.push_back(h);
    return h;
}

std::vector<Handle> PerceptualProcessor::recentPercepts() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _history;
}

void PerceptualProcessor::clearHistory()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _history.clear();
}

size_t PerceptualProcessor::processedCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _history.size();
}

// static private
std::vector<std::string> PerceptualProcessor::tokenize(const std::string& text)
{
    std::vector<std::string> tokens;
    std::istringstream iss(text);
    std::string word;
    while (iss >> word) {
        auto norm = normalize(word);
        if (!norm.empty())
            tokens.push_back(norm);
    }
    return tokens;
}

// static private
std::string PerceptualProcessor::normalize(const std::string& word)
{
    std::string out;
    out.reserve(word.size());
    for (unsigned char c : word) {
        if (std::isalnum(c) || c == '-' || c == '_')
            out += static_cast<char>(std::tolower(c));
        // strip other punctuation
    }
    return out;
}

} // namespace cog0
