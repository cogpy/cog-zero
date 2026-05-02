/*
 * include/opencog/agentzero/PerceptualProcessor.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * PerceptualProcessor - Processes raw sensory data into AtomSpace representations
 * Part of the AGENT-ZERO-GENESIS project - Task AZ-PERC-002
 */

#ifndef _OPENCOG_AGENTZERO_PERCEPTUAL_PROCESSOR_H
#define _OPENCOG_AGENTZERO_PERCEPTUAL_PROCESSOR_H

#include <memory>
#include <vector>
#include <string>
#include <chrono>

#include <opencog/atoms/base/Handle.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

namespace opencog
{
namespace agentzero
{

/**
 * @brief Sensory data input structure for processing
 */
struct SensoryInput
{
    std::string sensor_type;  ///< Type of sensor (visual, auditory, tactile, etc.)
    std::string modality;     ///< Specific modality within type
    std::vector<double> data; ///< Raw sensory data values
    std::chrono::system_clock::time_point timestamp; ///< When data was captured
    double confidence;        ///< Confidence level in the data (0.0-1.0)
    
    SensoryInput(const std::string& type, const std::string& mod, 
                const std::vector<double>& raw_data, double conf = 1.0)
        : sensor_type(type), modality(mod), data(raw_data), 
          timestamp(std::chrono::system_clock::now()), confidence(conf) {}
};

/**
 * @brief PerceptualProcessor - Converts raw sensory data into AtomSpace representations
 *
 * This class implements the core perception processing functionality for Agent-Zero,
 * taking multi-modal sensory inputs and creating structured knowledge representations
 * in the AtomSpace. It integrates with OpenCog's perception, sensory, vision, and
 * attention components.
 */
class PerceptualProcessor
{
private:
    AtomSpacePtr _atomspace;
    Handle _agent_self;
    Handle _perception_context;
    
    // Processing statistics
    std::atomic<size_t> _processed_count;
    std::atomic<size_t> _error_count;
    std::chrono::steady_clock::time_point _last_processing_time;

    // Internal processing methods
    Handle processSensoryData(const SensoryInput& input);
    Handle createPerceptionAtom(const std::string& perception_type, 
                               const std::vector<Handle>& components,
                               double confidence = 0.8);
    Handle createSensoryAtom(const SensoryInput& input);
    Handle processVisualInput(const SensoryInput& input);
    Handle processAuditoryInput(const SensoryInput& input);
    Handle processTactileInput(const SensoryInput& input);
    Handle processGenericInput(const SensoryInput& input);
    
    // Validation and error handling
    bool validateSensoryInput(const SensoryInput& input) const;
    void logProcessingError(const std::string& error_msg, const SensoryInput& input);

public:
    /**
     * @brief Constructor
     * @param atomspace Shared pointer to the AtomSpace for knowledge representation
     * @param agent_self Handle to the agent's self-representation atom
     */
    PerceptualProcessor(AtomSpacePtr atomspace, Handle agent_self);
    
    /**
     * @brief Destructor
     */
    ~PerceptualProcessor();

    /**
     * @brief Process a single sensory input into AtomSpace representation
     * @param input The sensory input data to process
     * @return Handle to the created perception atom, or Handle::UNDEFINED on error
     */
    Handle processInput(const SensoryInput& input);
    
    /**
     * @brief Process multiple sensory inputs as a batch
     * @param inputs Vector of sensory inputs to process
     * @return Vector of handles to created perception atoms
     */
    std::vector<Handle> processBatch(const std::vector<SensoryInput>& inputs);
    
    /**
     * @brief Set the current perception context
     * @param context Handle to the perception context atom
     */
    void setPerceptionContext(Handle context);
    
    /**
     * @brief Get current processing statistics
     * @return JSON string with processing statistics
     */
    std::string getProcessingStats() const;
    
    /**
     * @brief Reset processing statistics
     */
    void resetStats();
    
    /**
     * @brief Check if processor is healthy (low error rate)
     * @return true if error rate is below threshold
     */
    bool isHealthy() const;
    
    /**
     * @brief Get the AtomSpace used by this processor
     * @return Shared pointer to the AtomSpace
     */
    AtomSpacePtr getAtomSpace() const { return _atomspace; }
    
    /**
     * @brief Get the agent self atom
     * @return Handle to the agent self atom
     */
    Handle getAgentSelf() const { return _agent_self; }
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_PERCEPTUAL_PROCESSOR_H