/*
 * src/PerceptualProcessor.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * PerceptualProcessor Implementation
 * Processes raw sensory data into AtomSpace representations
 * Part of the AGENT-ZERO-GENESIS project - Task AZ-PERC-002
 */

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/PerceptualProcessor.h"

using namespace opencog;
using namespace opencog::agentzero;

PerceptualProcessor::PerceptualProcessor(AtomSpacePtr atomspace, Handle agent_self)
    : _atomspace(atomspace)
    , _agent_self(agent_self)
    , _perception_context(Handle::UNDEFINED)
    , _processed_count(0)
    , _error_count(0)
    , _last_processing_time(std::chrono::steady_clock::now())
{
    if (!_atomspace) {
        throw std::invalid_argument("AtomSpace cannot be null");
    }
    if (_agent_self == Handle::UNDEFINED) {
        throw std::invalid_argument("Agent self atom cannot be undefined");
    }
    
    logger().info() << "[PerceptualProcessor] Constructor: Initializing with AtomSpace";
}

PerceptualProcessor::~PerceptualProcessor()
{
    logger().info() << "[PerceptualProcessor] Destructor: Processed " 
                   << _processed_count.load() << " inputs with " 
                   << _error_count.load() << " errors";
}

Handle PerceptualProcessor::processInput(const SensoryInput& input)
{
    _last_processing_time = std::chrono::steady_clock::now();
    
    logger().debug() << "[PerceptualProcessor] Processing " << input.sensor_type 
                    << " input with " << input.data.size() << " data points";
    
    try {
        // Validate input
        if (!validateSensoryInput(input)) {
            logProcessingError("Input validation failed", input);
            _error_count++;
            return Handle::UNDEFINED;
        }
        
        // Process the sensory data
        Handle perception_atom = processSensoryData(input);
        
        if (perception_atom != Handle::UNDEFINED) {
            _processed_count++;
            logger().debug() << "[PerceptualProcessor] Successfully created perception atom";
        } else {
            _error_count++;
            logProcessingError("Failed to create perception atom", input);
        }
        
        return perception_atom;
        
    } catch (const std::exception& e) {
        _error_count++;
        logProcessingError("Exception during processing: " + std::string(e.what()), input);
        return Handle::UNDEFINED;
    }
}

std::vector<Handle> PerceptualProcessor::processBatch(const std::vector<SensoryInput>& inputs)
{
    std::vector<Handle> results;
    results.reserve(inputs.size());
    
    logger().debug() << "[PerceptualProcessor] Processing batch of " << inputs.size() << " inputs";
    
    for (const auto& input : inputs) {
        Handle result = processInput(input);
        results.push_back(result);
    }
    
    return results;
}

Handle PerceptualProcessor::processSensoryData(const SensoryInput& input)
{
    // Route to specific processing based on sensor type
    if (input.sensor_type == "visual" || input.sensor_type == "camera") {
        return processVisualInput(input);
    } else if (input.sensor_type == "auditory" || input.sensor_type == "microphone") {
        return processAuditoryInput(input);
    } else if (input.sensor_type == "tactile" || input.sensor_type == "touch") {
        return processTactileInput(input);
    } else {
        return processGenericInput(input);
    }
}

