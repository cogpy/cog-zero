/*
 * examples/PerceptualProcessorDemo.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Demo showing PerceptualProcessor integration with AtomSpace
 * Part of the AGENT-ZERO-GENESIS project - Task AZ-PERC-002
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/PerceptualProcessor.h"
#include "opencog/agentzero/MultiModalSensor.h"

using namespace opencog;
using namespace opencog::agentzero;

int main()
{
    // Initialize logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    std::cout << "PerceptualProcessor Demo - Agent-Zero-Genesis Project" << std::endl;
    std::cout << "=====================================================" << std::endl;
    
    try {
        // Create AtomSpace
        AtomSpacePtr atomspace = createAtomSpace();
        std::cout << "✓ Created AtomSpace" << std::endl;
        
        // Create agent self representation
        Handle agent_self = atomspace->add_node(CONCEPT_NODE, std::string("DemoAgent"));
        std::cout << "✓ Created agent self: " << agent_self->to_string() << std::endl;
        
        // Create PerceptualProcessor
        PerceptualProcessor processor(atomspace, agent_self);
        std::cout << "✓ Created PerceptualProcessor" << std::endl;
        
        // Set perception context
        Handle perception_context = atomspace->add_node(CONCEPT_NODE, std::string("DemoPerceptionContext"));
        processor.setPerceptionContext(perception_context);
        std::cout << "✓ Set perception context" << std::endl;
        
        // Create mock sensor
        SensorInfo visual_info("DemoVisualSensor", "Demo visual sensor", 
                              SensorCapability::VISUAL, 30.0);
        MockSensor visual_sensor(visual_info);
        
        // Initialize and start sensor
        visual_sensor.initialize();
        visual_sensor.start();
        std::cout << "✓ Started visual sensor" << std::endl;
        
        // Add test data
        visual_sensor.addTestData({0.1, 0.5, 0.8, 0.3, 0.7}); // Visual data
        visual_sensor.addTestData({0.2, 0.6, 0.9, 0.1, 0.4}); // More visual data
        
        // Register callback to process sensor data
        int processed_count = 0;
        visual_sensor.registerCallback([&processor, &processed_count](const SensoryInput& input) {
            Handle perception = processor.processInput(input);
            if (perception != Handle::UNDEFINED) {
                processed_count++;
                std::cout << "✓ Processed " << input.sensor_type << " input -> " 
                         << perception->to_string() << std::endl;
            }
        });
        
        // Generate some sensor data
        std::cout << "\nGenerating sensor data:" << std::endl;
        for (int i = 0; i < 5; ++i) {
            visual_sensor.generateNextSample();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Test batch processing
        std::cout << "\nTesting batch processing:" << std::endl;
        std::vector<SensoryInput> batch_inputs;
        batch_inputs.emplace_back("auditory", "microphone", std::vector<double>{0.3, 0.7, 0.2}, 0.9);
        batch_inputs.emplace_back("tactile", "pressure", std::vector<double>{0.8, 0.1, 0.6}, 0.7);
        batch_inputs.emplace_back("thermal", "temperature", std::vector<double>{22.5, 23.1, 22.8}, 0.8);
        
        std::vector<Handle> batch_results = processor.processBatch(batch_inputs);
        for (size_t i = 0; i < batch_results.size(); ++i) {
            if (batch_results[i] != Handle::UNDEFINED) {
                std::cout << "✓ Batch " << i << ": " << batch_inputs[i].sensor_type 
                         << " -> " << batch_results[i]->to_string() << std::endl;
            }
        }
        
        // Show processing statistics
        std::cout << "\nProcessing Statistics:" << std::endl;
        std::cout << processor.getProcessingStats() << std::endl;
        
        // Show AtomSpace contents
        std::cout << "\nAtomSpace size: " << atomspace->get_size() << " atoms" << std::endl;
        std::cout << "Health status: " << (processor.isHealthy() ? "Healthy" : "Unhealthy") << std::endl;
        
        visual_sensor.stop();
        std::cout << "\n✓ Demo completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}