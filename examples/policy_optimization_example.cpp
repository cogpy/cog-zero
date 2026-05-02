/**
 * policy_optimization_example.cpp - Example of MOSES Policy Optimization
 * 
 * Part of AZ-LEARN-003: MOSES Policy Optimization Integration
 * Demonstrates basic usage of PolicyOptimizer with MOSES integration
 * 
 * Copyright (C) 2024 OpenCog Foundation
 */

#include <iostream>
#include <memory>

#include <agentzero/learning/PolicyOptimizer.h>
#include <agentzero/learning/LearningUtils.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

using namespace opencog;
using namespace opencog::agentzero::learning;

/**
 * Example fitness function for XOR problem
 */
class XORFitnessFunction : public PolicyFitnessFunction {
public:
    double evaluate(const opencog::combo::combo_tree& program,
                   const std::map<std::string, Handle>& context = {}) override {
        // XOR truth table test cases
        std::vector<std::vector<bool>> inputs = {
            {false, false}, // -> false
            {false, true},  // -> true
            {true, false},  // -> true
            {true, true}    // -> false
        };
        
        std::vector<bool> expected_outputs = {false, true, true, false};
        
        int correct = 0;
        
        try {
            // Test each input case
            for (size_t i = 0; i < inputs.size(); ++i) {
                // For simplicity, we'll score based on program complexity
                // In a real implementation, you'd evaluate the program with inputs
                // This is a placeholder implementation
                bool predicted = (program.size() % 2 == inputs[i][0] ? inputs[i][1] : !inputs[i][1]);
                
                if (predicted == expected_outputs[i]) {
                    correct++;
                }
            }
        } catch (...) {
            return 0.0; // Invalid program
        }
        
        return static_cast<double>(correct) / inputs.size();
    }
    
    std::string getName() const override {
        return "XOR_Fitness_Function";
    }
    
    std::vector<std::string> getInputFeatures() const override {
        return {"input1", "input2"};
    }
};

int main() {
    std::cout << "=== Agent-Zero MOSES Policy Optimization Example ===" << std::endl;
    
    // Configure logging
    logger().set_level(Logger::INFO);
    logger().set_print_to_stdout_flag(true);
    
    try {
        // Create AtomSpace
        auto atomspace = std::make_shared<AtomSpace>();
        std::cout << "Created AtomSpace" << std::endl;
        
        // Create fitness function
        auto fitness_function = std::make_shared<XORFitnessFunction>();
        std::cout << "Created XOR fitness function" << std::endl;
        
        // Create policy optimizer with fast configuration for demo
        LearningConfig config = utils::getDefaultConfig("fast");
        config.max_evals = 500;  // Small number for quick demo
        config.max_gens = 50;
        config.population_size = 100;
        
        auto optimizer = std::make_unique<PolicyOptimizer>(atomspace, config);
        std::cout << "Created PolicyOptimizer with fast configuration" << std::endl;
        
        // Initialize with fitness function
        if (!optimizer->initialize(fitness_function)) {
            std::cerr << "Failed to initialize PolicyOptimizer" << std::endl;
            return 1;
        }
        std::cout << "Initialized PolicyOptimizer with fitness function" << std::endl;
        
        // Evolve a policy for XOR problem
        std::cout << "\nEvolving policy for XOR problem..." << std::endl;
        auto policy = optimizer->evolvePolicy("xor_policy_v1");
        
        if (policy) {
            std::cout << "Successfully evolved policy!" << std::endl;
            std::cout << "Policy ID: " << policy->id << std::endl;
            std::cout << "Fitness Score: " << policy->fitness_score << std::endl;
            std::cout << "Program Source: " << policy->program_source << std::endl;
            std::cout << "Evaluation Count: " << policy->evaluation_count << std::endl;
            
            // Test policy evaluation
            double test_fitness = optimizer->evaluatePolicy(*policy);
            std::cout << "Test Evaluation Fitness: " << test_fitness << std::endl;
            
        } else {
            std::cout << "Policy evolution failed or returned no result" << std::endl;
            std::cout << "This can happen with very restrictive evolution parameters" << std::endl;
        }
        
        // Get optimization statistics
        auto stats = optimizer->getOptimizationStats();
        std::cout << "\nOptimization Statistics:" << std::endl;
        for (const auto& pair : stats) {
            std::cout << "  " << pair.first << ": " << pair.second << std::endl;
        }
        
        // Demonstrate policy storage and retrieval
        std::cout << "\nTesting policy storage and retrieval..." << std::endl;
        
        // Get all policies
        auto all_policies = optimizer->getAllPolicies();
        std::cout << "Total policies in system: " << all_policies.size() << std::endl;
        
        if (!all_policies.empty()) {
            // Try to retrieve the first policy by ID
            auto retrieved = optimizer->retrievePolicyFromAtomSpace(all_policies[0]->id);
            if (retrieved) {
                std::cout << "Successfully retrieved policy: " << retrieved->id << std::endl;
            } else {
                std::cout << "Failed to retrieve policy from AtomSpace" << std::endl;
            }
        }
        
        std::cout << "\n=== Example completed successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}