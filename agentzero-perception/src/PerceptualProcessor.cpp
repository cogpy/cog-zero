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

    // Disambiguate empty samples vs content-bearing ones via data fingerprint
    if (!input.data.empty()) {
        name << "#";
        // Compact fingerprint: size + first/last values
        name << input.data.size();
        name << ":" << input.data.front();
        if (input.data.size() > 1) {
            name << ":" << input.data.back();
        }
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

    // Encode raw numeric channels as NUMBER_NODE members when present
    for (size_t i = 0; i < input.data.size(); ++i) {
        std::ostringstream ch;
        ch << "channel:" << i << "=" << input.data[i];
        Handle num = _atomspace->add_node(NUMBER_NODE, ch.str());
        _atomspace->add_link(MEMBER_LINK, HandleSeq{num, percept});
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
