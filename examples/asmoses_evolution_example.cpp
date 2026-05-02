/**
 * asmoses_evolution_example.cpp
 *
 * Example demonstrating ASMOSES integration for AtomSpace evolution
 * Part of Agent-Zero Learning & Adaptation Phase 5
 *
 * Copyright (C) 2024 OpenCog Foundation
 */

#include <iostream>
#include <memory>
#include <functional>

#include <opencog/util/Logger.h>
#include <opencog/atomspace/AtomSpace.h>

#include "agentzero-learning/ASMOSESIntegrator.h"

using namespace opencog;
using namespace opencog::agentzero;

int main()
{
    // Set up logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    std::cout << "=== Agent-Zero ASMOSES Evolution Example ===" << std::endl;
    
    try {
        // Create AtomSpace
        auto atomspace = std::make_shared<AtomSpace>();
        
        // Create ASMOSES integrator
        ASMOSESIntegrator integrator(atomspace, "example-agent");
        
        // Initialize the integrator
        if (!integrator.initialize()) {
            std::cerr << "Failed to initialize ASMOSES integrator" << std::endl;
            return 1;
        }
        
        std::cout << "ASMOSES integrator initialized successfully" << std::endl;
        
        // Create a simple problem atom
        Handle problem_atom = atomspace->add_node(CONCEPT_NODE, "find_optimal_program");
        std::cout << "Created problem atom: " << problem_atom->to_string() << std::endl;
        
        // Define a simple fitness function
        auto fitness_function = [](const Handle& program) -> double {
            if (program == Handle::UNDEFINED) {
                return 0.0;
            }
            
            // Simple fitness based on program name length
            // In real usage, this would evaluate program performance
            std::string name = program->get_name();
            return 1.0 / (1.0 + name.length());
        };
        
        // Evolve a program
        std::cout << "Starting program evolution..." << std::endl;
        Handle evolved_program = integrator.evolveProgram(problem_atom, fitness_function, 20);
        
        if (evolved_program != Handle::UNDEFINED) {
            std::cout << "Evolution successful!" << std::endl;
            std::cout << "Evolved program: " << evolved_program->to_string() << std::endl;
        } else {
            std::cout << "Evolution failed to find a solution" << std::endl;
        }
        
        // Demonstrate AtomSpace-based evolution
        std::cout << "\nTesting AtomSpace-based evolution..." << std::endl;
        
        Handle target_atom = atomspace->add_node(CONCEPT_NODE, "evolution_target");
        std::map<std::string, std::string> options;
        options["max_generations"] = "10";
        options["population_size"] = "50";
        
        Handle atomspace_result = integrator.evolveAtomspaceProgram(problem_atom, target_atom, options);
        
        if (atomspace_result != Handle::UNDEFINED) {
            std::cout << "AtomSpace evolution successful!" << std::endl;
            std::cout << "Result: " << atomspace_result->to_string() << std::endl;
        } else {
            std::cout << "AtomSpace evolution failed" << std::endl;
        }
        
        // Show statistics
        std::cout << "\nEvolution Statistics:" << std::endl;
        auto stats = integrator.getStatistics();
        for (const auto& stat : stats) {
            std::cout << "  " << stat.first << ": " << stat.second << std::endl;
        }
        
        // Demonstrate policy optimization
        std::cout << "\nTesting policy optimization..." << std::endl;
        
        Handle policy_atom = atomspace->add_node(CONCEPT_NODE, "test_policy");
        std::vector<Handle> experience_data = {
            atomspace->add_node(CONCEPT_NODE, "experience_1"),
            atomspace->add_node(CONCEPT_NODE, "experience_2")
        };
        
        auto reward_function = [](const Handle& policy, const std::vector<Handle>& experiences) -> double {
            return 0.8; // Simple reward
        };
        
        Handle optimized_policy = integrator.optimizePolicy(policy_atom, experience_data, reward_function);
        
        if (optimized_policy != Handle::UNDEFINED) {
            std::cout << "Policy optimization successful!" << std::endl;
            std::cout << "Optimized policy: " << optimized_policy->to_string() << std::endl;
        } else {
            std::cout << "Policy optimization failed" << std::endl;
        }
        
        // Demonstrate skill learning
        std::cout << "\nTesting skill learning..." << std::endl;
        
        std::vector<Handle> experiences = {
            atomspace->add_node(CONCEPT_NODE, "skill_experience_1"),
            atomspace->add_node(CONCEPT_NODE, "skill_experience_2")
        };
        
        auto learned_skills = integrator.learnSkills(experiences);
        std::cout << "Learned " << learned_skills.size() << " skills" << std::endl;
        
        for (size_t i = 0; i < learned_skills.size(); ++i) {
            std::cout << "  Skill " << i << ": " << learned_skills[i]->to_string() << std::endl;
        }
        
        // Final statistics
        std::cout << "\nFinal Statistics:" << std::endl;
        stats = integrator.getStatistics();
        for (const auto& stat : stats) {
            std::cout << "  " << stat.first << ": " << stat.second << std::endl;
        }
        
        std::cout << "\n=== Example completed successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error in example: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}