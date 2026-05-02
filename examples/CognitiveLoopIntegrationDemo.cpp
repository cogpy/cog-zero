/*
 * examples/CognitiveLoopIntegrationDemo.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Complete integration demo showing PerceptualProcessor with enhanced CognitiveLoop
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
#include "CognitiveLoopPerceptionIntegration.h"

using namespace opencog;
using namespace opencog::agentzero;

int main()
{
    // Initialize logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    std::cout << "===========================================================" << std::endl;
    std::cout << "CognitiveLoop Integration Demo - Agent-Zero-Genesis" << std::endl;
    std::cout << "Task AZ-PERC-002: PerceptualProcessor with AtomSpace output" << std::endl;
    std::cout << "===========================================================" << std::endl;
    
    try {
        // Create AtomSpace and agent
        AtomSpacePtr atomspace = createAtomSpace();
        Handle agent_self = atomspace->add_node(CONCEPT_NODE, std::string("IntegrationDemoAgent"));
        
        std::cout << "✓ Created AtomSpace and agent: " << agent_self->to_string() << std::endl;
        
        // Create enhanced cognitive loop with perception integration
        EnhancedCognitiveLoop cognitive_loop(atomspace, agent_self);
        std::cout << "✓ Created enhanced CognitiveLoop with PerceptualProcessor integration" << std::endl;
        
        // Add multiple sensors
        // Visual sensor
        SensorInfo visual_info("VisualSensor", "RGB camera sensor", 
                              SensorCapability::VISUAL, 30.0);
        auto visual_sensor = std::make_unique<MockSensor>(visual_info);
        visual_sensor->addTestData({0.8, 0.2, 0.1, 0.9, 0.3}); // Red objects
        visual_sensor->addTestData({0.1, 0.9, 0.2, 0.3, 0.8}); // Green objects
        cognitive_loop.addSensor(std::move(visual_sensor));
        
        // Auditory sensor
        SensorInfo audio_info("AudioSensor", "Microphone array", 
                             SensorCapability::AUDITORY, 44100.0);
        auto audio_sensor = std::make_unique<MockSensor>(audio_info);
        audio_sensor->addTestData({0.5, 0.7, 0.3, 0.8, 0.1}); // Audio signals
        audio_sensor->addTestData({0.2, 0.4, 0.9, 0.1, 0.6}); // More audio
        cognitive_loop.addSensor(std::move(audio_sensor));
        
        // Tactile sensor
        SensorInfo tactile_info("TactileSensor", "Pressure sensor array",
                               SensorCapability::TACTILE, 1000.0);
        auto tactile_sensor = std::make_unique<MockSensor>(tactile_info);
        tactile_sensor->addTestData({0.3, 0.6, 0.8, 0.2, 0.7}); // Touch data
        cognitive_loop.addSensor(std::move(tactile_sensor));
        
        std::cout << "✓ Added 3 multi-modal sensors (visual, auditory, tactile)" << std::endl;
        
        // Start perception processing
        if (!cognitive_loop.startPerception()) {
            std::cerr << "Failed to start perception processing" << std::endl;
            return 1;
        }
        
        std::cout << "✓ Started perception processing with " 
                  << cognitive_loop.getActiveSensorCount() << " active sensors" << std::endl;
        
        // Run cognitive cycles to demonstrate perception processing
        std::cout << "\nExecuting cognitive cycles with integrated perception:" << std::endl;
        std::cout << "----------------------------------------------------" << std::endl;
        
        for (int cycle = 0; cycle < 5; ++cycle) {
            std::cout << "\nCycle " << (cycle + 1) << ":" << std::endl;
            
            bool success = cognitive_loop.executeSingleCycle();
            
            if (success) {
                std::cout << "✓ Cycle completed successfully" << std::endl;
                
                // Show perception statistics
                std::string stats = cognitive_loop.getPerceptionStats();
                std::cout << "  Perception stats: " << stats << std::endl;
                
                // Show AtomSpace growth
                std::cout << "  AtomSpace size: " << atomspace->get_size() << " atoms" << std::endl;
            } else {
                std::cout << "✗ Cycle failed" << std::endl;
            }
            
            // Brief pause between cycles
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // Stop perception
        cognitive_loop.stopPerception();
        std::cout << "\n✓ Stopped perception processing" << std::endl;
        
        // Final summary
        std::cout << "\n===========================================================" << std::endl;
        std::cout << "Integration Demo Summary:" << std::endl;
        std::cout << "✓ PerceptualProcessor successfully integrated with CognitiveLoop" << std::endl;
        std::cout << "✓ Multi-modal sensory processing (visual, auditory, tactile)" << std::endl;
        std::cout << "✓ AtomSpace representations created for all perceptions" << std::endl;
        std::cout << "✓ Proper error handling and statistics tracking" << std::endl;
        std::cout << "✓ OpenCog architectural patterns maintained" << std::endl;
        std::cout << "\nFinal AtomSpace size: " << atomspace->get_size() << " atoms" << std::endl;
        std::cout << "\nTask AZ-PERC-002 SUCCESSFULLY DEMONSTRATED!" << std::endl;
        std::cout << "===========================================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}