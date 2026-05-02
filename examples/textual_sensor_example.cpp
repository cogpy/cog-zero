/*
 * textual_sensor_example.cpp
 *
 * Example demonstrating TextualSensor streaming text ingestion
 * Part of the AGENT-ZERO-GENESIS project - Phase 2 Perception
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <iostream>
#include <memory>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/TextualSensor.h"
#include "opencog/agentzero/AttentionManager.h"
#include "opencog/agentzero/PerceptualProcessor.h"

using namespace opencog;
using namespace opencog::agentzero;

int main()
{
    logger().setLevel(Logger::INFO);
    std::cout << "=== Agent-Zero TextualSensor Example ===" << std::endl;

    try {
        // Setup
        AtomSpacePtr as = createAtomSpace();
        Handle agent    = as->add_node(CONCEPT_NODE, "TextAgent");

        TextualSensor sensor(as, agent, TextProcessingMode::SENTENCES);
        AttentionManager attention(as);
        std::cout << "Created TextualSensor (SENTENCES mode)" << std::endl;

        // Process various text inputs
        std::vector<std::string> inputs = {
            "The agent perceives its environment through multiple sensors.",
            "Attention is allocated based on salience scores.",
            "Novel inputs receive higher initial attention values.",
            "Repeated patterns cause attention to decay over time.",
            "The agent perceives its environment through multiple sensors." // repeated
        };

        std::cout << "\nProcessing text inputs:" << std::endl;
        for (const auto& text : inputs) {
            // Calculate salience first
            TextSalienceScore score = sensor.calculateSalience(text);
            std::cout << "  Text: \"" << text.substr(0, std::min(text.size(), size_t(50))) << "...\"\n"
                      << "  Salience: overall=" << score.overall
                      << " novelty=" << score.novelty
                      << " lexical=" << score.lexical << std::endl;

            // Process through sensor
            auto handles = sensor.processText(text);
            for (const Handle& h : handles) {
                // Convert to SensoryInput and allocate attention
                SensoryInput si = sensor.toSensoryInput(text, score);
                SalienceScore sal = attention.calculateSalience(si);
                double sti = attention.allocateAttention(h, sal.overall);
                std::cout << "  -> Atom created, STI=" << sti << std::endl;
            }
        }

        // Demonstrate processing modes
        std::cout << "\n--- Processing Modes ---" << std::endl;
        const std::string sample = "Hello world! How are you? I am fine.";

        for (TextProcessingMode mode : {
                TextProcessingMode::SENTENCES,
                TextProcessingMode::WORDS,
                TextProcessingMode::DOCUMENTS,
                TextProcessingMode::STREAM}) {
            sensor.setMode(mode);
            auto handles = sensor.processText(sample);
            const char* mode_name = "unknown";
            switch (mode) {
                case TextProcessingMode::SENTENCES: mode_name = "SENTENCES"; break;
                case TextProcessingMode::WORDS:     mode_name = "WORDS";     break;
                case TextProcessingMode::DOCUMENTS: mode_name = "DOCUMENTS"; break;
                case TextProcessingMode::STREAM:    mode_name = "STREAM";    break;
            }
            std::cout << "  " << mode_name << " mode: "
                      << handles.size() << " atom(s) created" << std::endl;
        }

        // Show stats
        std::cout << "\n--- Statistics ---" << std::endl;
        std::cout << "TextualSensor: " << sensor.getStats() << std::endl;
        std::cout << "AttentionMgr:  " << attention.getStats() << std::endl;
        std::cout << "AtomSpace size: " << as->get_size() << " atoms" << std::endl;

        // Show attention focus
        HandleSeq focus = attention.getAttentionFocus();
        std::cout << "Atoms in attention focus: " << focus.size() << std::endl;

        std::cout << "\nExample completed successfully." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
