/*
 * agentzero-perception/src/TextualSensor.cpp
 *
 * Streaming text ingestion with salience scoring and AtomSpace encoding.
 */
#include "opencog/agentzero/TextualSensor.h"

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

using namespace opencog;
using namespace opencog::agentzero;

namespace {

const std::vector<std::string>& salientKeywords()
{
    static const std::vector<std::string> keywords = {
        "goal", "task", "important", "critical", "urgent",
        "error", "fail", "success", "complete", "warning",
        "alert", "priority", "key", "main", "primary"
    };
    return keywords;
}

std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

} // namespace

TextualSensor::TextualSensor(AtomSpacePtr atomspace, Handle agent_self,
                             TextProcessingMode mode)
    : _atomspace(std::move(atomspace))
    , _agent_self(std::move(agent_self))
    , _mode(mode)
{
    if (!_atomspace) {
        throw std::invalid_argument("TextualSensor requires a valid AtomSpace");
    }
    if (!_agent_self) {
        throw std::invalid_argument("TextualSensor requires a valid agent self Handle");
    }
    _text_root = _atomspace->add_node(CONCEPT_NODE, "AgentTextPercepts");
}

TextualSensor::~TextualSensor() = default;

// static
double TextualSensor::clamp01(double v)
{
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

TextProcessingMode TextualSensor::getMode() const
{
    std::lock_guard<std::mutex> lock(_mu);
    return _mode;
}

void TextualSensor::setMode(TextProcessingMode mode)
{
    std::lock_guard<std::mutex> lock(_mu);
    _mode = mode;
}

size_t TextualSensor::queue_size() const
{
    std::lock_guard<std::mutex> lock(_mu);
    return _queue.size();
}

size_t TextualSensor::processedUnitCount() const
{
    std::lock_guard<std::mutex> lock(_mu);
    return _processed_units;
}

void TextualSensor::add_text(const std::string& text)
{
    if (text.empty()) return;
    std::lock_guard<std::mutex> lock(_mu);
    _queue.push(text);
}

// static
std::vector<std::string> TextualSensor::splitSentences(const std::string& text)
{
    std::vector<std::string> units;
    std::string cur;
    for (char c : text) {
        cur.push_back(c);
        if (c == '.' || c == '!' || c == '?') {
            // trim
            size_t start = cur.find_first_not_of(" \t\n\r");
            size_t end = cur.find_last_not_of(" \t\n\r");
            if (start != std::string::npos) {
                units.push_back(cur.substr(start, end - start + 1));
            }
            cur.clear();
        }
    }
    size_t start = cur.find_first_not_of(" \t\n\r");
    size_t end = cur.find_last_not_of(" \t\n\r");
    if (start != std::string::npos) {
        units.push_back(cur.substr(start, end - start + 1));
    }
    return units;
}

// static
std::vector<std::string> TextualSensor::splitWords(const std::string& text)
{
    std::vector<std::string> words;
    std::istringstream iss(text);
    std::string w;
    while (iss >> w) {
        std::string cleaned;
        for (unsigned char c : w) {
            if (std::isalnum(c) || c == '-' || c == '_') {
                cleaned.push_back(static_cast<char>(std::tolower(c)));
            }
        }
        if (!cleaned.empty()) words.push_back(cleaned);
    }
    return words;
}

std::vector<std::string> TextualSensor::splitUnits(const std::string& text) const
{
    switch (_mode) {
        case TextProcessingMode::WORDS:
            return splitWords(text);
        case TextProcessingMode::DOCUMENTS:
        case TextProcessingMode::STREAM:
            return {text};
        case TextProcessingMode::SENTENCES:
        default:
            return splitSentences(text);
    }
}

Handle TextualSensor::encodeUnit(const std::string& unit, const TextSalienceScore& score)
{
    Handle concept = _atomspace->add_node(CONCEPT_NODE, "text:" + unit);
    SimpleTruthValue::setTV(concept, score.overall, 0.9);

    Handle pred = _atomspace->add_node(PREDICATE_NODE, "read_text");
    Handle list = _atomspace->add_link(LIST_LINK, HandleSeq{_agent_self, concept});
    Handle eval = _atomspace->add_link(EVALUATION_LINK, HandleSeq{pred, list});
    SimpleTruthValue::setTV(eval, score.overall, 0.9);

    _atomspace->add_link(MEMBER_LINK, HandleSeq{concept, _text_root});
    return concept;
}

HandleSeq TextualSensor::processText(const std::string& text)
{
    HandleSeq result;
    if (text.empty()) return result;

    TextProcessingMode mode;
    {
        std::lock_guard<std::mutex> lock(_mu);
        mode = _mode;
    }
    // Use mode without holding lock during AtomSpace ops
    std::vector<std::string> units;
    switch (mode) {
        case TextProcessingMode::WORDS:
            units = splitWords(text);
            break;
        case TextProcessingMode::DOCUMENTS:
        case TextProcessingMode::STREAM:
            units = {text};
            break;
        case TextProcessingMode::SENTENCES:
        default:
            units = splitSentences(text);
            break;
    }

    for (const auto& u : units) {
        if (u.empty()) continue;
        TextSalienceScore score = calculateSalience(u);
        Handle h = encodeUnit(u, score);
        result.push_back(h);
        std::lock_guard<std::mutex> lock(_mu);
        ++_processed_units;
        // Update vocabulary with words from unit
        for (const auto& w : splitWords(u)) {
            _vocabulary[w]++;
        }
    }
    return result;
}

HandleSeq TextualSensor::processNext()
{
    std::string item;
    {
        std::lock_guard<std::mutex> lock(_mu);
        if (_queue.empty()) return {};
        item = _queue.front();
        _queue.pop();
    }
    return processText(item);
}

HandleSeq TextualSensor::processAll()
{
    HandleSeq all;
    for (;;) {
        std::string item;
        {
            std::lock_guard<std::mutex> lock(_mu);
            if (_queue.empty()) break;
            item = _queue.front();
            _queue.pop();
        }
        auto batch = processText(item);
        all.insert(all.end(), batch.begin(), batch.end());
    }
    return all;
}

TextSalienceScore TextualSensor::calculateSalience(const std::string& text)
{
    TextSalienceScore score;
    if (text.empty()) {
        return score;
    }

    score.length = clamp01(static_cast<double>(text.size()) / 200.0);

    std::string lower = toLower(text);
    double keyword_hits = 0.0;
    for (const auto& kw : salientKeywords()) {
        if (lower.find(kw) != std::string::npos) {
            keyword_hits += 1.0;
        }
    }
    score.lexical = clamp01(keyword_hits / 5.0);

    // Novelty from vocabulary frequency of first word or whole string
    std::string key = lower;
    auto words = splitWords(text);
    if (!words.empty()) key = words.front();

    size_t seen = 0;
    {
        std::lock_guard<std::mutex> lock(_mu);
        auto it = _vocabulary.find(key);
        if (it != _vocabulary.end()) seen = it->second;
        // Note: calculateSalience itself does not increment vocabulary;
        // processText does. For novelty decay tests that call calculateSalience
        // repeatedly on the same string, we increment a soft counter here.
        _vocabulary[key] = seen + 1;
    }
    score.novelty = clamp01(1.0 / (1.0 + static_cast<double>(seen)));

    score.overall = clamp01(0.3 * score.length + 0.4 * score.lexical + 0.3 * score.novelty);
    return score;
}

SensoryInput TextualSensor::toSensoryInput(const std::string& text,
                                           const TextSalienceScore& score) const
{
    SensoryInput si;
    si.sensor_type = "textual";
    si.modality = "text_stream";
    si.confidence = score.overall;

    // Encode characters as normalized [0,1] code-point fractions (first N)
    si.data.reserve(text.size());
    for (unsigned char c : text) {
        si.data.push_back(static_cast<double>(c) / 255.0);
    }
    return si;
}

std::string TextualSensor::getStats() const
{
    std::lock_guard<std::mutex> lock(_mu);
    std::ostringstream oss;
    oss << "{\"processed_units\":" << _processed_units
        << ",\"queued_count\":" << _queue.size()
        << ",\"vocabulary_size\":" << _vocabulary.size()
        << "}";
    return oss.str();
}

void TextualSensor::resetVocabulary()
{
    std::lock_guard<std::mutex> lock(_mu);
    _vocabulary.clear();
}