Handle PerceptualProcessor::createPerceptionAtom(const std::string& perception_type,
                                                const std::vector<Handle>& components,
                                                double confidence)
{
    // Create the perception concept
    std::string perception_name = "Perception_" + perception_type + "_" + std::to_string(_processed_count.load());
    Handle perception_concept = _atomspace->add_node(CONCEPT_NODE, std::move(perception_name));
    
    // Set truth value based on confidence
    TruthValuePtr tv = SimpleTruthValue::createTV(confidence, 0.9);
    perception_concept->setTruthValue(tv);
    
    // Create evaluation link connecting agent to this perception
    HandleSeq eval_seq;
    eval_seq.push_back(_atomspace->add_node(PREDICATE_NODE, "experiences"));
    
    HandleSeq list_seq;
    list_seq.push_back(_agent_self);
    list_seq.push_back(perception_concept);
    Handle list_link = _atomspace->add_link(LIST_LINK, std::move(list_seq));
    
    eval_seq.push_back(list_link);
    Handle evaluation = _atomspace->add_link(EVALUATION_LINK, std::move(eval_seq));
    evaluation->setTruthValue(tv);
    
    // If we have a perception context, link to it
    if (_perception_context != Handle::UNDEFINED) {
        HandleSeq context_seq;
        context_seq.push_back(_atomspace->add_node(PREDICATE_NODE, "in_context"));
        
        HandleSeq context_list;
        context_list.push_back(perception_concept);
        context_list.push_back(_perception_context);
        Handle context_list_link = _atomspace->add_link(LIST_LINK, std::move(context_list));
        
        context_seq.push_back(context_list_link);
        Handle context_eval = _atomspace->add_link(EVALUATION_LINK, std::move(context_seq));
        context_eval->setTruthValue(tv);
    }
    
    // Link components if provided
    for (const auto& component : components) {
        if (component != Handle::UNDEFINED) {
            HandleSeq inherit_seq;
            inherit_seq.push_back(component);
            inherit_seq.push_back(perception_concept);
            Handle inherit_link = _atomspace->add_link(INHERITANCE_LINK, std::move(inherit_seq));
            inherit_link->setTruthValue(SimpleTruthValue::createTV(0.7, 0.8));
        }
    }
    
    return perception_concept;
}

Handle PerceptualProcessor::createSensoryAtom(const SensoryInput& input)
{
    // Create sensory data representation
    std::string data_name = input.sensor_type + "_data_" + input.modality;
    Handle sensory_atom = _atomspace->add_node(CONCEPT_NODE, std::move(data_name));
    
    // Set truth value based on input confidence
    TruthValuePtr tv = SimpleTruthValue::createTV(input.confidence, 0.8);
    sensory_atom->setTruthValue(tv);
    
    // Create numeric values for the raw data (simplified representation)
    if (!input.data.empty()) {
        // Create a summary representation (mean, variance, etc.)
        double mean = std::accumulate(input.data.begin(), input.data.end(), 0.0) / input.data.size();
        double variance = 0.0;
        for (const auto& val : input.data) {
            variance += (val - mean) * (val - mean);
        }
        variance /= input.data.size();
        
        // Create atoms for statistical properties
        Handle mean_atom = _atomspace->add_node(NUMBER_NODE, std::to_string(mean));
        Handle var_atom = _atomspace->add_node(NUMBER_NODE, std::to_string(variance));
        
        // Link statistical properties to sensory atom
        HandleSeq mean_seq;
        mean_seq.push_back(_atomspace->add_node(PREDICATE_NODE, "has_mean"));
        HandleSeq mean_list;
        mean_list.push_back(sensory_atom);
        mean_list.push_back(mean_atom);
        mean_seq.push_back(_atomspace->add_link(LIST_LINK, std::move(mean_list)));
        _atomspace->add_link(EVALUATION_LINK, std::move(mean_seq));
        
        HandleSeq var_seq;
        var_seq.push_back(_atomspace->add_node(PREDICATE_NODE, "has_variance"));
        HandleSeq var_list;
        var_list.push_back(sensory_atom);
        var_list.push_back(var_atom);
        var_seq.push_back(_atomspace->add_link(LIST_LINK, std::move(var_list)));
        _atomspace->add_link(EVALUATION_LINK, std::move(var_seq));
    }
    
    return sensory_atom;
}

Handle PerceptualProcessor::processVisualInput(const SensoryInput& input)
{
    logger().debug() << "[PerceptualProcessor] Processing visual input";
    
    // Create sensory data atom
    Handle sensory_atom = createSensoryAtom(input);
    
    // Create visual-specific concepts
    Handle visual_concept = _atomspace->add_node(CONCEPT_NODE, "VisualPerception");
    
    std::vector<Handle> components;
    components.push_back(sensory_atom);
    components.push_back(visual_concept);
    
    return createPerceptionAtom("Visual", components, input.confidence);
}

