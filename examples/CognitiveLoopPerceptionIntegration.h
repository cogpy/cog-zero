/*
 * CognitiveLoopPerceptionIntegration.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Integration example showing how to update CognitiveLoop to use PerceptualProcessor
 * Part of the AGENT-ZERO-GENESIS project - Task AZ-PERC-002
 */

#ifndef _OPENCOG_AGENTZERO_COGNITIVE_LOOP_PERCEPTION_INTEGRATION_H
#define _OPENCOG_AGENTZERO_COGNITIVE_LOOP_PERCEPTION_INTEGRATION_H

#include <memory>
#include "opencog/agentzero/PerceptualProcessor.h"
#include "opencog/agentzero/MultiModalSensor.h"

namespace opencog
{
namespace agentzero
{

/**
 * @brief Enhanced CognitiveLoop with PerceptualProcessor integration
 *
 * This shows how the original CognitiveLoop can be updated to use the new
 * PerceptualProcessor instead of the placeholder perception processing.
 */
class EnhancedCognitiveLoop
{
private:
    AtomSpacePtr _atomspace;
    Handle _agent_self;
    Handle _perception_context;
    
    // Perception components
    std::unique_ptr<PerceptualProcessor> _perceptual_processor;
    std::vector<std::unique_ptr<MultiModalSensor>> _sensors;
    
    // Processing state
    std::atomic<bool> _running;
    std::atomic<size_t> _cycle_count;

public:
    /**
     * @brief Constructor
     */
    EnhancedCognitiveLoop(AtomSpacePtr atomspace, Handle agent_self)
        : _atomspace(atomspace)
        , _agent_self(agent_self)
        , _running(false)
        , _cycle_count(0)
    {
        // Create PerceptualProcessor
        _perceptual_processor = std::make_unique<PerceptualProcessor>(_atomspace, _agent_self);
        
        // Create perception context
        _perception_context = _atomspace->add_node(CONCEPT_NODE, std::string("CognitiveLoop_Perception"));
        _perceptual_processor->setPerceptionContext(_perception_context);
    }
    
    /**
     * @brief Add a sensor to the cognitive loop
     */
    void addSensor(std::unique_ptr<MultiModalSensor> sensor)
    {
        // Register callback to process sensor data through PerceptualProcessor
        sensor->registerCallback([this](const SensoryInput& input) {
            Handle perception = _perceptual_processor->processInput(input);
            if (perception != Handle::UNDEFINED) {
                logger().debug() << "[CognitiveLoop] New perception: " << perception->to_string();
            }
        });
        
        _sensors.push_back(std::move(sensor));
    }
    
    /**
     * @brief Enhanced perception phase using PerceptualProcessor
     * 
     * This replaces the placeholder implementation in the original CognitiveLoop
     */
    bool executePerceptionPhase()
    {
        logger().debug() << "[CognitiveLoop] Executing enhanced perception phase";
        
        try {
            // Generate sensor data (in real implementation, sensors would provide actual data)
            for (auto& sensor : _sensors) {
                if (sensor->isActive()) {
                    MockSensor* mock_sensor = dynamic_cast<MockSensor*>(sensor.get());
                    if (mock_sensor) {
                        mock_sensor->generateNextSample(); // This triggers the callback
                    }
                }
            }
            
            // Update perception context with timestamp and processing stats
            std::string stats = _perceptual_processor->getProcessingStats();
            logger().debug() << "[CognitiveLoop] Perception stats: " << stats;
            
            return _perceptual_processor->isHealthy();
            
        } catch (const std::exception& e) {
            logger().error() << "[CognitiveLoop] Enhanced perception phase error: " << e.what();
            return false;
        }
    }
    
    /**
     * @brief Initialize sensors and start perception processing
     */
    bool startPerception()
    {
        logger().info() << "[CognitiveLoop] Starting perception with " << _sensors.size() << " sensors";
        
        for (auto& sensor : _sensors) {
            if (!sensor->initialize()) {
                logger().error() << "[CognitiveLoop] Failed to initialize sensor: " 
                                 << sensor->getSensorInfo().name;
                return false;
            }
            
            if (!sensor->start()) {
                logger().error() << "[CognitiveLoop] Failed to start sensor: "
                                 << sensor->getSensorInfo().name;
                return false;
            }
        }
        
        logger().info() << "[CognitiveLoop] All sensors started successfully";
        return true;
    }
    
    /**
     * @brief Stop perception processing
     */
    void stopPerception()
    {
        logger().info() << "[CognitiveLoop] Stopping perception";
        
        for (auto& sensor : _sensors) {
            sensor->stop();
        }
        
        logger().info() << "[CognitiveLoop] Perception stopped";
    }
    
    /**
     * @brief Get perception processing statistics
     */
    std::string getPerceptionStats() const
    {
        return _perceptual_processor->getProcessingStats();
    }
    
    /**
     * @brief Get number of active sensors
     */
    size_t getActiveSensorCount() const
    {
        size_t count = 0;
        for (const auto& sensor : _sensors) {
            if (sensor->isActive()) count++;
        }
        return count;
    }
    
    /**
     * @brief Execute a single cognitive cycle with enhanced perception
     */
    bool executeSingleCycle()
    {
        logger().debug() << "[CognitiveLoop] Executing cycle " << _cycle_count.load();
        
        // Execute enhanced perception phase
        bool perception_success = executePerceptionPhase();
        
        // Other phases would follow (planning, action, reflection)
        // For this demo, we just focus on perception
        
        _cycle_count++;
        return perception_success;
    }
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_COGNITIVE_LOOP_PERCEPTION_INTEGRATION_H