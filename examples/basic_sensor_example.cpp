/*
 * basic_sensor_example.cpp
 *
 * Basic example demonstrating MultiModalSensor interface usage
 * Part of the AGENT-ZERO-GENESIS project - Phase 2 Perception
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <iostream>
#include <memory>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/MultiModalSensor.h"
#include "opencog/agentzero/PerceptualProcessor.h"
#include "opencog/agentzero/AttentionManager.h"

using namespace opencog;
using namespace opencog::agentzero;

int main()
{
    logger().setLevel(Logger::INFO);
    std::cout << "=== Agent-Zero MultiModalSensor Basic Example ===" << std::endl;

    try {
        // Create AtomSpace and agent self-atom
        AtomSpacePtr as = createAtomSpace();
        Handle agent = as->add_node(CONCEPT_NODE, "ExampleAgent");
        std::cout << "Created AtomSpace and agent" << std::endl;

        // Create PerceptualProcessor
        PerceptualProcessor processor(as, agent);
        std::cout << "Created PerceptualProcessor" << std::endl;

        // Create AttentionManager
        AttentionManager attention(as);
        std::cout << "Created AttentionManager" << std::endl;

        // Create a MockSensor with VISUAL capability
        SensorInfo info("ExampleSensor", "Example visual sensor",
                        SensorCapability::VISUAL, 10.0);
        MockSensor sensor(info);

        // Initialize and start
        sensor.initialize();
        sensor.start();
        std::cout << "Sensor started: " << sensor.getSensorInfo().name << std::endl;

        // Register callback to process each sensory input
        sensor.registerCallback([&](const SensoryInput& input) {
            // Calculate salience
            SalienceScore sal = attention.calculateSalience(input);
            std::cout << "Salience: overall=" << sal.overall
                      << " novelty=" << sal.novelty
                      << " signal=" << sal.signal_quality << std::endl;

            // Process through PerceptualProcessor
            Handle h = processor.processInput(input);
            if (h != Handle::UNDEFINED) {
                // Allocate attention to the percept
                attention.allocateAttention(h, sal.overall);
                std::cout << "Perception atom created, STI="
                          << attention.getSTI(h) << std::endl;
            }
        });

        // Add test data and generate samples
        sensor.addTestData({0.8, 0.6, 0.9, 0.7});
        for (int i = 0; i < 5; ++i) {
            sensor.generateNextSample();
        }

        // Show attention focus
        HandleSeq focus = attention.getAttentionFocus();
        std::cout << "Attention focus size: " << focus.size() << std::endl;

        // Show stats
        std::cout << "Processor stats: " << processor.getProcessingStats() << std::endl;
        std::cout << "Attention stats: " << attention.getStats() << std::endl;

        sensor.stop();
        std::cout << "\nExample completed successfully." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
