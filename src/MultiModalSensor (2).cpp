/*
 * src/MultiModalSensor.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * MultiModalSensor Implementation
 * Unified interface for various sensory inputs
 * Part of the AGENT-ZERO-GENESIS project - Task AZ-PERC-001/002
 */

#include <sstream>
#include <random>

#include <opencog/util/Logger.h>

#include "opencog/agentzero/MultiModalSensor.h"

using namespace opencog;
using namespace opencog::agentzero;

MultiModalSensor::MultiModalSensor(const SensorInfo& info)
    : _sensor_info(info)
    , _is_initialized(false)
{
    logger().info() << "[MultiModalSensor] Constructor: " << _sensor_info.name;
}

void MultiModalSensor::registerCallback(const SensorDataCallback& callback)
{
    _callbacks.push_back(callback);
    logger().debug() << "[MultiModalSensor] Callback registered for " << _sensor_info.name;
}

void MultiModalSensor::clearCallbacks()
{
    _callbacks.clear();
    logger().debug() << "[MultiModalSensor] Callbacks cleared for " << _sensor_info.name;
}

std::string MultiModalSensor::getStatusInfo() const
{
    std::ostringstream status;
    status << "{";
    status << "\"name\":\"" << _sensor_info.name << "\",";
    status << "\"description\":\"" << _sensor_info.description << "\",";
    status << "\"is_active\":" << (_sensor_info.is_active ? "true" : "false") << ",";
    status << "\"is_initialized\":" << (_is_initialized ? "true" : "false") << ",";
    status << "\"sampling_rate\":" << _sensor_info.sampling_rate << ",";
    status << "\"callback_count\":" << _callbacks.size();
    status << "}";
    return status.str();
}

bool MultiModalSensor::hasCapability(SensorCapability capability) const
{
    return (static_cast<uint32_t>(_sensor_info.capabilities) & 
            static_cast<uint32_t>(capability)) != 0;
}

void MultiModalSensor::notifyCallbacks(const SensoryInput& input)
{
    for (const auto& callback : _callbacks) {
        try {
            callback(input);
        } catch (const std::exception& e) {
            logger().error() << "[MultiModalSensor] Callback error for " 
                           << _sensor_info.name << ": " << e.what();
        }
    }
}

// ===================================================================
// MockSensor Implementation
// ===================================================================

MockSensor::MockSensor(const SensorInfo& info)
    : MultiModalSensor(info)
    , _current_sample(0)
{
    logger().info() << "[MockSensor] Constructor: " << info.name;
}

bool MockSensor::initialize()
{
    logger().info() << "[MockSensor] Initializing " << _sensor_info.name;
    
    // Mock initialization - always succeeds
    setInitialized(true);
    
    // Add some default test data if none provided
    if (_test_data.empty()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<> dis(0.0, 1.0);
        
        // Generate 100 samples of random data
        for (int i = 0; i < 100; ++i) {
            std::vector<double> sample;
            for (int j = 0; j < 10; ++j) { // 10 data points per sample
                sample.push_back(dis(gen));
            }
            _test_data.push_back(sample);
        }
    }
    
    logger().info() << "[MockSensor] Initialized with " << _test_data.size() << " test samples";
    return true;
}

bool MockSensor::start()
{
    if (!isInitialized()) {
        logger().error() << "[MockSensor] Cannot start - not initialized: " << _sensor_info.name;
        return false;
    }
    
    logger().info() << "[MockSensor] Starting " << _sensor_info.name;
    _sensor_info.is_active = true;
    _current_sample = 0;
    
    return true;
}

bool MockSensor::stop()
{
    logger().info() << "[MockSensor] Stopping " << _sensor_info.name;
    _sensor_info.is_active = false;
    return true;
}

void MockSensor::addTestData(const std::vector<double>& data)
{
    _test_data.push_back(data);
    logger().debug() << "[MockSensor] Added test data with " << data.size() 
                    << " values to " << _sensor_info.name;
}

bool MockSensor::generateNextSample()
{
    if (!isActive() || _test_data.empty()) {
        return false;
    }
    
    // Cycle through test data
    const auto& data = _test_data[_current_sample % _test_data.size()];
    _current_sample++;
    
    // Create sensory input with appropriate type based on capabilities
    std::string sensor_type = "generic";
    std::string modality = "default";
    
    if (hasCapability(SensorCapability::VISUAL)) {
        sensor_type = "visual";
        modality = "camera";
    } else if (hasCapability(SensorCapability::AUDITORY)) {
        sensor_type = "auditory";
        modality = "microphone";
    } else if (hasCapability(SensorCapability::TACTILE)) {
        sensor_type = "tactile";
        modality = "pressure";
    }
    
    // Create sensory input
    SensoryInput input(sensor_type, modality, data, 0.8);
    
    // Notify callbacks
    notifyCallbacks(input);
    
    logger().debug() << "[MockSensor] Generated sample " << _current_sample 
                    << " for " << _sensor_info.name;
    
    return true;
}