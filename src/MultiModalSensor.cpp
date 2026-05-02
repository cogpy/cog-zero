/*
 * standalone/src/MultiModalSensor.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "cog0/MultiModalSensor.h"

#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace cog0 {

MultiModalSensor::MultiModalSensor(const std::string& name, Modality modality)
    : _name(name), _modality(modality)
{
}

void MultiModalSensor::registerCallback(SensorCallback cb)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _callbacks.push_back(std::move(cb));
}

void MultiModalSensor::clearCallbacks()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _callbacks.clear();
}

void MultiModalSensor::ingest(const std::string& content, double salience)
{
    if (!_active) return;

    PerceptInput p;
    p.source   = _name;
    p.modality = modalityName(_modality);
    p.content  = content;
    p.salience = salience;

    _ingestCount.fetch_add(1, std::memory_order_relaxed);
    notify(p);
}

void MultiModalSensor::ingestNumeric(double value, double salience)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << value;
    ingest(oss.str(), salience);
}

void MultiModalSensor::ingestEvent(const std::string& eventName, double salience)
{
    ingest(eventName, salience);
}

// static
std::string MultiModalSensor::modalityName(Modality m)
{
    switch (m) {
        case Modality::TEXT:    return "text";
        case Modality::NUMERIC: return "numeric";
        case Modality::EVENT:   return "event";
    }
    return "unknown";
}

// private
void MultiModalSensor::notify(const PerceptInput& p)
{
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto& cb : _callbacks)
        cb(p);
}

} // namespace cog0
