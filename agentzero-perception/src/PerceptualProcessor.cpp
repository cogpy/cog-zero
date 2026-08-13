/*
 * agentzero-perception/src/PerceptualProcessor.cpp
 *
 * Encodes SensoryInput samples into AtomSpace percept structures.
 */
#include "opencog/agentzero/PerceptualProcessor.h"

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include <cmath>
#include <cstdint>
#include <sstream>
#include <stdexcept>

using namespace opencog;
using namespace opencog::agentzero;

PerceptualProcessor::PerceptualProcessor(AtomSpacePtr atomspace, Handle agent_self)
    : _atomspace(std::move(atomspace))
    , _agent_self(std::move(agent_self))
{
    if (!_atomspace) {
        throw std::invalid_argument("PerceptualProcessor requires a valid AtomSpace");
    }
    if (!_agent_self) {
        throw std::invalid_argument("PerceptualProcessor requires a valid agent self Handle");
    }
    _percepts_root = _atomspace->add_node(CONCEPT_NODE, "AgentPercepts");
}

PerceptualProcessor::~PerceptualProcessor() = default;

bool PerceptualProcessor::validateInput(const SensoryInput& input) const
{
    if (input.confidence < 0.0 || input.confidence > 1.0) {
        return false;
    }
    for (double v : input.data) {
        if (!std::isfinite(v)) {
            return false;
        }
    }
    return true;
}

Handle PerceptualProcessor::encodeInput(const SensoryInput& input)
{
    // Primary concept: percept/<type>/<source>
    std::ostringstream name;
    name << "percept/" << (input.sensor_type.empty() ? "generic" : input.sensor_type)
         << "/" << (input.modality.empty() ? "unknown" : input.modality);

    // Full-content fingerprint so distinct vectors never collide.
    // FNV-1a 64-bit over little-endian doubles + size.
    if (!input.data.empty()) {
        uint64_t hash = 14695981039346656037ull;
        auto mix = [&](const void* p, size_t n) {
            const auto* bytes = static_cast<const unsigned char*>(p);
            for (size_t i = 0; i < n; ++i) {
                hash ^= bytes[i];
                hash *= 1099511628211ull;
            }
        };
        const uint64_t n = static_cast<uint64_t>(input.data.size());
        mix(&n, sizeof(n));
        for (double v : input.data) {
            mix(&v, sizeof(v));
        }
        name << "#h" << std::hex << hash << std::dec;
        name << "n" << input.data.size();
    } else {
        name << "#empty";
    }

    Handle percept = _atomspace->add_node(CONCEPT_NODE, name.str());
    SimpleTruthValue::setTV(percept, input.confidence, 0.9);

    // EvaluationLink: (perceived agent_self percept)
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "perceived");
    Handle list = _atomspace->add_link(LIST_LINK, HandleSeq{_agent_self, percept});
    Handle eval = _atomspace->add_link(EVALUATION_LINK, HandleSeq{pred, list});
    SimpleTruthValue::setTV(eval, input.confidence, 0.9);

    // Member of agent percepts root
    _atomspace->add_link(MEMBER_LINK, HandleSeq{percept, _percepts_root});

    // Optional context link
    if (_context) {
        _atomspace->add_link(CONTEXT_LINK, HandleSeq{_context, percept});
    }

    // Encode raw numeric channels: NUMBER_NODE holds the value; CONCEPT labels the index.
    for (size_t i = 0; i < input.data.size(); ++i) {
        std::ostringstream val;
        val << input.data[i];
        Handle num = _atomspace->add_node(NUMBER_NODE, val.str());
        Handle ch_label = _atomspace->add_node(CONCEPT_NODE, "channel:" + std::to_string(i));
        Handle pair = _atomspace->add_link(LIST_LINK, HandleSeq{ch_label, num});
        _atomspace->add_link(MEMBER_LINK, HandleSeq{pair, percept});
    }

    return percept;
}

Handle PerceptualProcessor::processInput(const SensoryInput& input)
{
    std::lock_guard<std::mutex> lock(_mu);
    ++_attempt_count;

    if (!validateInput(input)) {
        ++_error_count;
        logger().warn() << "[PerceptualProcessor] Rejected invalid sensory input";
        return Handle::UNDEFINED;
    }

    Handle h = encodeInput(input);
    ++_processed_count;
    return h;
}

std::vector<Handle> PerceptualProcessor::processBatch(const std::vector<SensoryInput>& inputs)
{
    std::vector<Handle> out;
    out.reserve(inputs.size());
    for (const auto& in : inputs) {
        out.push_back(processInput(in));
    }
    return out;
}

void PerceptualProcessor::setPerceptionContext(const Handle& context)
{
    std::lock_guard<std::mutex> lock(_mu);
    _context = context;
}

std::string PerceptualProcessor::getProcessingStats() const
{
    std::lock_guard<std::mutex> lock(_mu);
    std::ostringstream oss;
    oss << "{\"processed_count\":" << _processed_count
        << ",\"error_count\":" << _error_count
        << ",\"attempt_count\":" << _attempt_count
        << "}";
    return oss.str();
}

bool PerceptualProcessor::isHealthy() const
{
    std::lock_guard<std::mutex> lock(_mu);
    if (_attempt_count == 0) return true;
    double err_rate = static_cast<double>(_error_count) /
                      static_cast<double>(_attempt_count);
    // Unhealthy only when a strict majority of attempts failed.
    return err_rate <= 0.5;
}

void PerceptualProcessor::resetStats()
{
    std::lock_guard<std::mutex> lock(_mu);
    _processed_count = 0;
    _error_count = 0;
    _attempt_count = 0;
}