Handle PerceptualProcessor::processAuditoryInput(const SensoryInput& input)
{
    logger().debug() << "[PerceptualProcessor] Processing auditory input";
    
    // Create sensory data atom
    Handle sensory_atom = createSensoryAtom(input);
    
    // Create auditory-specific concepts
    Handle auditory_concept = _atomspace->add_node(CONCEPT_NODE, "AuditoryPerception");
    
    std::vector<Handle> components;
    components.push_back(sensory_atom);
    components.push_back(auditory_concept);
    
    return createPerceptionAtom("Auditory", components, input.confidence);
}

Handle PerceptualProcessor::processTactileInput(const SensoryInput& input)
{
    logger().debug() << "[PerceptualProcessor] Processing tactile input";
    
    // Create sensory data atom
    Handle sensory_atom = createSensoryAtom(input);
    
    // Create tactile-specific concepts
    Handle tactile_concept = _atomspace->add_node(CONCEPT_NODE, "TactilePerception");
    
    std::vector<Handle> components;
    components.push_back(sensory_atom);
    components.push_back(tactile_concept);
    
    return createPerceptionAtom("Tactile", components, input.confidence);
}

Handle PerceptualProcessor::processGenericInput(const SensoryInput& input)
{
    logger().debug() << "[PerceptualProcessor] Processing generic input: " << input.sensor_type;
    
    // Create sensory data atom
    Handle sensory_atom = createSensoryAtom(input);
    
    // Create generic perception concept
    std::string concept_name = input.sensor_type + "Perception";
    Handle generic_concept = _atomspace->add_node(CONCEPT_NODE, std::move(concept_name));
    
    std::vector<Handle> components;
    components.push_back(sensory_atom);
    components.push_back(generic_concept);
    
    return createPerceptionAtom("Generic", components, input.confidence);
}

bool PerceptualProcessor::validateSensoryInput(const SensoryInput& input) const
{
    // Check for basic validity
    if (input.sensor_type.empty()) {
        return false;
    }
    
    if (input.confidence < 0.0 || input.confidence > 1.0) {
        return false;
    }
    
    // Data can be empty (some sensors might not provide numeric data)
    // but if present, should not contain invalid values
    for (const auto& value : input.data) {
        if (std::isnan(value) || std::isinf(value)) {
            return false;
        }
    }
    
    return true;
}

void PerceptualProcessor::logProcessingError(const std::string& error_msg, 
                                           const SensoryInput& input)
{
    logger().error() << "[PerceptualProcessor] " << error_msg 
                    << " - Sensor: " << input.sensor_type 
                    << ", Modality: " << input.modality
                    << ", Data size: " << input.data.size()
                    << ", Confidence: " << input.confidence;
}

void PerceptualProcessor::setPerceptionContext(Handle context)
{
    _perception_context = context;
    logger().debug() << "[PerceptualProcessor] Perception context updated";
}

std::string PerceptualProcessor::getProcessingStats() const
{
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - _last_processing_time);
    
    std::ostringstream stats;
    stats << "{";
    stats << "\"processed_count\":" << _processed_count.load() << ",";
    stats << "\"error_count\":" << _error_count.load() << ",";
    stats << "\"error_rate\":" << std::fixed << std::setprecision(4) 
          << ((_processed_count.load() > 0) ? 
              (double(_error_count.load()) / double(_processed_count.load() + _error_count.load())) : 0.0) << ",";
    stats << "\"last_processing_ms_ago\":" << duration.count() << ",";
    stats << "\"is_healthy\":" << (isHealthy() ? "true" : "false");
    stats << "}";
    
    return stats.str();
}

void PerceptualProcessor::resetStats()
{
    _processed_count = 0;
    _error_count = 0;
    _last_processing_time = std::chrono::steady_clock::now();
    
    logger().info() << "[PerceptualProcessor] Statistics reset";
}

bool PerceptualProcessor::isHealthy() const
{
    size_t total = _processed_count.load() + _error_count.load();
    if (total == 0) return true; // No processing yet, consider healthy
    
    double error_rate = double(_error_count.load()) / double(total);
    return error_rate < 0.1; // Less than 10% error rate
}