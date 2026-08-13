/*
 * agentzero-perception/src/MultiModalSensor.cpp
 *
 * Multi-modal sensor base + MockSensor implementation.
 */
#include "opencog/agentzero/MultiModalSensor.h"

#include <opencog/util/Logger.h>

#include <sstream>
#include <stdexcept>

using namespace opencog;
using namespace opencog::agentzero;

MultiModalSensor::MultiModalSensor(const SensorInfo& info)
    : _info(info)
{
}

MultiModalSensor::~MultiModalSensor()
{
    if (_active.load()) {
        stop();
    }
}

bool MultiModalSensor::initialize()
{
    _initialized.store(true);
    logger().info() << "[MultiModalSensor] Initialized sensor: " << _info.name;
    return true;
}

bool MultiModalSensor::start()
{
    if (!_initialized.load()) {
        return false;
    }
    _active.store(true);
    logger().info() << "[MultiModalSensor] Started sensor: " << _info.name;
    return true;
}

bool MultiModalSensor::stop()
{
    _active.store(false);
    logger().info() << "[MultiModalSensor] Stopped sensor: " << _info.name;
    return true;
}

bool MultiModalSensor::hasCapability(SensorCapability cap) const
{
    return has_flag(_info.capabilities, cap);
}

void MultiModalSensor::registerCallback(SensorCallback cb)
{
    std::lock_guard<std::mutex> lock(_cb_mu);
    _callbacks.push_back(std::move(cb));
}

void MultiModalSensor::clearCallbacks()
{
    std::lock_guard<std::mutex> lock(_cb_mu);
    _callbacks.clear();
}

std::string MultiModalSensor::getStatusInfo() const
{
    std::ostringstream oss;
    oss << "{\"name\":\"" << _info.name << "\""
        << ",\"is_active\":" << (_active.load() ? "true" : "false")
        << ",\"is_initialized\":" << (_initialized.load() ? "true" : "false")
        << ",\"sampling_rate\":" << _info.sampling_rate
        << "}";
    return oss.str();
}

// static
std::string MultiModalSensor::capabilityToType(SensorCapability caps)
{
    if (has_flag(caps, SensorCapability::VISUAL)) return "visual";
    if (has_flag(caps, SensorCapability::AUDITORY)) return "auditory";
    if (has_flag(caps, SensorCapability::TACTILE)) return "tactile";
    if (has_flag(caps, SensorCapability::OLFACTORY)) return "olfactory";
    if (has_flag(caps, SensorCapability::GUSTATORY)) return "gustatory";
    if (has_flag(caps, SensorCapability::PROPRIOCEPTIVE)) return "proprioceptive";
    if (has_flag(caps, SensorCapability::TEXTUAL)) return "textual";
    if (has_flag(caps, SensorCapability::NUMERIC)) return "numeric";
    if (has_flag(caps, SensorCapability::EVENT)) return "event";
    if (has_flag(caps, SensorCapability::GENERIC)) return "generic";
    return "unknown";
}

void MultiModalSensor::emit(const SensoryInput& input)
{
    std::vector<SensorCallback> copy;
    {
        std::lock_guard<std::mutex> lock(_cb_mu);
        copy = _callbacks;
    }
    for (auto& cb : copy) {
        try {
            cb(input);
        } catch (const std::exception& e) {
            logger().warn() << "[MultiModalSensor] Callback exception: " << e.what();
        } catch (...) {
            logger().warn() << "[MultiModalSensor] Callback threw unknown exception";
        }
    }
}

// ---------------------------------------------------------------------------
// MockSensor
// ---------------------------------------------------------------------------

MockSensor::MockSensor(const SensorInfo& info)
    : MultiModalSensor(info)
{
}

void MockSensor::addTestData(const std::vector<double>& sample)
{
    std::lock_guard<std::mutex> lock(_data_mu);
    _test_data.push_back(sample);
}

bool MockSensor::generateNextSample()
{
    if (!_active.load()) {
        return false;
    }

    std::vector<double> sample;
    {
        std::lock_guard<std::mutex> lock(_data_mu);
        if (!_test_data.empty()) {
            sample = _test_data[_next_index % _test_data.size()];
            ++_next_index;
        }
    }

    SensoryInput input;
    input.sensor_type = capabilityToType(_info.capabilities);
    input.modality = _info.name;
    input.data = std::move(sample);
    input.confidence = 1.0;

    emit(input);
    return true;
}
