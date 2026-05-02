/*
 * standalone/include/cog0/MultiModalSensor.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * MultiModalSensor — unified interface for text, numeric, and event inputs.
 * Part of Phase 2: Perception & Sensory Processing.
 */

#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "CognitiveLoop.h"  // PerceptInput

namespace cog0 {

// -----------------------------------------------------------------------
// MultiModalSensor

class MultiModalSensor {
public:
    enum class Modality { TEXT, NUMERIC, EVENT };

    using SensorCallback = std::function<void(const PerceptInput&)>;

    explicit MultiModalSensor(const std::string& name,
                              Modality modality = Modality::TEXT);

    // Register a callback invoked on every new percept
    void registerCallback(SensorCallback cb);
    void clearCallbacks();

    // Inject raw data; fires all registered callbacks
    void ingest(const std::string& content, double salience = 0.5);

    // Ingest numeric data (converted to string representation)
    void ingestNumeric(double value, double salience = 0.5);

    // Ingest an event by name
    void ingestEvent(const std::string& eventName, double salience = 0.5);

    // Accessors
    const std::string& name()     const { return _name; }
    Modality           modality() const { return _modality; }
    bool               isActive() const { return _active; }
    void               setActive(bool active) { _active = active; }
    size_t             ingestCount() const { return _ingestCount.load(); }

    static std::string modalityName(Modality m);

private:
    std::string _name;
    Modality    _modality;
    bool        _active = true;

    std::atomic<size_t>       _ingestCount{0};
    std::vector<SensorCallback> _callbacks;
    mutable std::mutex          _mutex;

    void notify(const PerceptInput& p);
};

} // namespace cog0
