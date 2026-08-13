/*
 * opencog/agentzero/MultiModalSensor.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Unified multi-modal sensor interface for text, numeric, event, and visual
 * inputs. Part of AGENT-ZERO-GENESIS Phase 2 (Perception & Sensory Processing).
 */
#ifndef _OPENCOG_AGENTZERO_MULTI_MODAL_SENSOR_H
#define _OPENCOG_AGENTZERO_MULTI_MODAL_SENSOR_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace opencog {
namespace agentzero {

/**
 * Bit-flag sensory capabilities a MultiModalSensor may expose.
 */
enum class SensorCapability : uint32_t {
    NONE      = 0,
    VISUAL    = 1u << 0,
    AUDITORY  = 1u << 1,
    TACTILE   = 1u << 2,
    OLFACTORY = 1u << 3,
    GUSTATORY = 1u << 4,
    PROPRIOCEPTIVE = 1u << 5,
    TEXTUAL   = 1u << 6,
    NUMERIC   = 1u << 7,
    EVENT     = 1u << 8,
    GENERIC   = 1u << 9
};

inline SensorCapability operator|(SensorCapability a, SensorCapability b)
{
    return static_cast<SensorCapability>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline SensorCapability operator&(SensorCapability a, SensorCapability b)
{
    return static_cast<SensorCapability>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool has_flag(SensorCapability caps, SensorCapability flag)
{
    return (static_cast<uint32_t>(caps) & static_cast<uint32_t>(flag)) != 0;
}

/**
 * Static description of a sensor endpoint.
 */
struct SensorInfo {
    std::string name;
    std::string description;
    SensorCapability capabilities = SensorCapability::NONE;
    double sampling_rate = 1.0;

    SensorInfo() = default;
    SensorInfo(std::string n, std::string d, SensorCapability caps, double rate)
        : name(std::move(n))
        , description(std::move(d))
        , capabilities(caps)
        , sampling_rate(rate)
    {}
};

/**
 * Raw sensory sample produced by a MultiModalSensor.
 * confidence is treated as salience in [0, 1].
 */
struct SensoryInput {
    std::string sensor_type;
    std::string modality;
    std::vector<double> data;
    double confidence = 0.5;

    SensoryInput() = default;
    SensoryInput(std::string type, std::string mod,
                 std::vector<double> values, double conf)
        : sensor_type(std::move(type))
        , modality(std::move(mod))
        , data(std::move(values))
        , confidence(conf)
    {}
};

/**
 * Abstract multi-modal sensor base class.
 *
 * Concrete sensors (hardware, mock, textual adapters) implement
 * initialize/start/stop and emit SensoryInput via registered callbacks.
 */
class MultiModalSensor {
public:
    using SensorCallback = std::function<void(const SensoryInput&)>;

    explicit MultiModalSensor(const SensorInfo& info);
    virtual ~MultiModalSensor();

    virtual bool initialize();
    virtual bool start();
    virtual bool stop();

    bool isInitialized() const { return _initialized.load(); }
    bool isActive() const { return _active.load(); }

    const SensorInfo& getSensorInfo() const { return _info; }
    bool hasCapability(SensorCapability cap) const;

    void registerCallback(SensorCallback cb);
    void clearCallbacks();

    /** JSON-like status string for diagnostics. */
    virtual std::string getStatusInfo() const;

    /** Map primary capability bit to a sensor_type string. */
    static std::string capabilityToType(SensorCapability caps);

protected:
    /** Emit a sample to all callbacks (exceptions are isolated per callback). */
    void emit(const SensoryInput& input);

    SensorInfo _info;
    std::atomic<bool> _initialized{false};
    std::atomic<bool> _active{false};

    mutable std::mutex _cb_mu;
    std::vector<SensorCallback> _callbacks;
};

/**
 * In-process mock sensor for unit tests and demos.
 * Cycles through queued test vectors when generateNextSample() is called.
 */
class MockSensor : public MultiModalSensor {
public:
    explicit MockSensor(const SensorInfo& info);

    void addTestData(const std::vector<double>& sample);
    bool generateNextSample();

private:
    mutable std::mutex _data_mu;
    std::vector<std::vector<double>> _test_data;
    size_t _next_index = 0;
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_MULTI_MODAL_SENSOR_H
