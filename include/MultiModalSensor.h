/*
 * include/opencog/agentzero/MultiModalSensor.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * MultiModalSensor - Unified interface for various sensory inputs
 * Part of the AGENT-ZERO-GENESIS project - Task AZ-PERC-001/002
 */

#ifndef _OPENCOG_AGENTZERO_MULTIMODAL_SENSOR_H
#define _OPENCOG_AGENTZERO_MULTIMODAL_SENSOR_H

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "PerceptualProcessor.h"

namespace opencog
{
namespace agentzero
{

/**
 * @brief Sensor capability flags
 */
enum class SensorCapability : uint32_t
{
    VISUAL = 1 << 0,       ///< Visual perception capability
    AUDITORY = 1 << 1,     ///< Auditory perception capability  
    TACTILE = 1 << 2,      ///< Tactile perception capability
    PROPRIOCEPTIVE = 1 << 3, ///< Body position sensing
    CHEMICAL = 1 << 4,     ///< Chemical sensing (smell/taste)
    THERMAL = 1 << 5,      ///< Temperature sensing
    MAGNETIC = 1 << 6,     ///< Magnetic field sensing
    TEMPORAL = 1 << 7      ///< Time-based sensing
};

/**
 * @brief Sensor information structure
 */
struct SensorInfo
{
    std::string name;           ///< Sensor name/identifier
    std::string description;    ///< Human-readable description
    SensorCapability capabilities; ///< Sensor capabilities bitmask
    double sampling_rate;       ///< Sampling rate in Hz
    bool is_active;            ///< Whether sensor is currently active
    
    SensorInfo(const std::string& n, const std::string& desc, 
               SensorCapability caps, double rate = 10.0)
        : name(n), description(desc), capabilities(caps), 
          sampling_rate(rate), is_active(false) {}
};

/**
 * @brief Callback function type for sensor data processing
 */
using SensorDataCallback = std::function<void(const SensoryInput&)>;

/**
 * @brief Abstract base class for multi-modal sensor interface
 *
 * This interface provides a unified way to interact with various types
 * of sensors, from simple data sources to complex perception systems.
 * Implementations should handle specific sensor hardware/software integration.
 */
class MultiModalSensor
{
protected:
    SensorInfo _sensor_info;
    std::vector<SensorDataCallback> _callbacks;
    bool _is_initialized;

public:
    /**
     * @brief Constructor
     * @param info Sensor information structure
     */
    explicit MultiModalSensor(const SensorInfo& info);
    
    /**
     * @brief Virtual destructor
     */
    virtual ~MultiModalSensor() = default;

    /**
     * @brief Initialize the sensor
     * @return true if initialization successful
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief Start sensor data collection
     * @return true if started successfully
     */
    virtual bool start() = 0;
    
    /**
     * @brief Stop sensor data collection
     * @return true if stopped successfully
     */
    virtual bool stop() = 0;
    
    /**
     * @brief Check if sensor is currently active
     * @return true if sensor is collecting data
     */
    virtual bool isActive() const { return _sensor_info.is_active; }
    
    /**
     * @brief Get sensor information
     * @return Reference to sensor info structure
     */
    const SensorInfo& getSensorInfo() const { return _sensor_info; }
    
    /**
     * @brief Register callback for sensor data
     * @param callback Function to call when new sensor data is available
     */
    void registerCallback(const SensorDataCallback& callback);
    
    /**
     * @brief Remove all registered callbacks
     */
    void clearCallbacks();
    
    /**
     * @brief Get current sensor status as string
     * @return JSON string with sensor status information
     */
    virtual std::string getStatusInfo() const;
    
    /**
     * @brief Check if sensor has specific capability
     * @param capability Capability to check for
     * @return true if sensor has the capability
     */
    bool hasCapability(SensorCapability capability) const;

protected:
    /**
     * @brief Notify all registered callbacks with new data
     * @param input Sensory input data to send to callbacks
     */
    void notifyCallbacks(const SensoryInput& input);
    
    /**
     * @brief Check if sensor is properly initialized
     * @return true if initialized
     */
    bool isInitialized() const { return _is_initialized; }
    
    /**
     * @brief Set initialization status
     * @param initialized New initialization status
     */
    void setInitialized(bool initialized) { _is_initialized = initialized; }
};

/**
 * @brief Mock sensor implementation for testing
 */
class MockSensor : public MultiModalSensor
{
private:
    std::vector<std::vector<double>> _test_data;
    size_t _current_sample;
    
public:
    explicit MockSensor(const SensorInfo& info);
    ~MockSensor() override = default;
    
    bool initialize() override;
    bool start() override;
    bool stop() override;
    
    /**
     * @brief Add test data for the mock sensor
     * @param data Vector of test data samples
     */
    void addTestData(const std::vector<double>& data);
    
    /**
     * @brief Generate next sample (for testing)
     * @return true if sample generated successfully
     */
    bool generateNextSample();
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_MULTIMODAL_SENSOR_H